#include "physics.h"
#include <util/array.h>
#include <util/handle_manager.h>
#include <util/bbox.h>
#include <util/buffer_utils.h>

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
    array_t< const u16* >     index;
    array_t< Count >          count;
    array_t< u32 >            offset_dPos;
    array_t< Vector3 >        dposData;

    int size() const { return array::size( pos ); }
    int empty() const { return array::empty( pos ); }
    void clear()
    {
        array::clear( pos );
        array::clear( index );
        array::clear( count );
        array::clear( offset_dPos );
        array::clear( dposData );
    }
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

}///

struct bxPhysics_Contacts
{
    void* memoryHandle;
    Vector3* normal;
    f32* depth;
    u16* index;

    i32 size;
    i32 capacity;
};

namespace bxPhysics
{



void _Contacts_allocateData( bxPhysics_Contacts* con, int newCap )
{
    if( newCap <= con->capacity )
        return;

    int memSize = 0;
    memSize += newCap * sizeof( *con->normal );
    memSize += newCap * sizeof( *con->depth );
    memSize += newCap * sizeof( *con->index );

    void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
    memset( mem, 0x00, memSize );

    bxBufferChunker chunker( mem, memSize );

    bxPhysics_Contacts newCon;
    newCon.memoryHandle = mem;
    newCon.normal = chunker.add< Vector3 >( newCap );
    newCon.depth = chunker.add< f32 >( newCap );
    newCon.index = chunker.add< u16 >( newCap );
    chunker.check();

    newCon.size = con->size;
    newCon.capacity = newCap;

    if( con->size )
    {
        BX_CONTAINER_COPY_DATA( newCon, con, normal );
        BX_CONTAINER_COPY_DATA( newCon, con, depth );
        BX_CONTAINER_COPY_DATA( newCon, con, index );
    }

    BX_FREE( bxDefaultAllocator(), con->memoryHandle );
    *con = newCon;
}
bxPhysics_Contacts* contacts_new( int capacity )
{
    bxPhysics_Contacts* contacts = BX_NEW( bxDefaultAllocator(), bxPhysics_Contacts );
    memset( contacts, 0x00, sizeof( bxPhysics_Contacts ) );
    _Contacts_allocateData( contacts, capacity );
    return contacts;
}

void contacts_delete( bxPhysics_Contacts** con )
{
    BX_DELETE0( bxDefaultAllocator(), con[0] );
}
void contacts_clear( bxPhysics_Contacts* con )
{
    con->size = 0;
}

int contacts_size( bxPhysics_Contacts* con )
{
    return con->size;
}

int contacts_pushBack( bxPhysics_Contacts* con, const Vector3& normal, float depth, u16 index )
{
    if( con->size + 1 > con->capacity )
    {
        _Contacts_allocateData( con, con->size * 2 + 8 );
    }

    int i = con->size++;
    con->normal[i] = normal;
    con->depth[i] = depth;
    con->index[i] = index;
    return i;
}
void contacts_get( bxPhysics_Contacts* con, Vector3* normal, float* depth, u16* index0, u16* index1, int i )
{
    SYS_ASSERT( i >= 0 || i < con->size );

    normal[0] = con->normal[i];
    depth[0] = con->depth[i];
    index0[0] = con->index[i];
    index1[0] = 0xFFFF;
}
}///


//bxPhysics_CTriHandle collisiosSpace_addTriangles( bxPhysics_CollisionSpace* cs, const Vector3* positions, int nPositions, const u16* indices, int nIndices )
//{
//    SYS_ASSERT( nPositions < 0xFFFF );
//    SYS_ASSERT( nIndices < 0xFFFF );
//    bxPhysics_CollisionTriangles::Count count;
//    count.numPoints = (u16)nPositions;
//    count.numIndices = (u16)nIndices;
//
//    bxPhysics_CollisionTriangles& ctri = cs->tri;
//
//    u32 offset_dPos = 0xFFFFFFFF;
//    {
//        const int curSize = array::size( ctri.dposData );
//        const int curCapacity = array::capacity( ctri.dposData );
//
//        offset_dPos = (u32)curSize;
//
//        const int spaceLeft = curCapacity - curSize;
//        if( spaceLeft < nPositions )
//        {
//            const int newCapacity = ( curCapacity + nPositions ) * 2;
//            array::reserve( ctri.dposData, newCapacity );
//            array::resize( ctri.dposData, curSize + nPositions );
//        }
//    }
//    
//    int i0 = array::push_back( ctri.pos, positions );
//    int i1 = array::push_back( ctri.index, indices );
//    int i2 = array::push_back( ctri.count, count );
//    int i3 = array::push_back( ctri.offset_dPos, offset_dPos );
//    
//    SYS_ASSERT( i0 == i1 );
//    SYS_ASSERT( i0 == i2 );
//    SYS_ASSERT( i0 == i3 );
//
//    return collisionHandle_create< bxPhysics_CTriHandle >( u32( i0 ) );
//}

