#include "design_block.h"

#include <util/debug.h>
#include <util/hash.h>
#include <util/string_util.h>
#include <util/id_array.h>
#include <util/buffer_utils.h>
#include <util/array_util.h>
#include <util/array.h>

#include <gfx/gfx_material.h>
#include <gfx/gfx.h>

namespace bxDesignBlock_Util
{
    static inline bxDesignBlock::Handle makeInvalidHandle()
    {
        bxDesignBlock::Handle h = { 0 };
        return h;
    }
    static inline bxDesignBlock::Handle makeHandle( id_t id )
    {
        bxDesignBlock::Handle h = { id.hash };
        return h;
    }
    static inline id_t makeId( bxDesignBlock::Handle h )
    {
        return make_id( h.h );
    }

    inline u32 makeNameHash( bxDesignBlock* _this, const char* name )
    {
        const u32 SEED = u32( _this ) ^ 0xDEADF00D;
        return murmur3_hash32( name, string::length( name ), SEED );
    }

    struct CreateDesc
    {
        bxDesignBlock::Handle h;
        bxGdiShaderFx_Instance* material;
    };

}///
using namespace bxDesignBlock_Util;
struct bxDesignBlock_Impl : public bxDesignBlock
{
    static const u64 DEFAULT_TAG = bxMakeTag64<'_', 'D', 'B', 'L', 'O', 'C', 'K', '_'>::tag;
    enum
    {
        eMAX_BLOCKS = 0xFFFF,
    };
    struct Data
    {
        Matrix4* pose;
        bxDesignBlock::Shape* shape;

        u32* name;
        u64* tag;

        bxGfx_HMeshInstance* gfxMeshI;
        bxPhx_HShape* phxShape;

        void* memoryHandle;
        i32 size;
        i32 capacity;
    };

    id_array_t< eMAX_BLOCKS > _idContainer;
    Data _data;

    array_t< Handle > _list_updatePose;
    array_t< CreateDesc > _list_create;
    array_t< Handle > _list_release;

    u32 _flag_releaseAll : 1;

    void startup()
    {
        memset( &_data, 0x00, sizeof( bxDesignBlock_Impl::Data ) );
        _flag_releaseAll = 0;
    }
    void shutdown()
    {
        BX_FREE0( bxDefaultAllocator(), _data.memoryHandle );
    }

    ////
    ////

    inline int dataIndex_get( Handle h )
    {
        id_t id = makeId( h );
        return (id_array::has( _idContainer, id )) ? id_array::index( _idContainer, id ) : -1;
    }

    void _AllocateData( int newcap )
    {
        if ( newcap <= _data.capacity )
            return;

        int memSize = 0;
        memSize += newcap * sizeof( *_data.pose );
        memSize += newcap * sizeof( *_data.shape );
        memSize += newcap * sizeof( *_data.name );
        memSize += newcap * sizeof( *_data.tag );
        memSize += newcap * sizeof( *_data.gfxMeshI );
        memSize += newcap * sizeof( *_data.phxShape );


        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );

        Data newdata;
        newdata.memoryHandle = mem;
        newdata.size = _data.size;
        newdata.capacity = newcap;

        bxBufferChunker chunker( mem, memSize );
        newdata.pose = chunker.add< Matrix4 >( newcap );
        newdata.shape = chunker.add< bxDesignBlock::Shape >( newcap );
        newdata.name = chunker.add< u32 >( newcap );
        newdata.tag = chunker.add< u64 >( newcap );
        newdata.gfxMeshI = chunker.add< bxGfx_HMeshInstance >( newcap );
        newdata.phxShape = chunker.add< bxPhx_HShape >( newcap );

        chunker.check();

        if ( _data.size )
        {
            BX_CONTAINER_COPY_DATA( &newdata, &_data, pose );
            BX_CONTAINER_COPY_DATA( &newdata, &_data, shape );
            BX_CONTAINER_COPY_DATA( &newdata, &_data, name );
            BX_CONTAINER_COPY_DATA( &newdata, &_data, tag );
            BX_CONTAINER_COPY_DATA( &newdata, &_data, gfxMeshI );
            BX_CONTAINER_COPY_DATA( &newdata, &_data, phxShape );

        }

