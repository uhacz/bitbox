#include "renderer_scene.h"
#include <util/id_array.h>
#include <util/string_util.h>
#include <util/debug.h>

namespace bx{ namespace gfx{

static inline MeshInstance makeMeshInstance( id_t id )
{
    MeshInstance mi = { id.hash };
    return mi;
}
static inline MeshInstance makeInvalidMeshInstance()
{
    return makeMeshInstance( makeInvalidHandle<id_t>() );
}

Matrix4* getMatrixPtr( MeshInstanceMatrix& m, u32 numInstances )
{
    SYS_ASSERT( numInstances > 0 );
    return ( numInstances == 1 ) ? (Matrix4*)m._single : m._multi;
}

void SceneImpl::prepare()
{
}
void SceneImpl::unprepare()
{
}

MeshInstance SceneImpl::add( const char* name, u32 numInstances )
{
    MeshInstance mi = find( name );
    if( id_array::has( _id, make_id( mi.i ) ) )
    {
        bxLogError( "MeshInstance with name '%s' already exists", name );
        return makeInvalidMeshInstance();
    }

    id_t id = id_array::create( _id );
    const u32 index = id_array::index( _id, id );

    _SetToDefaults( index );

    _names[index] = string::duplicate( _names[index], name );
    _num_instances[index] = numInstances;
    if( numInstances > 1 )
    {
        u32 mem_size = numInstances * sizeof( Matrix4 );
        _matrices[index]._multi = (Matrix4*)BX_MALLOC( bxDefaultAllocator(), mem_size, 16 );
    }

    Matrix4* matrices = getMatrixPtr( _matrices[index], numInstances );
    for( u32 i = 0; i < numInstances; ++i )
    {
        matrices[i] = Matrix4::identity();
    }

    return makeMeshInstance( id );
}

void SceneImpl::remove( MeshInstance mi )
{

}
MeshInstance SceneImpl::find( const char* name )
{
    const u32 n = id_array::size( _id );
    for( u32 i = 0; i < n; ++i )
    {
        if( string::equal( name, _names[i] ) )
        {
            return _mesh_instance[i];
        }
    }
    return makeInvalidMeshInstance();
}

void SceneImpl::_SetToDefaults( u32 index )
{
    string::free_and_null( &_names[index] );
    _mesh_instance [index] = makeInvalidMeshInstance();
    _render_sources[index] = BX_GFX_NULL_HANDLE;
    _materials     [index] = {};
    
    if( _num_instances[index] > 1 )
    {
        BX_FREE( bxDefaultAllocator(), _matrices[index]._multi );
        memset( &_matrices[index], 0x00, sizeof( MeshInstanceMatrix ) );
    }
    _num_instances[index] = 0;
}




}
}///