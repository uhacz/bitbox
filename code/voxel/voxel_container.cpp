#include "voxel_container.h"
#include "voxel.h"
#include "voxel_file.h"

#include <util/pool_allocator.h>
#include <util/debug.h>
#include <util/bbox.h>
#include <util/buffer_utils.h>

#include <gdi/gdi_backend.h>

#include <string.h>

namespace
{
	void _Container_startup(bxVoxel_Container* cnt)
	{
		memset(&cnt->_data, 0x00, sizeof(bxVoxel_Container::Data));
		cnt->_data._freeList = -1;

		cnt->_alloc_main = bxDefaultAllocator();
		bxDynamicPoolAllocator* pool = BX_NEW(bxDefaultAllocator(), bxDynamicPoolAllocator);
		pool->startup(sizeof(bxVoxel_Octree), 64, bxDefaultAllocator());
		cnt->_alloc_octree = pool;
	}
	void _Container_shutdown(bxVoxel_Container* cnt)
	{
		SYS_ASSERT(cnt->_data.size == 0);

		BX_FREE0(cnt->_alloc_main, cnt->_data._memoryHandle);

		bxDynamicPoolAllocator* pool = (bxDynamicPoolAllocator*)cnt->_alloc_octree;
		pool->shutdown();
		cnt->_alloc_main = 0;
	}

	bxGdiBuffer _Gfx_createVoxelDataBuffer(bxGdiDeviceBackend* dev, int maxVoxels)
	{
		//const int maxVoxels = gridSize*gridSize*gridSize;
		const bxGdiFormat format(bxGdi::eTYPE_UINT, 1);
		bxGdiBuffer buff = dev->createBuffer(maxVoxels, format, bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ);
		return buff;
	}
	void _Gpu_uploadShell(bxGdiDeviceBackend* dev, bxVoxel_Container* cnt, int dataIndex)
	{
		bxVoxel_Container::Data& data = cnt->_data;

		bxVoxel_Map* map = &data.map[dataIndex];
		const int vxDataCapacity = (int)map->size;
		if (vxDataCapacity == 0)
		{
			bxLogWarning("Voxel map is empty. There is nothing to upload to GPU!!");
			return;
		}

		bxVoxel_GpuData* tmp = (bxVoxel_GpuData*)BX_MALLOC(bxDefaultAllocator(), vxDataCapacity * sizeof(bxVoxel_GpuData), 8);

		const int numShellVoxels = bxVoxel::map_getShell(tmp, vxDataCapacity, map[0], &data.aabb[dataIndex] );
		const int requiredMemSize = numShellVoxels * sizeof(bxVoxel_GpuData);

		bxGdiBuffer buff = cnt->_data.voxelDataGpu[dataIndex];
		if (buff.id == 0 || buff.sizeInBytes != requiredMemSize)
		{
			dev->releaseBuffer(&buff);
			cnt->_data.voxelDataGpu[dataIndex] = _Gfx_createVoxelDataBuffer(dev, numShellVoxels);
			buff = cnt->_data.voxelDataGpu[dataIndex];
		}

		bxGdiContextBackend* ctx = dev->ctx;

		bxVoxel_GpuData* vxData = (bxVoxel_GpuData*)bxGdi::buffer_map(ctx, buff, 0, numShellVoxels);
		memcpy(vxData, tmp, numShellVoxels * sizeof(bxVoxel_GpuData));
		ctx->unmap(buff.rs);

		BX_FREE0(bxDefaultAllocator(), tmp);

		bxVoxel_ObjectData& objData = cnt->_data.voxelDataObj[dataIndex];
		objData.numShellVoxels = numShellVoxels;
		//objData.gridSize = octree->nodes[0].size;
	}
}///

bxVoxel_Container::bxVoxel_Container()
    : _alloc_main( 0 )
    , _alloc_octree( 0 )
{
    memset( &_data, 0x00, sizeof( bxVoxel_Container::Data ) );
}

