#include "physics.h"
#include <util/array.h>
#include <util/handle_manager.h>
#include <gfx/gfx_debug_draw.h>

namespace bxPhysics{
    template< typename T >
    inline T collisionHandle_create( u32 uhandle )
    {
        T h = { uhandle };
        return h;
    }

    template< typename Tcontainer, typename Thandle >
    inline int collisionContainer_has( Tcontainer* cnt, Thandle handle )
    {
        return cnt->indices.isValid( Tcontainer::Indices::Handle( handle.h ) );
    }

    template< typename Tcontainer, typename Thandle >
    int collisionContainer_removeHandle( u32* removedIndex, u32* lastIndex, Tcontainer* cnt, Thandle* h )
    {
        if( !collisionContainer_has( cnt, h[0] ) )
            return -1;

        if( cnt->empty() )
        {
            bxLogWarning( "No box instances left!!!" );
            return -1;
        }

        int count = cnt->size();
        SYS_ASSERT( count == cnt->indices.size() );

        bxPhysics_CollisionBox::Indices::Handle handle( h->h );
        lastIndex[0] = count - 1;
        removedIndex[0] = packedArrayHandle_remove( &cnt->indices, handle, lastIndex[0] );

        h->h = 0;
        return 0;
    }
}///

struct bxPhysics_CollisionBox
{
    typedef bxHandleManager<u32> Indices;
    Indices indices;

    array_t< Vector3 > pos;
    array_t< Quat > rot;
    array_t< Vector3 > ext;

    bxPhysics_CollisionBox()
    {}

    int size () const { return array::size( pos ); }
    int empty() const { return array::empty( pos ); }
};

struct bxPhysics_CollisionVector4
{
    typedef bxHandleManager<u32> Indices;
    Indices indices;

    array_t< Vector4 > value;

    bxPhysics_CollisionVector4()
    {}

    int size() const { return array::size( value ); }
    int empty() const { return array::empty( value ); }
};
typedef bxPhysics_CollisionVector4 bxPhysics_CollisionSphere;
typedef bxPhysics_CollisionVector4 bxPhysics_CollisionPlane;


struct bxPhysics_CollisionTriangles
{
    struct Count
    {
        u16 numPoints;
        u16 numIndices;
    };
    array_t< const Vector3* > pos;
    array_t< const u16* > index;
    array_t< Count > count;

    int size() const { return array::size( pos ); }
    int empty() const { return array::empty( pos ); }
};

namespace bxPhysics{
    bxPhysics_CBoxHandle collisionBox_create( bxPhysics_CollisionBox* cnt, const Vector3& pos, const Quat& rot, const Vector3& ext )
    {
        int i0 = array::push_back( cnt->pos, pos );
        int i1 = array::push_back( cnt->rot, rot );
        int i2 = array::push_back( cnt->ext, ext );
        SYS_ASSERT( i0 == i1 && i0 == i2 );

        bxPhysics_CollisionBox::Indices::Handle handle = cnt->indices.add( i0 );
        return collisionHandle_create< bxPhysics_CBoxHandle>(  handle.asU32() );
    }
    void collisionBox_release( bxPhysics_CollisionBox* cnt, bxPhysics_CBoxHandle* h )
    {
        u32 removedIndex, lastIndex;
        int ierr = collisionContainer_removeHandle( &removedIndex, &lastIndex, cnt, h );
        if( ierr == -1 )
        {
            return;
        }
        cnt->pos[removedIndex] = cnt->pos[lastIndex];
        cnt->rot[removedIndex] = cnt->rot[lastIndex];
        cnt->ext[removedIndex] = cnt->ext[lastIndex];
        array::pop_back( cnt->pos );
        array::pop_back( cnt->rot );
        array::pop_back( cnt->ext );
    }