//void collisionSpace_newFrame( bxPhysics_CollisionSpace* cs )
//{
//    cs->tri.clear();
//}

//struct bxPhysics_CollisionHashTable
//{
//    union CellKey
//    {
//        u64 hash;
//        struct {
//            i16 x;
//            i16 y;
//            i16 z;
//            i16 w; // padding
//        };
//    };
//    union CellData
//    {
//        u64 hash;
//        struct {
//            u16 objIndex;
//            u16 pointIndex;
//            i16 next;
//            i16 last;
//        };
//    };
//    hashmap_t table;
//    array_t< CellData > items;
//};

namespace
{
//#define BX_PHYSICS_COLLISION_HASH_PRIME1 73856093
//#define BX_PHYSICS_COLLISION_HASH_PRIME2 19349663
//#define BX_PHYSICS_COLLISION_HASH_PRIME3 83492791
//
//    inline u32 computeHash( const __m128 point, const __m128 cellSizeInv, unsigned hashTableSize )
//    {
//        const __m128i primes = { BX_PHYSICS_COLLISION_HASH_PRIME1, BX_PHYSICS_COLLISION_HASH_PRIME2, BX_PHYSICS_COLLISION_HASH_PRIME3, BX_PHYSICS_COLLISION_HASH_PRIME1 };
//        const __m128 gridPointF = vec_mul( point, cellSizeInv );
//        const __m128i gridPointI = _mm_cvttps_epi32( gridPointF );
//        const SSEScalar s( vec_mul( gridPointI, primes ) );
//        return ( u32( s.ix ) ^ u32( s.iy ) ^ u32( s.iz ) ) % hashTableSize;
//    }

    //inline u64 computeHash( const __m128 point, const __m128 cellSizeInv )
    //{
    //    //const __m128i primes = { BX_PHYSICS_COLLISION_HASH_PRIME1, BX_PHYSICS_COLLISION_HASH_PRIME2, BX_PHYSICS_COLLISION_HASH_PRIME3, BX_PHYSICS_COLLISION_HASH_PRIME1 };
    //    const __m128 gridPointF = vec_mul( point, cellSizeInv );
    //    const __m128i gridPointI = _mm_cvttps_epi32( gridPointF );
    //    const SSEScalar s( gridPointI );
    //    
    //    bxPhysics_CollisionHashTable::CellKey ckey = { 0 };
    //    ckey.x = i16( s.ix );
    //    ckey.y = i16( s.iy );
    //    ckey.z = i16( s.iz );

    //    return ckey.hash;
    //}
}///