namespace bxVoxel
{
	bxVoxel_Container* container_new()
	{
		bxVoxel_Container* cnt = BX_NEW(bxDefaultAllocator(), bxVoxel_Container);
		_Container_startup(cnt);
		return cnt;
	}


	void container_load(bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxVoxel_Container* cnt)
	{
		bxVoxel_Container::Data& data = cnt->_data;
		const int n = data.size;
		for (int i = 0; i < n; ++i)
		{
			const bxVoxel_ObjectAttribute& attribs = data.attribute[i];

			const Vector3 rotation(xyz_to_m128(attribs.rot.xyz));
			const Vector3 position(xyz_to_m128(attribs.pos.xyz));

			const Matrix4 pose = Matrix4(Matrix3::rotationZYX(rotation), position);
			data.worldPose[i] = pose;

			if (attribs.file)
			{
				if (string::find(attribs.file, ".vox"))
				{
					map_loadMagicaVox(dev, resourceManager, &data.map[i], attribs.file);
				}
				else if (string::find(attribs.file, ".raw"))
				{
					map_loadHeightmapRaw8(resourceManager, &data.map[i], attribs.file);
				}
				else
				{
					bxLogError("Unsupported voxel file '%s'", attribs.file);
				}
			}
			_Gpu_uploadShell(dev, cnt, i);
		}
	}

	namespace
	{
		void object_releaseData(bxGdiDeviceBackend* dev, bxVoxel_Container::Data& data, int dataIndex)
		{
			dev->releaseBuffer(&data.voxelDataGpu[dataIndex]);
			data.map[dataIndex].~bxVoxel_Map();
			data.attribute[dataIndex].release();
            string::free_and_null( &data.name[dataIndex] );
		}
	}///
	void container_unload(bxGdiDeviceBackend* dev, bxVoxel_Container* cnt)
	{
		bxVoxel_Container::Data& data = cnt->_data;
		const int n = data.size;
		for (int i = 0; i < n; ++i)
		{
			object_releaseData(dev, data, i);
			data.indices[i].generation += 1;
		}

		data.size = 0;
	}

	void container_delete(bxVoxel_Container** cnt)
	{
		_Container_shutdown(cnt[0]);
		BX_DELETE0(bxDefaultAllocator(), cnt[0]);
	}

	namespace
	{
		void _Container_allocateData(bxVoxel_Container* cnt, int newCapacity)
		{
			if (newCapacity <= cnt->_data.capacity)
				return;

			bxVoxel_Container::Data& data = cnt->_data;

			int memSize = 0;
            memSize += newCapacity * sizeof( *data.name );
			memSize += newCapacity * sizeof(*data.worldPose);
			memSize += newCapacity * sizeof(*data.aabb);
			//memSize += newCapacity * sizeof( *data.octree );
			memSize += newCapacity * sizeof(*data.map);
			memSize += newCapacity * sizeof(*data.voxelDataObj);
			memSize += newCapacity * sizeof(*data.voxelDataGpu);
			memSize += newCapacity * sizeof(*data.attribute);
			memSize += newCapacity * sizeof(*data.indices);

			void* mem = BX_MALLOC(cnt->_alloc_main, memSize, 16);
			memset(mem, 0x00, memSize);

			bxVoxel_Container::Data newData;
			memset(&newData, 0x00, sizeof(bxVoxel_Container::Data));

			bxBufferChunker chunker(mem, memSize);
            newData.name = chunker.add<char*>( newCapacity );
			newData.worldPose = chunker.add<Matrix4>(newCapacity);
			newData.aabb = chunker.add<bxAABB>(newCapacity);
			//newData.octree       = chunker.add<bxVoxel_Octree*>( newCapacity );
			newData.map = chunker.add<bxVoxel_Map>(newCapacity);
			newData.voxelDataObj = chunker.add<bxVoxel_ObjectData>(newCapacity);
			newData.voxelDataGpu = chunker.add<bxGdiBuffer>(newCapacity);
			newData.attribute = chunker.add<bxVoxel_ObjectAttribute>(newCapacity);
			newData.indices = chunker.add<bxVoxel_ObjectId>(newCapacity);
			chunker.check();
			newData._memoryHandle = mem;
			newData.size = data.size;
			newData.capacity = newCapacity;
			newData._freeList = data._freeList;

			if (data._memoryHandle)
			{
                memcpy( newData.name, data.name, data.size * sizeof( *data.name ) );
                memcpy( newData.worldPose, data.worldPose, data.size * sizeof(*data.worldPose));
				memcpy( newData.aabb, data.aabb, data.size * sizeof(*data.aabb));
				//memcpy( newData.octree, data.octree, data.size * sizeof( *data.octree ) );
				memcpy( newData.map, data.map, data.size * sizeof(*data.map));
				memcpy( newData.voxelDataObj, data.voxelDataObj, data.size * sizeof(*data.voxelDataObj));
				memcpy( newData.voxelDataGpu, data.voxelDataGpu, data.size * sizeof(*data.voxelDataGpu));
				memcpy( newData.attribute, data.attribute, data.size * sizeof(*data.attribute));
				memcpy( newData.indices, data.indices, data.size * sizeof(*data.indices));

				BX_FREE0(cnt->_alloc_main, data._memoryHandle);
			}

			cnt->_data = newData;
		}
	}///



