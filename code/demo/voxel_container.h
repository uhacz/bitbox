#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>
#include <util/memory.h>
#include <util/string_util.h>
#include "voxel_type.h"
#include "voxel_octree.h"

struct bxAABB;
struct bxGdiBuffer;

struct bxVoxel_ObjectData
{
	i32 numShellVoxels;
	i32 colorPalette;
};

struct bxVoxel_ObjectAttribute
{
	float3_t pos;
	float3_t rot;
	char* file;

	bxVoxel_ObjectAttribute()
		: pos(0.f)
		, rot(0.f)
		, file(0)
	{}

	void release(){
		string::free_and_null(&file);
	}

};

struct bxVoxel_Container
{
	struct Data
	{
		Matrix4*            worldPose;
		bxAABB*             aabb;
		//bxVoxel_Octree**    octree;
		bxVoxel_Map*        map;
		bxVoxel_ObjectData* voxelDataObj;
		bxGdiBuffer*        voxelDataGpu;
		bxVoxel_ObjectAttribute* attribute;

		bxVoxel_ObjectId*   indices;


		i32 capacity;
		i32 size;
		i32 _freeList;
		void* _memoryHandle;
	};

	Data _data;
	bxAllocator* _alloc_main;
	bxAllocator* _alloc_octree;

	bxVoxel_Container()
		: _alloc_main(0)
		, _alloc_octree(0)
	{
		memset(&_data, 0x00, sizeof(bxVoxel_Container::Data));
	}

	int objectIndex(bxVoxel_ObjectId id){
		return _data.indices[id.index].index;
	}
};

namespace bxVoxel
{
	
}