        BX_FREE0( bxDefaultAllocator(), _data.memoryHandle );
        _data = newdata;
    }



    int findByHash( u32 hash )
    {
        return array::find1( _data.name, _data.name + _data.size, array::OpEqual<u32>( hash ) );
    }

    virtual Handle create( const char* name, const Matrix4& pose, const Shape& shape, const char* material )
    {
        const u32 nameHash = makeNameHash( this, name );
        int index = findByHash( nameHash );
        if ( index != -1 )
        {
            bxLogError( "Block with given name '%s' already exists at index: %d!", name, index );
            return makeInvalidHandle();
        }

        SYS_ASSERT( id_array::size( _idContainer ) == _data.size );

        id_t id = id_array::create( _idContainer );
        index = id_array::index( _idContainer, id );
        while ( index >= _data.capacity )
        {
            _AllocateData( _data.capacity * 2 + 8 );
        }

        SYS_ASSERT( index == _data.size );
        ++_data.size;

        _data.pose[index] = pose;
        _data.shape[index] = shape;
        _data.name[index] = nameHash;
        _data.tag[index] = DEFAULT_TAG;
        _data.gfxMeshI[index] = makeInvalidHandle< bxGfx_HMeshInstance >();
        _data.phxShape[index] = makeInvalidHandle< bxPhx_HShape >();

        Handle handle = makeHandle( id );

        CreateDesc createDesc;
        createDesc.h = handle;
        createDesc.material = bxGfxMaterialManager::findMaterial( material );
        if ( !createDesc.material )
        {
            createDesc.material = bxGfxMaterialManager::findMaterial( "red" );
        }
        array::push_back( _list_create, createDesc );

        return handle;

    }
    virtual void release( Handle* h )
    {
        id_t id = makeId( h[0] );
        if ( !id_array::has( _idContainer, id ) )
            return;

        array::push_back( _list_release, h[0] );

        //int thisIndex = id_array::index( _idContainer, id );
        //int lastIndex = id_array::size( _idContainer ) - 1;
        //SYS_ASSERT( lastIndex == (_data.size - 1) );

        //id_array::destroy( _idContainer, id );

        //_data.name[thisIndex]     = _data.name[lastIndex];
        //_data.tag[thisIndex]      = _data.tag[lastIndex];
        //_data.gfxMeshI[thisIndex]     = _data.gfxMeshI[lastIndex];
        //_data.phxShape[thisIndex]   = _data.phxShape[lastIndex];

        h[0] = makeInvalidHandle();
    }

    void cleanUp()
    {
        _flag_releaseAll = 1;
    }

    void bxDesignBlock::manageResources( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxPhx_CollisionSpace* cs, bxGfx_World* gfxWorld )
    {
        if ( _flag_releaseAll )
        {
            _flag_releaseAll = 0;

            for ( int i = 0; i < _data.size; ++i )
            {
                bxPhx::shape_release( cs, &_data.phxShape[i] );
                bxGfx::worldMeshRemoveAndRelease( &_data.gfxMeshI[i] );
            }
            id_array::destroyAll( _idContainer );
            array::clear( _list_updatePose );
            array::clear( _list_create );
            array::clear( _list_release );
        }

        ////
        for ( int ihandle = 0; ihandle < array::size( _list_create ); ++ihandle )
        {
            const CreateDesc& createDesc = _list_create[ihandle];
            Handle h = createDesc.h;
            int index = dataIndex_get( h );
            if ( index == -1 )
                continue;

            SYS_ASSERT( _data.phxShape[index].h == 0 );
            SYS_ASSERT( _data.gfxMeshI[index].h == 0 );

            const bxDesignBlock::Shape& shape = _data.shape[index];
            const Matrix4& pose = _data.pose[index];
            Vector3 scale( 1.f );
            bxPhx_HShape phxShape = makeInvalidHandle< bxPhx_HShape >();
            bxGdiRenderSource* rsource = 0;
            bxGdiShaderFx_Instance* fxI = createDesc.material;
            switch ( shape.type )
            {
            case Shape::eSPHERE:
                {
                    phxShape = bxPhx::shape_createSphere( cs, Vector4( pose.getTranslation(), shape.radius ) );
                    scale = Vector3( shape.radius * 2.f );
                    rsource = bxGfx::globalResources()->mesh.sphere;
                }break;
            case Shape::eCAPSULE:
                {
                    SYS_NOT_IMPLEMENTED;
                }break;
            case Shape::eBOX:
                {
                    phxShape = bxPhx::shape_createBox( cs, pose.getTranslation(), Quat( pose.getUpper3x3() ), Vector3( shape.vec4 ) );
                    scale = Vector3( shape.vec4 ) * 2.f;
                    rsource = bxGfx::globalResources()->mesh.box;
                }break;
            }

            bxGfx_HMesh hmesh = bxGfx::meshCreate();
            bxGfx::meshStreamsSet( hmesh, dev, rsource );
            bxGfx::meshShaderSet( hmesh, dev, resourceManager, fxI );

            bxGfx_HMeshInstance gfxMeshI = bxGfx::worldMeshAdd( gfxWorld, hmesh, 1 );
            
            bxGfx_HInstanceBuffer hibuff = bxGfx::meshInstanceHInstanceBuffer( gfxMeshI );
            const Matrix4 poseWithScale = appendScale( pose, scale );
            bxGfx::instanceBufferDataSet( hibuff, &poseWithScale, 1, 0 );

            _data.phxShape[index] = phxShape;
            _data.gfxMeshI[index] = gfxMeshI;
        }
        array::clear( _list_create );

        ////
        for ( int ihandle = 0; ihandle < array::size( _list_release ); ++ihandle )
        {
            id_t id = makeId( _list_release[ihandle] );
            if ( !id_array::has( _idContainer, id ) )
                return;

            int thisIndex = id_array::index( _idContainer, id );
            int lastIndex = id_array::size( _idContainer ) - 1;
            SYS_ASSERT( lastIndex == (_data.size - 1) );

            id_array::destroy( _idContainer, id );

            bxGfx::worldMeshRemoveAndRelease( &_data.gfxMeshI[thisIndex] );
            bxPhx::shape_release( cs, &_data.phxShape[thisIndex] );

            _data.pose[thisIndex] = _data.pose[lastIndex];
            _data.shape[thisIndex] = _data.shape[lastIndex];
            _data.name[thisIndex] = _data.name[lastIndex];
            _data.tag[thisIndex] = _data.tag[lastIndex];
            _data.gfxMeshI[thisIndex] = _data.gfxMeshI[lastIndex];
            _data.phxShape[thisIndex] = _data.phxShape[lastIndex];
        }
        array::clear( _list_release );
    }


    virtual void assignTag( Handle h, u64 tag )
    {
        int dataIndex = dataIndex_get( h );
        if ( dataIndex == -1 )
            return;

        _data.tag[dataIndex] = tag;
    }
    //virtual void assignMesh( Handle h, bxGfx_HMeshInstance hmeshi )
    //{
    //    int dataIndex = dataIndex_get( h );
    //    if( dataIndex == -1 )
    //        return;

    //    _data.meshi[dataIndex] = hmeshi;

    //    array::push_back( _list_updateMeshInstance, h );

    //}
    //virtual void assignCollisionShape( Handle h, bxPhx_HShape hshape )
    //{
    //    int dataIndex = dataIndex_get( h );
    //    if( dataIndex == -1 )
    //        return;

    //    _data.cshape[dataIndex] = hshape;
    //    
    //    array::push_back( _list_updateMeshInstance, h );
    //}

    virtual void tick( bxPhx_CollisionSpace* cs )
    {
        {
            for ( int i = 0; i < array::size( _list_updatePose ); ++i )
            {
                int dataIndex = dataIndex_get( _list_updatePose[i] );
                if ( dataIndex == -1 )
                    continue;

                bxGfx_HMesh hmesh = bxGfx::meshInstanceHMesh( _data.gfxMeshI[dataIndex] );
                bxGfx_HInstanceBuffer hibuffer = bxGfx::meshInstanceHInstanceBuffer( _data.gfxMeshI[dataIndex] );

                const Matrix4 pose = bxPhx::shape_pose( cs, _data.phxShape[dataIndex] );
                bxGfx::instanceBufferDataSet( hibuffer, &pose, 1, 0 );
            }
            array::clear( _list_updatePose );
        }
    }

    ////
    ////
};