	bxVoxel_ObjectId object_new(bxVoxel_Container* cnt, const char* name )
	{
		if (cnt->_data.size + 1 > cnt->_data.capacity)
		{
			const int newCapacity = cnt->_data.capacity + 64;
			_Container_allocateData(cnt, newCapacity);
		}
		bxVoxel_Container::Data& data = cnt->_data;
		const int dataIndex = data.size;

        SYS_ASSERT( data.name[dataIndex] == 0 );
        data.name[dataIndex] = string::duplicate( NULL, name );
		data.worldPose[dataIndex] = Matrix4::identity();
		data.aabb[dataIndex] = bxAABB(Vector3(-1.f), Vector3(1.f));
		//data.octree[dataIndex] = 0; // octree_new( gridSize, menago->_alloc_octree );
		data.voxelDataGpu[dataIndex] = bxGdiBuffer();// _Gfx_createVoxelDataBuffer( dev, gridSize );
		data.voxelDataObj[dataIndex].colorPalette = 0;
		data.voxelDataObj[dataIndex].numShellVoxels = 0;
		new(&data.map[dataIndex]) hashmap_t();

		int iobjIndex = -1;
		if (data._freeList == -1)
		{
			iobjIndex = dataIndex;
			data.indices[iobjIndex].generation = 1;
		}
		else
		{
			iobjIndex = data._freeList;
			data._freeList = data.indices[iobjIndex].index;
		}
		++data.size;

		bxVoxel_ObjectId& objIndex = data.indices[iobjIndex];
		objIndex.index = dataIndex;

		bxVoxel_ObjectId id;
		id.index = iobjIndex;
		id.generation = objIndex.generation;

		return id;
	}

    bxVoxel_ObjectId object_find( const bxVoxel_Container* container, const char* name )
    {
        bxVoxel_ObjectId id = { 0 };
        
        const bxVoxel_Container::Data& data = container->_data;
        int dataIndex = 0;
        for ( ; dataIndex < data.size; ++dataIndex )
        {
            if( string::equal( data.name[dataIndex], name ) )
            {
                break;
            }
        }

        for( int iobj = 0; iobj < data.size; ++iobj )
        {
            if( dataIndex == data.indices[iobj].index )
            {
                id.index = iobj;
                id.generation = data.indices[iobj].generation;
                break;
            }
        }

        return id;
    }
    
