#include "scene_graph.h"
#include <util/buffer_utils.h>
#include <util/hashmap.h>
#include <util/array.h>
#include <util/debug.h>
#include <string.h>

namespace bx
{
    
    SceneGraph::Pose::Pose( const Matrix4& m4 )
    {
        Matrix3 rotm = m4.getUpper3x3();
        floatInVec x = length( rotm.getCol0() );
        floatInVec y = length( rotm.getCol1() );
        floatInVec z = length( rotm.getCol2() );
        
        pos = m4.getTranslation();
        rot.setCol0( rotm.getCol0() / x );
        rot.setCol1( rotm.getCol1() / y );
        rot.setCol2( rotm.getCol2() / z );
        scale = Vector3( x, y, z );
    }

    Matrix4 SceneGraph::Pose::toMatrix4( const Pose& pose )
    {
        return appendScale( Matrix4( pose.rot, pose.pos ), pose.scale );
    }
    //////////////////////////////////////////////////////////////////////////
    SceneGraph::SceneGraph( bxAllocator* alloc /*= bxDefaultAllocator() */ )
        : _allocator( alloc )
    {}
    SceneGraph::~SceneGraph()
    {
        BX_FREE0( _allocator, _data.buffer );
    }

    void SceneGraph::grow()
    {
        allocate( _data.capacity * 2 + 8 );
    }

