#include "scene.h"
#include <engine/engine.h>

//#include <util/buffer_utils.h>

//bxScene_Graph::bxScene_Graph( int allocationChunkSize /*= 16*/, bxAllocator* alloc /*= bxDefaultAllocator() */ )
//    : _alloc( alloc )
//    , _alloc_chunkSize( allocationChunkSize )
//    , _flag_recompute(0)
//{
//    memset( &_data, 0x00, sizeof(_data) );
//    _data._freeList = -1;
//}
//
//void bxScene_Graph::_Allocate( int newCapacity )
//{
//    if( newCapacity <= _data.capacity )
//        return;
//
//    int memSize = 0;
//    memSize += newCapacity * sizeof( *_data.localPose );
//    memSize += newCapacity * sizeof( *_data.worldPose );
//    memSize += newCapacity * sizeof( *_data.parent );
//    memSize += newCapacity * sizeof( *_data.flag );
//
//    void* mem = BX_MALLOC( _alloc, memSize, 8 );
//    memset( mem, 0x00, memSize );
//
//    Data newData;
//    memset( &newData, 0x00, sizeof(Data) );
//    
//    bxBufferChunker chunker( mem, memSize );
//    newData.localPose     = chunker.add<Matrix4>( newCapacity );
//    newData.worldPose     = chunker.add<Matrix4>( newCapacity );
//    newData.parent        = chunker.add<i16>( newCapacity );
//    newData.flag          = chunker.add<u8>( newCapacity );
//    newData.capacity = newCapacity;
//    newData.size = _data.size;
//    newData._freeList = _data._freeList;
//    newData._memoryHandle = mem;
//
//    chunker.check();
//
//    if( _data._memoryHandle )
//    {
//        memcpy( newData.localPose     , _data.localPose     , _data.size * sizeof(*_data.localPose     ) );
//        memcpy( newData.worldPose     , _data.worldPose     , _data.size * sizeof(*_data.worldPose     ) );
//        memcpy( newData.parent        , _data.parent        , _data.size * sizeof(*_data.parent        ) );
//        memcpy( newData.flag          , _data.flag          , _data.size * sizeof(*_data.flag          ) );
//        
//        BX_FREE0( _alloc, _data._memoryHandle );
//    }
//    _data = newData;
//}
//
//
//namespace
//{
//    inline bxScene_Graph::Id makeNodeId( int i )
//    {
//        bxScene_Graph::Id id = { i };
//        return id;
//    }
//    inline i16 nodeId_indexSafe( bxScene_Graph::Id nodeId, i32 numNodesInContainer )
//    {
//        const i16 index = nodeId.index;
//        SYS_ASSERT( index >= 0 && index < numNodesInContainer );
//        return index;
//    }
//}///
//
//
//bxScene_Graph::Id bxScene_Graph::create()
//{
//    int index = -1;
//
//    if( _data.size + 1 > _data.capacity )
//    {
//        const int newCapacity = _data.size + 64;
//        _Allocate( newCapacity );
//    }
//
//    if( _data._freeList == -1 )
//    {
//        index = _data.size;
//    }
//    else
//    {
//        SYS_ASSERT( _data._freeList < _data.size );
//        index = _data._freeList;
//        _data._freeList = _data.parent[ index ];
//        SYS_ASSERT( _data._freeList == -1 || _data._freeList < _data.size );
//    }
//
//    SYS_ASSERT( _data.flag[index] == 0 );
//    _data.localPose[index] = Matrix4::identity();
//    _data.worldPose[index] = Matrix4::identity();
//    _data.parent[index] = -1;
//    SYS_ASSERT( _data.flag[index] == 0 );
//    _data.flag[index] = eFLAG_ACTIVE;
//    ++_data.size;
//    _flag_recompute = 1;
//    return makeNodeId( index );
//}
//
//void bxScene_Graph::release( bxScene_Graph::Id* id )
//{
//    int index = id->index;
//    if ( index < 0 || index > _data.size )
//        return;
//
//    if ( !(_data.flag[index] & eFLAG_ACTIVE) )
//        return;
//
//    SYS_ASSERT( _data.size > 0 );
//
//    const i16 parentIndex = _data.parent[index];
//    unlink( id[0] );
//
//    _data.flag[index] = 0;
//    _data.parent[index] = _data._freeList;
//    _data._freeList = index;
//    
//    --_data.size;
//    _flag_recompute = 1;
//}
//
//void bxScene_Graph::link( bxScene_Graph::Id parent, bxScene_Graph::Id child )
//{
//    const int parentIndex = parent.index;
//    const int childIndex = child.index;
//
//    if ( parentIndex < 0 || parentIndex > _data.size )
//        return;
//
//    if ( childIndex < 0 || childIndex > _data.size )
//        return;
//
//    unlink( child );
//    _data.parent[childIndex] = parentIndex;
//}
//
//void bxScene_Graph::unlink( bxScene_Graph::Id child )
//{
//    const int childIndex = child.index;
//    if ( childIndex < 0 || childIndex > _data.size )
//        return;
//
//    _data.parent[childIndex] = -1;
//}
//
//bxScene_Graph::Id bxScene_Graph::parent( bxScene_Graph::Id nodeId )
//{
//    const int index = nodeId_indexSafe( nodeId, _data.size );
//    return makeNodeId( _data.parent[ index ] );
//}
//
//const Matrix4& bxScene_Graph::localPose( bxScene_Graph::Id nodeId ) const 
//{
//    const int index = nodeId_indexSafe( nodeId, _data.size );
//    return _data.localPose[index];
//}
//
//const Matrix4& bxScene_Graph::worldPose( bxScene_Graph::Id nodeId ) const 
//{
//    const int index = nodeId_indexSafe( nodeId, _data.size );
//    return _data.worldPose[index];
//}
//
//
//void bxScene_Graph::setLocalRotation( bxScene_Graph::Id nodeId, const Matrix3& rot )
//{
//    const int index = nodeId_indexSafe( nodeId, _data.size );
//    _data.localPose[index].setUpper3x3( rot );
//
//    const int parentIndex = _data.parent[index];
//    Matrix4 parentPose = (parentIndex != -1) ? _data.worldPose[parentIndex] : Matrix4::identity();
//    _Transform( parentPose, nodeId );
//}
//
//void bxScene_Graph::setLocalPosition( bxScene_Graph::Id nodeId, const Vector3& pos )
//{
//    const int index = nodeId_indexSafe( nodeId, _data.size );
//    _data.localPose[index].setTranslation( pos );
//
//    const int parentIndex = _data.parent[index];
//    Matrix4 parentPose = (parentIndex != -1) ? _data.worldPose[parentIndex] : Matrix4::identity();
//    _Transform( parentPose, nodeId );
//}
//
//void bxScene_Graph::setLocalPose( bxScene_Graph::Id nodeId, const Matrix4& pose )
//{
//    const int index = nodeId_indexSafe( nodeId, _data.size );
//    _data.localPose[index] = pose;
//    
//    const int parentIndex = _data.parent[index];
//    Matrix4 parentPose = (parentIndex != -1) ? _data.worldPose[parentIndex] : Matrix4::identity();
//    _Transform( parentPose, nodeId );
//}
//
//void bxScene_Graph::setWorldPose( bxScene_Graph::Id nodeId, const Matrix4& pose )
//{
//    const int index = nodeId_indexSafe( nodeId, _data.size );
//    const int parentIndex = _data.parent[index];
//    const Matrix4 parentPose = (parentIndex != -1) ? ( _data.worldPose[parentIndex] ) : Matrix4::identity();
//    _data.localPose[index] = inverse( parentPose ) * pose;
//
//    _Transform( parentPose, nodeId );
//}
//
//void bxScene_Graph::_Transform( const Matrix4& parentPose, bxScene_Graph::Id nodeId )
//{
//    const int index = nodeId_indexSafe( nodeId, _data.size );
//    _data.worldPose[index] = parentPose * _data.localPose[index];
//}