//void collisionHashTable_insert( bxPhysics_CollisionHashTable* table, bxPhysics_CollisionSpace* cs, bxPhysics_CTriHandle hTri )
//{
//    
//}
//
//void collisionSpace_detect( bxPhysics_CollisionSpace* cs )
//{
//
//}
//
//void collisionSpace_resolve( bxPhysics_CollisionSpace* cs )
//{
//
//}
namespace bxPhysics{



void collisionSpace_collide( bxPhysics_CollisionSpace* cs, bxPhysics_Contacts* contacts, Vector3* points, int nPoints )
{
    int n = 0;
    for ( int ipoint = 0; ipoint < nPoints; ++ipoint )
    {
        const Vector3 pt0 = points[ipoint];
        Vector3 pt = pt0;

        n = cs->plane.size();
        for ( int i = 0; i < n; ++i )
        {
            const Vector4& plane = cs->plane.value[i];
            const floatInVec d = dot( plane, Vector4( pt, oneVec ) );
            pt += -plane.getXYZ() * minf4( d, zeroVec );
        }

        n = cs->sphere.size();
        for ( int i = 0; i < n; ++i )
        {
            const Vector4& sphere = cs->sphere.value[i];
            const Vector3 v = pt - sphere.getXYZ();
            const floatInVec d = length( v );

            pt += normalizeSafe( v ) * maxf4( sphere.getW() - d, zeroVec );
        }

        n = cs->box.size();
        for ( int i = 0; i < n; ++i )
        {
            const Vector3& pos = cs->box.pos[i];
            const Quat& rot = cs->box.rot[i];
            const Vector3& ext = cs->box.ext[i];

            const Vector3 ptInBoxSpace = fastRotateInv( rot, pt - pos );
            if( !bxAABB::isPointInside( bxAABB( -ext, ext ), ptInBoxSpace ) )
                continue;

            const Vector3 absPtInBoxSpace = absPerElem( ptInBoxSpace );
            const Vector3 distToSurface = ext - absPtInBoxSpace;

            const SSEScalar dts( distToSurface.get128() );

            Vector3 dpos( 0.f );
            if( dts.y < dts.x )
            {
                if( dts.y < dts.z ) 
                {
                    // y
                    dpos = select( Vector3( 0.f, -dts.y, 0.f ), Vector3( 0.f, dts.y, 0.f ), ptInBoxSpace.getY() > zeroVec );
                }
                else 
                {
                    // z
                    dpos = select( Vector3( 0.f, 0.f, -dts.z ), Vector3( 0.f, 0.f, dts.z ), ptInBoxSpace.getZ() > zeroVec );
                }
            }
            else
            {
                if ( dts.x < dts.z )
                {
                    // x
                    dpos = select( Vector3( -dts.x, 0.f, 0.f ), Vector3( dts.x, 0.f, 0.f ), ptInBoxSpace.getX() > zeroVec );
                }
                else
                {
                    // z
                    dpos = select( Vector3( 0.f, 0.f, -dts.z ), Vector3( 0.f, 0.f, dts.z ), ptInBoxSpace.getZ() > zeroVec );
                }
            }
            dpos = fastRotate( rot, dpos );
            pt += dpos;
        }

        const Vector3 dpos = pt - pt0;
        const float depth = length( dpos ).getAsFloat();
        if( depth > FLT_EPSILON )
        {
            SYS_ASSERT( ipoint < 0xFFFF );
            const Vector3 normal = normalize( dpos );
            contacts_pushBack( contacts, normal, depth, u16( ipoint ) );
        }

        points[ipoint] = pt;
    }
}

void collisionSpace_collide( bxPhysics_CollisionSpace* cs, bxPhysics_Contacts* contacts, Vector3* points, int nPoints, const u16* indices, int nIndices )
{
    SYS_ASSERT( (nIndices % 3) == 0 );
    for( int itri = 0; itri < nIndices; itri += 3 )
    {
        const u32 triIndices[] = 
        {
            indices[itri + 0],
            indices[itri + 1],
            indices[itri + 2],
        };

        Vector3 triPoints[] =
        {
            points[triIndices[0]],
            points[triIndices[1]],
            points[triIndices[2]],
        };

        int n = 0;
        n = cs->plane.size();
        for ( int i = 0; i < n; ++i )
        {
            const Vector4& plane = cs->plane.value[i];
            const floatInVec d0 = dot( plane, Vector4( triPoints[0], oneVec ) );
            const floatInVec d1 = dot( plane, Vector4( triPoints[1], oneVec ) );
            const floatInVec d2 = dot( plane, Vector4( triPoints[2], oneVec ) );
            const floatInVec d = minf4( d0, minf4( d1, d2 ) );
            const Vector3 displ = -plane.getXYZ() * minf4( d, zeroVec );
            triPoints[0] += displ;
            triPoints[1] += displ;
            triPoints[2] += displ;
        }
    }
}

////
////
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
            const Quat& rot = cs->box.rot[i];
            const Vector3& ext = cs->box.ext[i];
            bxGfxDebugDraw::addBox( Matrix4( rot, pos ), ext, color, 1 );
        }
    }
}

}///