    bxPhysics_CPlaneHandle collisionPlane_create( bxPhysics_CollisionPlane* cnt, const Vector4& plane )
    {
        int i0 = array::push_back( cnt->value, plane );
        bxPhysics_CollisionBox::Indices::Handle handle = cnt->indices.add( i0 );
        return collisionHandle_create< bxPhysics_CPlaneHandle >( handle.asU32() );
    }
    void collisionPlane_release( bxPhysics_CollisionPlane* cnt, bxPhysics_CBoxHandle* h )
    {
        u32 removedIndex, lastIndex;
        int ierr = collisionContainer_removeHandle( &removedIndex, &lastIndex, cnt, h );
        if( ierr == -1 )
        {
            return;
        }
        cnt->value[removedIndex] = cnt->value[lastIndex];
        array::pop_back( cnt->value );
    }
    bxPhysics_CSphereHandle collisionSphere_create( bxPhysics_CollisionSphere* cnt, const Vector4& sph )
    {
        int i0 = array::push_back( cnt->value, sph );
        bxPhysics_CollisionBox::Indices::Handle handle = cnt->indices.add( i0 );
        return collisionHandle_create< bxPhysics_CSphereHandle >( handle.asU32() );
    }
    void collisionSphere_release( bxPhysics_CollisionSphere* cnt, bxPhysics_CSphereHandle* h )
    {
        u32 removedIndex, lastIndex;
        int ierr = collisionContainer_removeHandle( &removedIndex, &lastIndex, cnt, h );
        if( ierr == -1 )
        {
            return;
        }
        cnt->value[removedIndex] = cnt->value[lastIndex];
        array::pop_back( cnt->value );
    }
}

struct bxPhysics_CollisionSpace
{
    bxPhysics_CollisionPlane plane;
    bxPhysics_CollisionSphere sphere;
    bxPhysics_CollisionBox box;
    bxPhysics_CollisionTriangles tri;
};

namespace bxPhysics{

    bxPhysics_CollisionSpace* __cspace = 0;

bxPhysics_CollisionSpace* collisionSpace_new()
{
    bxPhysics_CollisionSpace* cspace = BX_NEW( bxDefaultAllocator(), bxPhysics_CollisionSpace );
    __cspace = cspace;
    return cspace;
}
void bxPhysics::collisionSpace_delete( bxPhysics_CollisionSpace** cs )
{
    if( !cs[0] )
        return;

    if( !cs[0]->plane.empty() )
    {
        bxLogError( "There are unreleased planes in collision space '%d'", cs[0]->plane.size() );
    }
    if( !cs[0]->sphere.empty() )
    {
        bxLogError( "There are unreleased spheres in collision space '%d'", cs[0]->plane.size() );
    }
    if( !cs[0]->box.empty() )
    {
        bxLogError( "There are unreleased boxes in collision space '%d'", cs[0]->plane.size() );
    }

    BX_DELETE0( bxDefaultAllocator(), cs[0] );

    __cspace = 0;
}

bxPhysics_CPlaneHandle collisionSpace_createPlane( bxPhysics_CollisionSpace* cs, const Vector4& plane )
{
    return collisionPlane_create( &cs->plane, plane );
}

bxPhysics_CBoxHandle collisionSpace_createBox( bxPhysics_CollisionSpace* cs, const Vector3& pos, const Quat& rot, const Vector3& ext )
{
    return collisionBox_create( &cs->box, pos, rot, ext );
}

bxPhysics_CSphereHandle collisionSpace_createSphere( bxPhysics_CollisionSpace* cs, const Vector4& sph )
{
    return collisionSphere_create( &cs->sphere, sph );
}


void collisionSpace_debugDraw( bxPhysics_CollisionSpace* cs )
{
    {
        const u32 color = 0xFF0000FF;
        const int n = cs->plane.size();
        for( int i = 0; i < n; ++i )
        {
            const Vector4& plane = cs->plane.value[i];
            const Vector3 pos = projectPointOnPlane( Vector3( 0.f ), plane );
            const Matrix3 rot = createBasis( plane.getXYZ() );

            bxGfxDebugDraw::addBox( Matrix4( rot, pos ), Vector3( 10.f, FLT_EPSILON*2.f, 10.f ), color, 1 );
        }
    }
    {
        const u32 color = 0x00FF00FF;
        const int n = cs->sphere.size();
        for( int i = 0; i < n; ++i )
        {
            const Vector4& sphere = cs->sphere.value[i];
            bxGfxDebugDraw::addSphere( sphere, color, 1 );
        }
    }

    {
        const u32 color = 0x0000FFFF;
        const int n = cs->box.size();
        for( int i = 0; i < n; ++i )
        {
            const Vector3& pos = cs->box.pos[i];
            const Quat& rot    = cs->box.rot[i];
            const Vector3& ext = cs->box.ext[i];
            bxGfxDebugDraw::addBox( Matrix4( rot, pos ), ext, color, 1 );
        }
    }
}

bxPhysics_CTriHandle collisiosSpace_addTriangles( bxPhysics_CollisionSpace* cs, const Vector3* positions, int nPositions, const u16* indices, int nIndices )
{
    SYS_ASSERT( nPositions < 0xFFFF );
    SYS_ASSERT( nIndices < 0xFFFF );



}

}///
