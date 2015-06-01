#include "physics.h"
#include <util/array.h>
#include <util/handle_manager.h>

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

namespace bxPhysics{
    bxPhysics_CBoxHandle collisionBox_add( bxPhysics_CollisionBox* cnt, const Vector3& pos, const Quat& rot, const Vector3& ext )
    {
        int i0 = array::push_back( cnt->pos, pos );
        int i1 = array::push_back( cnt->rot, rot );
        int i2 = array::push_back( cnt->ext, ext );
        SYS_ASSERT( i0 == i1 == i2 );

        bxPhysics_CollisionBox::Indices::Handle handle = cnt->indices.add( i0 );
        return collisionHandle_create< bxPhysics_CBoxHandle>(  handle.asU32() );
    }
    void collisionBox_remove( bxPhysics_CollisionBox* cnt, bxPhysics_CBoxHandle* h )
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

    bxPhysics_CPlaneHandle collisionPlane_add( bxPhysics_CollisionPlane* cnt, const Vector4& plane )
    {
        int i0 = array::push_back( cnt->value, plane );
        bxPhysics_CollisionBox::Indices::Handle handle = cnt->indices.add( i0 );
        return collisionHandle_create< bxPhysics_CPlaneHandle >( handle.asU32() );
    }
    void collisionPlane_remove( bxPhysics_CollisionPlane* cnt, bxPhysics_CBoxHandle* h )
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
    bxPhysics_CSphereHandle collisionSphere_add( bxPhysics_CollisionSphere* cnt, const Vector4& sph )
    {
        int i0 = array::push_back( cnt->value, sph );
        bxPhysics_CollisionBox::Indices::Handle handle = cnt->indices.add( i0 );
        return collisionHandle_create< bxPhysics_CSphereHandle >( handle.asU32() );
    }
    void collisionSphere_remove( bxPhysics_CollisionSphere* cnt, bxPhysics_CSphereHandle* h )
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
};

namespace bxPhysics{

bxPhysics_CollisionSpace* collisionSpace_new()
{
    return 0;
}
void bxPhysics::collisionSpace_delete( bxPhysics_CollisionSpace** cs )
{

}

}///