namespace bx
{
    void gameSceneStartup( GameScene* scene, bxEngine* engine )
    {
        bx::gfxSceneCreate( &scene->gfxScene, engine->gfxContext );
        bx::phxSceneCreate( &scene->phxScene, engine->phxContext );

        bx::designBlockStartup( &scene->dblock );
        bx::cameraManagerStartup( &scene->cameraManager, engine->gfxContext );


        bx::terrainCreate( &scene->terrain, scene );
        scene->character = bx::character_new();

        //scene->flock = bxGame::flock_new();

        bx::characterInit( scene->character, engine->gdiDevice, scene, Matrix4( Matrix3::identity(), Vector3( 0.f, 2.f, 0.f ) ) );
        //bxGame::flock_init( scene->flock, 128, Vector3( 0.f ), 5.f );
    }

    void gameSceneShutdown( GameScene* scene, bxEngine* engine )
    {
        //bxGame::flock_delete( &scene->flock );

        bx::characterDeinit( scene->character, engine->gdiDevice );
        bx::character_delete( &scene->character );

        bx::terrainDestroy( &scene->terrain, scene );

        bx::cameraManagerShutdown( &scene->cameraManager );
    
        scene->dblock->cleanUp();
        scene->dblock->manageResources( scene->gfxScene, scene->phxScene );
        bx::designBlockShutdown( &scene->dblock );
    
        bx::phxSceneDestroy( &scene->phxScene );
        bx::gfxSceneDestroy( &scene->gfxScene );
    }

}///