    void SceneGraph::allocate( u32 num )
    {
        SYS_ASSERT( num > _data.size );
        const u32 bytes = num * (   sizeof( id_t )
                                  + sizeof( Matrix4 )
                                  + sizeof( Pose )
                                  + sizeof( TransformInstance ) * 4
                                  + sizeof( u8 )
                                  );

        InstanceData new_data;
        new_data.size = _data.size;
        new_data.capacity = num;
        new_data.buffer = _allocator->alloc( bytes, 16 );
        memset( new_data.buffer, 0x00, bytes );

        bxBufferChunker chunker( new_data.buffer, bytes );

        new_data.world = chunker.add<Matrix4>( num );
        new_data.local = chunker.add<Pose>( num );
        new_data.unit = chunker.add<id_t>( num );
        new_data.parent = chunker.add<TransformInstance>( num );
        new_data.first_child  = chunker.add<TransformInstance>( num );
        new_data.next_sibling = chunker.add<TransformInstance>( num );
        new_data.prev_sibling = chunker.add<TransformInstance>( num );
        new_data.changed = chunker.add<u8>( num );

        chunker.check();

        BX_CONTAINER_COPY_DATA( &new_data, &_data, world );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, local );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, unit );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, parent );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, first_child );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, next_sibling );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, prev_sibling );
        BX_CONTAINER_COPY_DATA( &new_data, &_data, changed );

        _allocator->free( _data.buffer );
        _data = new_data;
    }

    void SceneGraph::unitDestroyedCallback( id_t id )
    {
        TransformInstance ti = get( id );
        if( isValid( ti ) )
            destroy( ti );
    }

    TransformInstance SceneGraph::create( id_t id, const Matrix4& pose )
    {
        SYS_ASSERT( hashmap::lookup( _map, id.hash ) == nullptr );

        if( _data.capacity == _data.size )
            grow();

        const u32 last = _data.size;

        _data.unit[last] = id;
        _data.world[last] = pose;
        _data.local[last] = Pose( pose );
        _data.parent[last].i = UINT32_MAX;
        _data.first_child[last].i = UINT32_MAX;
        _data.next_sibling[last].i = UINT32_MAX;
        _data.prev_sibling[last].i = UINT32_MAX;
        _data.changed[last] = 0;

        ++_data.size;

        hashmap::set( _map, id.hash, last );

        return makeInstance( last );
    }

    TransformInstance SceneGraph::create( id_t id, const Vector3& pos, const Quat& rot, const Vector3& scale )
    {
        return create( id, appendScale( Matrix4( rot, pos ), scale ) );
    }

    void SceneGraph::destroy( TransformInstance i )
    {
        SYS_ASSERT( i.i < _data.size );

        const u32 last = _data.size - 1;
        const id_t u = _data.unit[i.i];
        const id_t last_u = _data.unit[last];

        _data.unit[i.i] = _data.unit[last];
        _data.world[i.i] = _data.world[last];
        _data.local[i.i] = _data.local[last];
        _data.parent[i.i] = _data.parent[last];
        _data.first_child[i.i] = _data.first_child[last];
        _data.next_sibling[i.i] = _data.next_sibling[last];
        _data.prev_sibling[i.i] = _data.prev_sibling[last];
        _data.changed[i.i] = _data.changed[last];

        hashmap::set( _map, last_u.hash, i.i );
        hashmap::eraseByKey( _map, u.hash );

        --_data.size;
    }

    TransformInstance SceneGraph::get( id_t id )
    {
        hashmap_t::cell_t* cell = hashmap::lookup( _map, id.hash );
        u32 i = ( cell ) ? (u32)cell->value : UINT32_MAX;
        return makeInstance( i );
    }

    void SceneGraph::setLocalPosition( TransformInstance i, const Vector3& pos )
    {
        SYS_ASSERT( i.i < _data.size );
        _data.local[i.i].pos = pos;
        setLocal( i );
    }

    void SceneGraph::setLocalRotation( TransformInstance i, const Quat& rot )
    {
        SYS_ASSERT( i.i < _data.size );
        _data.local[i.i].rot = Matrix3( rot );
        setLocal( i );
    }

    void SceneGraph::setLocalScale( TransformInstance i, const Vector3& scale )
    {
        SYS_ASSERT( i.i < _data.size );
        _data.local[i.i].scale = scale;
        setLocal( i );
    }

    void SceneGraph::setLocalPose( TransformInstance i, const Matrix4& pose )
    {
        SYS_ASSERT( i.i < _data.size );
        _data.local[i.i] = Pose( pose );
        setLocal( i );
    }

    void SceneGraph::setWorldPose( TransformInstance i, const Matrix4& pose )
    {
        SYS_ASSERT( i.i < _data.size );
        _data.world[i.i] = pose;
        _data.changed[i.i] = true;
    }

    Vector3 SceneGraph::localPosition( TransformInstance i ) const
    {
        SYS_ASSERT( i.i < _data.size );
        return _data.local[i.i].pos;
    }

    Quat SceneGraph::localRotation( TransformInstance i ) const
    {
        SYS_ASSERT( i.i < _data.size );
        return Quat( _data.local[i.i].rot );
    }

    Vector3 SceneGraph::localScale( TransformInstance i ) const
    {
        SYS_ASSERT( i.i < _data.size );
        return _data.local[i.i].scale;
    }

    Matrix4 SceneGraph::localPose( TransformInstance i ) const
    {
        SYS_ASSERT( i.i < _data.size );
        return Pose::toMatrix4( _data.local[i.i] );
    }

    Vector3 SceneGraph::worldPosition( TransformInstance i ) const
    {
        SYS_ASSERT( i.i < _data.size );
        return _data.world[i.i].getTranslation();
    }

    Quat SceneGraph::worldRotation( TransformInstance i ) const
    {
        SYS_ASSERT( i.i < _data.size );
        return Quat( _data.world[i.i].getUpper3x3() );
    }

    Matrix4 SceneGraph::worldPose( TransformInstance i ) const
    {
        SYS_ASSERT( i.i < _data.size );
        return _data.world[i.i];
    }

    u32 SceneGraph::size() const
    {
        return _data.size;
    }

    void SceneGraph::link( TransformInstance child, TransformInstance parent )
    {
        SYS_ASSERT( child.i < _data.size );
        SYS_ASSERT( parent.i < _data.size );

        unlink( child );

        if( !isValid( _data.first_child[parent.i] ) )
        {
            _data.first_child[parent.i] = child;
            _data.parent[child.i] = parent;
        }
        else
        {
            TransformInstance prev = { UINT32_MAX };
            TransformInstance node = _data.first_child[parent.i];
            while( isValid( node ) )
            {
                prev = node;
                node = _data.next_sibling[node.i];
            }

            _data.next_sibling[prev.i] = child;

            _data.first_child[child.i].i = UINT32_MAX;
            _data.next_sibling[child.i].i = UINT32_MAX;
            _data.prev_sibling[child.i] = prev;
        }

        Matrix4 parent_tr = _data.world[parent.i];
        Matrix4 child_tr = _data.world[child.i];
        
        Vector4 px = parent_tr.getCol0();
        Vector4 py = parent_tr.getCol1();
        Vector4 pz = parent_tr.getCol2();
        Vector4 cx = child_tr.getCol0();
        Vector4 cy = child_tr.getCol1();
        Vector4 cz = child_tr.getCol2();

        const Vector3 cs = Vector3( length( cx ), length( cy ), length( cz ) );

        parent_tr.setCol0( normalize( px ) );
        parent_tr.setCol1( normalize( py ) );
        parent_tr.setCol2( normalize( pz ) );
        child_tr.setCol0( normalize( cx ) );
        child_tr.setCol1( normalize( cy ) );
        child_tr.setCol2( normalize( cz ) );

        const Matrix4 rel_tr = inverse( parent_tr ) * child_tr;

        _data.local[child.i].pos = rel_tr.getTranslation();
        _data.local[child.i].rot = rel_tr.getUpper3x3();
        _data.local[child.i].scale = cs;
        _data.parent[child.i] = parent;

        transform( parent_tr, child );
    }

    void SceneGraph::unlink( TransformInstance child )
    {
        SYS_ASSERT( child.i < _data.size );

        if( !isValid( _data.parent[child.i] ) )
            return;

        if( !isValid( _data.prev_sibling[child.i] ) )
            _data.first_child[_data.parent[child.i].i] = _data.next_sibling[child.i];
        else
            _data.next_sibling[_data.prev_sibling[child.i].i] = _data.next_sibling[child.i];

        if( isValid( _data.next_sibling[child.i] ) )
            _data.prev_sibling[_data.next_sibling[child.i].i] = _data.prev_sibling[child.i];

        _data.parent[child.i].i = UINT32_MAX;
        _data.next_sibling[child.i].i = UINT32_MAX;
        _data.prev_sibling[child.i].i = UINT32_MAX;
    }

    void SceneGraph::clearChanged()
    {
        for( u32 i = 0; i < _data.size; ++i )
        {
            _data.changed[i] = 0;
        }
    }

    void SceneGraph::getChanged( array_t<id_t>* units, array_t<Matrix4>* world_poses )
    {
        for( u32 i = 0; i < _data.size; ++i )
        {
            if( _data.changed[i] )
            {
                array::push_back( *units, _data.unit[i] );
                array::push_back( *world_poses, _data.world[i] );
            }
        }
    }

    bool SceneGraph::isValid( TransformInstance i )
    {
        return i.i != UINT32_MAX;
    }

    void SceneGraph::setLocal( TransformInstance i )
    {
        TransformInstance parent = _data.parent[i.i];
        Matrix4 parent_tm = isValid( parent ) ? _data.world[parent.i] : Matrix4::identity();
        transform( parent_tm, i );

        _data.changed[i.i] = true;
    }

    void SceneGraph::transform( const Matrix4& parent, TransformInstance i )
    {
        _data.world[i.i] = localPose( i ) * parent;

        TransformInstance child = _data.first_child[i.i];
        while( isValid( child ) )
        {
            transform( _data.world[i.i], child );
            child = _data.next_sibling[child.i];
        }
    }

}////