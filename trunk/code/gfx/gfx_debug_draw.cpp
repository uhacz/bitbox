#include "gfx_debug_draw.h"
#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include <util/array.h>
#include <util/debug.h>
#include <util/memory.h>
#include <util/poly/poly_shape.h>
#include <util/color.h>
#include <util/thread/mutex.h>
#include <util/range_splitter.h>

struct BIT_ALIGNMENT_16 bxGfxDebugDraw_Shpere
{
    Vector4 pos_radius;
    u32 colorRGBA;
    i32 depth;
};

struct BIT_ALIGNMENT_16 bxGfxDebugDraw_Box
{
    Matrix4 transform; // pose * (ext*2)
    u32 colorRGBA;
    i32 depth;
};

struct BIT_ALIGNMENT_16 bxGfxDebugDraw_Line
{
    Vector3 pointA;
    Vector3 pointB;
    u32 colorRGBA;
    i32 depth;
};

struct bxGfxDebugDraw_InstanceBuffer
{
    enum { eMAX_INSTANCES = 16 };

    Matrix4 worldMatrix[eMAX_INSTANCES];
    float4_t colorRGBA[eMAX_INSTANCES];
};

struct bxGfxDebugDrawContext
{
    enum { eMAX_LINES = 64 };

    bxGfxDebugDrawContext()
        : fxI(0)
        , rSource_sphere(0)
        , rSource_box(0)
    {}

    bxBenaphore lock;

    bxGdiBuffer cbuffer_instances;

    bxGdiShaderFx_Instance* fxI;
    bxGdiRenderSource* rSource_sphere;
    bxGdiRenderSource* rSource_box;
    bxGdiRenderSource* rSource_lines;

    array_t<bxGfxDebugDraw_Shpere> spheres;
    array_t<bxGfxDebugDraw_Box>    boxes;
    array_t<bxGfxDebugDraw_Line>   lines;

};

namespace bxGfxDebugDraw
{
    static bxGfxDebugDrawContext* __dd = 0;

    void _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        __dd = BX_NEW( bxDefaultAllocator(), bxGfxDebugDrawContext );

        __dd->cbuffer_instances = dev->createConstantBuffer( sizeof( bxGfxDebugDraw_InstanceBuffer ) );