bxDesignBlock* bxDesignBlock_new()
{
    bxDesignBlock_Impl* impl = BX_NEW( bxDefaultAllocator(), bxDesignBlock_Impl );
    impl->startup();
    return impl;
}

void bxDesignBlock_delete( bxDesignBlock** dblock )
{
    bxDesignBlock_Impl* impl = (bxDesignBlock_Impl*)dblock[0];
    impl->shutdown();
    BX_DELETE0( bxDefaultAllocator(), impl );

    dblock[0] = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bxDesignBlock_SceneScriptCallback::bxDesignBlock_SceneScriptCallback()
    : dblock( 0 )
{
    memset( &desc.name, 0x00, sizeof( desc.name ) );
    memset( &desc.material, 0x00, sizeof( desc.material ) );
    memset( &desc.shape, 0x00, sizeof( bxDesignBlock::Shape ) );
    desc.pose = Matrix4::identity();
}

void bxDesignBlock_SceneScriptCallback::onCreate( const char* typeName, const char* objectName )
{
    const size_t bufferSize = sizeof( desc.name ) - 1;
    sprintf_s( desc.name, bufferSize, "%s", objectName );
}

void bxDesignBlock_SceneScriptCallback::onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData )
{
    if ( string::equal( attrName, "pos" ) )
    {
        if ( attribData.dataSizeInBytes() != 12 )
            goto label_besignBlockAttributeInvalidSize;

        desc.pose.setTranslation( Vector3( attribData.fnumber[0], attribData.fnumber[1], attribData.fnumber[2] ) );
    }
    else if ( string::equal( attrName, "rot" ) )
    {
        if ( attribData.dataSizeInBytes() != 12 )
            goto label_besignBlockAttributeInvalidSize;

        const Vector3 eulerXYZ( attribData.fnumber[0], attribData.fnumber[1], attribData.fnumber[2] );
        desc.pose.setUpper3x3( Matrix3::rotationZYX( eulerXYZ ) );
    }
    else if ( string::equal( attrName, "material" ) )
    {
        if ( attribData.dataSizeInBytes() <= 0 )
            goto label_besignBlockAttributeInvalidSize;

        const size_t bufferSize = sizeof( desc.material ) - 1;
        sprintf_s( desc.material, bufferSize, "%s", attribData.string );
    }
    else if ( string::equal( attrName, "shape" ) )
    {
        if ( attribData.dataSizeInBytes() < 4 )
            goto label_besignBlockAttributeInvalidSize;

        if ( attribData.dataSizeInBytes() == 4 )
        {
            desc.shape = bxDesignBlock::Shape( attribData.fnumber[0] );
        }
        else if ( attribData.dataSizeInBytes() == 8 )
        {
            desc.shape = bxDesignBlock::Shape( attribData.fnumber[0], attribData.fnumber[1] );
        }
        else if ( attribData.dataSizeInBytes() == 12 )
        {
            desc.shape = bxDesignBlock::Shape( attribData.fnumber[0], attribData.fnumber[1], attribData.fnumber[2] );
        }
        else
        {
            goto label_besignBlockAttributeInvalidSize;
        }
    }

    return;

label_besignBlockAttributeInvalidSize:
    bxLogError( "invalid data size" );
}

void bxDesignBlock_SceneScriptCallback::onCommand( const char* cmdName, const bxAsciiScript_AttribData& args )
{
    (void)args;
    if ( string::equal( cmdName, "dblock_commit" ) )
    {
        int descValid = 1;
        descValid &= string::length( desc.name ) > 0;
        descValid &= string::length( desc.material ) > 0;
        descValid &= (desc.shape.type != UINT32_MAX);

        if ( !descValid )
        {
            bxLogError( "Design block desc invalid!" );
        }
        else
        {
            dblock->create( desc.name, desc.pose, desc.shape, desc.material );
        }
    }
}