    void object_delete(bxGdiDeviceBackend* dev, bxVoxel_Container* cnt, bxVoxel_ObjectId* id)
	{
		bxVoxel_Container::Data& data = cnt->_data;
		if (data.indices[id->index].generation != id->generation)
		{
			return;
		}

		const int handleIndex = id->index;
		const int dataIndex = data.indices[handleIndex].index;

		const int lastDataIndex = data.size - 1;
		SYS_ASSERT(lastDataIndex >= 0);

		int lastHandleIndex = -1;
		for (int i = 0; i < data.size; ++i)
		{
			if (data.indices[i].index == lastDataIndex)
			{
				lastHandleIndex = i;
				break;
			}
		}

		//BX_DELETE0( menago->_alloc_octree, data.octree[dataIndex] );
		object_releaseData(dev, data, dataIndex);

		data.indices[lastHandleIndex].index = dataIndex;
		data.indices[handleIndex].index = data._freeList;
		data._freeList = handleIndex;
		data.indices[handleIndex].generation += 1;
		data.worldPose[dataIndex] = data.worldPose[lastDataIndex];
		data.aabb[dataIndex] = data.aabb[lastDataIndex];
		//data.octree[dataIndex] = data.octree[lastDataIndex];
		data.voxelDataGpu[dataIndex] = data.voxelDataGpu[lastDataIndex];
		data.voxelDataObj[dataIndex] = data.voxelDataObj[lastDataIndex];
		data.attribute[dataIndex] = data.attribute[lastDataIndex];

		--data.size;
		id->index = 0;
		id->generation = 0;
	}

	bool object_valid(bxVoxel_Container* cnt, bxVoxel_ObjectId id)
	{
		return (id.index < (u32)cnt->_data.size) && (cnt->_data.indices[id.index].generation == id.generation);
	}
	int object_setAttribute(bxVoxel_Container* cnt, bxVoxel_ObjectId id, const char* attrName, const void* attrData, unsigned attrDataSize)
	{
		if (!object_valid(cnt, id))
			return -1;

		bxVoxel_ObjectAttribute& attribs = cnt->_data.attribute[cnt->_data.indices[id.index].index];
		if (string::equal(attrName, "pos"))
		{
			if (attrDataSize < 12)
				goto object_setAttribute_sizeError;

			memcpy(attribs.pos.xyz, attrData, 12);
		}
		else if (string::equal(attrName, "rot"))
		{
			if (attrDataSize < 12)
				goto object_setAttribute_sizeError;

			memcpy(attribs.rot.xyz, attrData, 12);
		}
		else if (string::equal(attrName, "file"))
		{
			if (attrDataSize == 0)
				goto object_setAttribute_sizeError;

			attribs.file = string::duplicate(attribs.file, (const char*)attrData);
		}

		return 0;

	object_setAttribute_sizeError:
		bxLogError("Attribute '%s' has wrong data size (%u)", attrName, attrDataSize);
		return -1;

	}
	//bxVoxel_Octree* object_octree( bxVoxel_Manager* menago, bxVoxel_ObjectId id )
	//{
	//    return menago->_data.octree[ menago->_data.indices[id.index].index ];
	//}

	bxVoxel_Map* object_map(bxVoxel_Container* cnt, bxVoxel_ObjectId id)
	{
		return &cnt->_data.map[cnt->objectIndex(id)];
	}

	const bxAABB& object_aabb(bxVoxel_Container* cnt, bxVoxel_ObjectId id)
	{
		return cnt->_data.aabb[cnt->objectIndex(id)];
	}

	const Matrix4& object_pose(bxVoxel_Container* cnt, bxVoxel_ObjectId id)
	{
		return cnt->_data.worldPose[cnt->objectIndex(id)];
	}

	void object_setPose(bxVoxel_Container* cnt, bxVoxel_ObjectId id, const Matrix4& pose)
	{
		cnt->_data.worldPose[cnt->objectIndex(id)] = pose;
	}

	void object_setAABB(bxVoxel_Container* cnt, bxVoxel_ObjectId id, const bxAABB& aabb)
	{
		cnt->_data.aabb[cnt->objectIndex(id)] = aabb;
	}

	void gpu_uploadShell(bxGdiDeviceBackend* dev, bxVoxel_Container* cnt, bxVoxel_ObjectId id)
	{
		SYS_ASSERT(object_valid(cnt, id));
		const int dataIndex = cnt->objectIndex(id);
		_Gpu_uploadShell(dev, cnt, dataIndex);
	}
}///