        __dd->fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "debug" );
        SYS_ASSERT( __dd->fxI != 0 );

        bxPolyShape polyShape;
        bxPolyShape_createBox( &polyShape, 1 );
        __dd->rSource_box = bxGdi::renderSource_createFromPolyShape( dev, polyShape );
        bxPolyShape_deallocateShape( &polyShape );

        bxPolyShape_createShpere( &polyShape, 3 );
        __dd->rSource_sphere = bxGdi::renderSource_createFromPolyShape( dev, polyShape );
        bxPolyShape_deallocateShape( &polyShape );
        
        {
            bxGdiVertexStreamDesc vsDesc;
            vsDesc.addBlock( bxGdi::eSLOT_POSITION, bxGdi::eTYPE_FLOAT, 4 );

            bxGdiVertexBuffer vBuffer = dev->createVertexBuffer( vsDesc, bxGfxDebugDrawContext::eMAX_LINES * 2, 0 );

            __dd->rSource_lines = bxGdi::renderSource_new( 1 );
            bxGdi::renderSource_setVertexBuffer( __dd->rSource_lines, vBuffer, 0 );

        }

        array::reserve( __dd->spheres, bxGfxDebugDraw_InstanceBuffer::eMAX_INSTANCES );
        array::reserve( __dd->boxes, bxGfxDebugDraw_InstanceBuffer::eMAX_INSTANCES );
        array::reserve( __dd->spheres, bxGfxDebugDrawContext::eMAX_LINES );

    }
    void _Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
        if( !__dd )
            return;

        bxGdi::renderSource_releaseAndFree( dev, &__dd->rSource_lines );
        bxGdi::renderSource_releaseAndFree( dev, &__dd->rSource_box );
        bxGdi::renderSource_releaseAndFree( dev, &__dd->rSource_sphere );
        bxGdi::shaderFx_releaseWithInstance( dev, &__dd->fxI );
        dev->releaseBuffer( &__dd->cbuffer_instances );

        BX_DELETE0( bxDefaultAllocator(), __dd );
    }

    void addSphere( const Vector4& pos_radius, u32 colorRGBA, int depth )
    {
        if( !__dd )
            return;

        bxScopeBenaphore lock( __dd->lock );

        bxGfxDebugDraw_Shpere sphere;
        sphere.pos_radius = pos_radius;
        sphere.colorRGBA = colorRGBA;
        sphere.depth = depth;

        array::push_back( __dd->spheres, sphere );
    }
    void addBox( const Matrix4& pose, const Vector3& ext, u32 colorRGBA, int depth )
    {
        if( !__dd )
            return;

        bxScopeBenaphore lock( __dd->lock );

        bxGfxDebugDraw_Box box;
        box.transform = appendScale( pose, ext * twoVec );
        box.colorRGBA = colorRGBA;
        box.depth = depth;

        array::push_back( __dd->boxes, box );

    }
    void addLine( const Vector3& pointA, const Vector3& pointB, u32 colorRGBA, int depth )
    {
        if( !__dd )
            return;

        bxScopeBenaphore lock( __dd->lock );

        bxGfxDebugDraw_Line line;
        line.pointA = pointA;
        line.pointB = pointB;
        line.colorRGBA = colorRGBA;
        line.depth = depth;

        array::push_back( __dd->lines, line );
    }

    void flush( bxGdiContext* ctx, const Matrix4& viewProj )
    {
        if( !__dd )
            return;

        bxScopeBenaphore lock( __dd->lock );
        
        bxGdiHwStateDesc hwState;
        hwState.raster.fillMode = bxGdi::eFILL_WIREFRAME;

        __dd->fxI->setUniform( "view_proj_matrix", viewProj );
        __dd->fxI->setHwState( 0, hwState );
        __dd->fxI->setHwState( 1, hwState );

        //ctx->clear();
        ctx->setCbuffer( __dd->cbuffer_instances, 1, bxGdi::eSTAGE_MASK_VERTEX );
        
        const int nSpheres = array::size( __dd->spheres );
        const int nBoxes = array::size( __dd->boxes );

        if( nSpheres || nBoxes )
        {
            bxGfxDebugDraw_InstanceBuffer ibuffer;

            bxGdi::shaderFx_enable( ctx, __dd->fxI, "object" );
            if( nSpheres )
            {
                const bxGdiRenderSurface surf = bxGdi::renderSource_surface( __dd->rSource_sphere, bxGdi::eTRIANGLES );
                bxGdi::renderSource_enable( ctx, __dd->rSource_sphere );
                bxRangeSplitter splitter = bxRangeSplitter::splitByGrab( nSpheres, bxGfxDebugDraw_InstanceBuffer::eMAX_INSTANCES );
                while( splitter.elementsLeft() )
                {
                    const int offset = splitter.grabbedElements;
                    const int grab = splitter.nextGrab();
                    for( int iobj = 0; iobj < grab; ++iobj )
                    {
                        const bxGfxDebugDraw_Shpere& sph = __dd->spheres[offset + iobj];
                        ibuffer.worldMatrix[iobj] = appendScale( Matrix4::translation( sph.pos_radius.getXYZ() ), Vector3( sph.pos_radius.getW() * twoVec ) );
                        bxColor::u32ToFloat4( sph.colorRGBA, ibuffer.colorRGBA[iobj].xyzw );
                    }

                    ctx->backend()->updateCBuffer( __dd->cbuffer_instances, &ibuffer );

                    bxGdi::renderSurface_drawIndexedInstanced( ctx, surf, grab );
                }
            }
            if( nBoxes )
            {
                const bxGdiRenderSurface surf = bxGdi::renderSource_surface( __dd->rSource_box, bxGdi::eTRIANGLES );
                bxGdi::renderSource_enable( ctx, __dd->rSource_box );
                bxRangeSplitter splitter = bxRangeSplitter::splitByGrab( nBoxes, bxGfxDebugDraw_InstanceBuffer::eMAX_INSTANCES );
                while ( splitter.elementsLeft() )
                {
                    const int offset = splitter.grabbedElements;
                    const int grab = splitter.nextGrab();
                    for ( int iobj = 0; iobj < grab; ++iobj )
                    {
                        const bxGfxDebugDraw_Box& box = __dd->boxes[offset + iobj];
                        ibuffer.worldMatrix[iobj] = box.transform;
                        bxColor::u32ToFloat4( box.colorRGBA, ibuffer.colorRGBA[iobj].xyzw );
                    }

                    ctx->backend()->updateCBuffer( __dd->cbuffer_instances, &ibuffer );
                    bxGdi::renderSurface_drawIndexedInstanced( ctx, surf, grab );
                }

            }

        }

        const int nLines = array::size( __dd->lines );
        if( nLines )
        {
            bxGdi::shaderFx_enable( ctx, __dd->fxI, "lines" );
            bxGdiVertexBuffer lineVBuffer = __dd->rSource_lines->vertexBuffers[0];
            bxGdi::renderSource_enable( ctx, __dd->rSource_lines );

            bxRangeSplitter splitter = bxRangeSplitter::splitByGrab( nLines, bxGfxDebugDrawContext::eMAX_LINES );
            while ( splitter.elementsLeft() )
            {
                const int offset = splitter.grabbedElements;
                const int grab = splitter.nextGrab();

                Vector4* lineBuffer = (Vector4*)ctx->backend()->mapVertices( lineVBuffer, 0, grab*2, bxGdi::eMAP_WRITE );

                for ( int iobj = 0; iobj < grab; ++iobj )
                {
                    const bxGfxDebugDraw_Line& line = __dd->lines[offset + iobj];
                    Vector4* dstLine = lineBuffer + iobj * 2;
                    dstLine[0].setXYZ( line.pointA );
                    dstLine[0].setW( TypeReinterpert( line.colorRGBA ).f );
                    dstLine[1].setXYZ( line.pointB );
                    dstLine[1].setW( TypeReinterpert( line.colorRGBA ).f );
                }

                ctx->backend()->unmapVertices( lineVBuffer );

                bxGdiRenderSurface surf( bxGdi::eLINES, 0, grab * 2 );
                bxGdi::renderSurface_draw( ctx, surf );
            }
        }

        array::clear( __dd->spheres );
        array::clear( __dd->boxes );
        array::clear( __dd->lines );
    }

}///