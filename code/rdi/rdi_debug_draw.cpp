#include "rdi_debug_draw.h"
#include "rdi.h"

#include <util/array.h>
#include <util/debug.h>
#include <util/memory.h>
#include <util/poly/poly_shape.h>
#include <util/color.h>
#include <util/thread/mutex.h>
#include <util/range_splitter.h>
#include <util/view_frustum.h>
#include <util/camera.h>
#include "resource_manager/resource_manager.h"

namespace bx{ namespace rdi
{
namespace debug_draw
{
struct BIT_ALIGNMENT_16 Shpere
{
    Vector4 pos_radius;
    u32 colorRGBA;
    i32 depth;
};

struct BIT_ALIGNMENT_16 Box
{
    Matrix4 transform; // pose * (ext*2)
    u32 colorRGBA;
    i32 depth;
};

struct BIT_ALIGNMENT_16 Line
{
    Vector3 pointA;
    Vector3 pointB;
    u32 colorRGBA;
    i32 depth;
};

struct InstanceBuffer
{
    enum { eMAX_INSTANCES = 16 };

    Matrix4 worldMatrix[eMAX_INSTANCES];
    float4_t colorRGBA[eMAX_INSTANCES];
};
struct MaterialData
{
    Matrix4 view_proj;
};

struct Context
{
    enum { eMAX_LINES = 64 };

    bxBenaphore lock;

    rdi::ConstantBuffer cbuffer_instances = {};
    rdi::ConstantBuffer cbuffer_mdata = {};
        
    rdi::RenderSource rSource_sphere = BX_RDI_NULL_HANDLE;
    rdi::RenderSource rSource_box    = BX_RDI_NULL_HANDLE;
    rdi::RenderSource rSource_lines  = BX_RDI_NULL_HANDLE;

    rdi::Pipeline pipeline_object = BX_RDI_NULL_HANDLE;
    rdi::Pipeline pipeline_lines  = BX_RDI_NULL_HANDLE;

    array_t<Shpere> spheres;
    array_t<Box>    boxes;
    array_t<Line>   lines;
};
static Context* __dd = nullptr;
  
//////////////////////////////////////////////////////////////////////////
void _Startup()
{
    ResourceManager* resourceManager = bx::GResourceManager();

    __dd = BX_NEW( bxDefaultAllocator(), Context );

    __dd->cbuffer_instances = device::CreateConstantBuffer( sizeof( InstanceBuffer ) );
    __dd->cbuffer_mdata = device::CreateConstantBuffer( sizeof( MaterialData ) );

    rdi::ShaderFile* sf = rdi::ShaderFileLoad( "shader/bin/debug.shader", resourceManager );

    rdi::PipelineDesc pip_desc = {};
    rdi::ResourceDescriptor rdesc = BX_RDI_NULL_HANDLE;

    pip_desc.Shader( sf, "object" );
    __dd->pipeline_object = rdi::CreatePipeline( pip_desc );
    rdesc = GetResourceDescriptor( __dd->pipeline_object );
    SetConstantBuffer( rdesc, "InstanceData", &__dd->cbuffer_instances );
    SetConstantBuffer( rdesc, "MaterialData", &__dd->cbuffer_mdata );

    pip_desc.Shader( sf, "lines" );
    __dd->pipeline_lines = rdi::CreatePipeline( pip_desc );
    rdesc = GetResourceDescriptor( __dd->pipeline_lines );
    SetConstantBuffer( rdesc, "MaterialData", &__dd->cbuffer_mdata );

    rdi::ShaderFileUnload( &sf, resourceManager );

    bxPolyShape polyShape;
    bxPolyShape_createBox( &polyShape, 1 );
    __dd->rSource_box = CreateRenderSourceFromPolyShape( polyShape );
    bxPolyShape_deallocateShape( &polyShape );

    bxPolyShape_createShpere( &polyShape, 3 );
    __dd->rSource_sphere = CreateRenderSourceFromPolyShape( polyShape );
    bxPolyShape_deallocateShape( &polyShape );

    {
        RenderSourceDesc desc;
        desc.VertexBuffer( VertexBufferDesc( EVertexSlot::POSITION ).DataType( EDataType::FLOAT, 4 ), nullptr );
        desc.Count( Context::eMAX_LINES * 2, 0 );
        __dd->rSource_lines = rdi::CreateRenderSource( desc );
    }

    array::reserve( __dd->spheres, InstanceBuffer::eMAX_INSTANCES );
    array::reserve( __dd->boxes, InstanceBuffer::eMAX_INSTANCES );
    array::reserve( __dd->spheres, Context::eMAX_LINES );

}
void _Shutdown()
{
    if( !__dd )
        return;

    ResourceManager* resourceManager = bx::GResourceManager();

    DestroyRenderSource( &__dd->rSource_lines );
    DestroyRenderSource( &__dd->rSource_box );
    DestroyRenderSource( &__dd->rSource_sphere );
    DestroyPipeline( &__dd->pipeline_lines );
    DestroyPipeline( &__dd->pipeline_object );

    device::DestroyConstantBuffer( &__dd->cbuffer_instances );
    device::DestroyConstantBuffer( &__dd->cbuffer_mdata );

    BX_DELETE0( bxDefaultAllocator(), __dd );
}

void _Flush( CommandQueue* cmdq, const Matrix4& view, const Matrix4& proj )
{
    if( !__dd )
        return;

    bxScopeBenaphore lock( __dd->lock );

    MaterialData mdata;
    mdata.view_proj = proj * view;
    context::UpdateCBuffer( cmdq, __dd->cbuffer_mdata, &mdata );
    
    const int nSpheres = array::size( __dd->spheres );
    const int nBoxes = array::size( __dd->boxes );

    if( nSpheres || nBoxes )
    {
        BindPipeline( cmdq, __dd->pipeline_object, true );

        if( nSpheres )
        {
            BindRenderSource( cmdq, __dd->rSource_sphere );
            
            InstanceBuffer ibuffer;
            bxRangeSplitter splitter = bxRangeSplitter::splitByGrab( nSpheres, InstanceBuffer::eMAX_INSTANCES );
            
            while( splitter.elementsLeft() )
            {
                const int offset = splitter.grabbedElements;
                const int grab = splitter.nextGrab();
                for( int iobj = 0; iobj < grab; ++iobj )
                {
                    const Shpere& sph = __dd->spheres[offset + iobj];
                    ibuffer.worldMatrix[iobj] = appendScale( Matrix4::translation( sph.pos_radius.getXYZ() ), Vector3( sph.pos_radius.getW() * twoVec ) );
                    bxColor::u32ToFloat4( sph.colorRGBA, ibuffer.colorRGBA[iobj].xyzw );
                }
                context::UpdateCBuffer( cmdq, __dd->cbuffer_instances, &ibuffer );
                SubmitRenderSourceInstanced( cmdq, __dd->rSource_sphere, grab );
            }
        }

        if( nBoxes )
        {
            BindRenderSource( cmdq, __dd->rSource_box );
            
            InstanceBuffer ibuffer;
            bxRangeSplitter splitter = bxRangeSplitter::splitByGrab( nBoxes, InstanceBuffer::eMAX_INSTANCES );

            while( splitter.elementsLeft() )
            {
                const int offset = splitter.grabbedElements;
                const int grab = splitter.nextGrab();
                for( int iobj = 0; iobj < grab; ++iobj )
                {
                    const Box& box = __dd->boxes[offset + iobj];
                    ibuffer.worldMatrix[iobj] = box.transform;
                    bxColor::u32ToFloat4( box.colorRGBA, ibuffer.colorRGBA[iobj].xyzw );
                }

                context::UpdateCBuffer( cmdq, __dd->cbuffer_instances, &ibuffer );
                SubmitRenderSourceInstanced( cmdq, __dd->rSource_box, grab );
            }
        }
    }

    const int nLines = array::size( __dd->lines );
    if( nLines )
    {
        BindPipeline( cmdq, __dd->pipeline_lines, true );

        VertexBuffer vbuffer = GetVertexBuffer( __dd->rSource_lines, 0 );
        BindRenderSource( cmdq, __dd->rSource_lines );

        //gdi::shaderFx_enable( ctx, __dd->fxI, "lines" );
        //bxGdiVertexBuffer lineVBuffer = __dd->rSource_lines->vertexBuffers[0];
        //gdi::renderSource_enable( ctx, __dd->rSource_lines );

        bxRangeSplitter splitter = bxRangeSplitter::splitByGrab( nLines, Context::eMAX_LINES );
        while( splitter.elementsLeft() )
        {
            const int offset = splitter.grabbedElements;
            const int grab = splitter.nextGrab();

            //Vector4* lineBuffer = (Vector4*)ctx->backend()->mapVertices( lineVBuffer, 0, grab * 2, gdi::eMAP_WRITE );
            Vector4* lineBuffer = (Vector4*)context::Map( cmdq, vbuffer, 0, grab * 2, EMapType::WRITE );

            for( int iobj = 0; iobj < grab; ++iobj )
            {
                const Line& line = __dd->lines[offset + iobj];
                Vector4* dstLine = lineBuffer + iobj * 2;
                dstLine[0].setXYZ( line.pointA );
                dstLine[0].setW( TypeReinterpert( line.colorRGBA ).f );
                dstLine[1].setXYZ( line.pointB );
                dstLine[1].setW( TypeReinterpert( line.colorRGBA ).f );
            }

            context::Unmap( cmdq, vbuffer );
            //ctx->backend()->unmapVertices( lineVBuffer );
            //bxGdiRenderSurface surf( gdi::eLINES, 0, grab * 2 );
            //gdi::renderSurface_draw( ctx, surf );
            context::SetTopology( cmdq, ETopology::LINES );
            context::Draw( cmdq, grab * 2, 0 );
        }
    }

    array::clear( __dd->spheres );
    array::clear( __dd->boxes );
    array::clear( __dd->lines );
}

//////////////////////////////////////////////////////////////////////////
void AddSphere( const Vector4& pos_radius, u32 colorRGBA, int depth )
{
    if( !__dd )
        return;

    bxScopeBenaphore lock( __dd->lock );

    Shpere sphere;
    sphere.pos_radius = pos_radius;
    sphere.colorRGBA = colorRGBA;
    sphere.depth = depth;

    array::push_back( __dd->spheres, sphere );
}
void AddBox( const Matrix4& pose, const Vector3& ext, u32 colorRGBA, int depth )
{
    if( !__dd )
        return;

    bxScopeBenaphore lock( __dd->lock );

    Box box;
    box.transform = appendScale( pose, ext * twoVec );
    box.colorRGBA = colorRGBA;
    box.depth = depth;

    array::push_back( __dd->boxes, box );

}
void AddLine( const Vector3& pointA, const Vector3& pointB, u32 colorRGBA, int depth )
{
    if( !__dd )
        return;

    bxScopeBenaphore lock( __dd->lock );

    Line line;
    line.pointA = pointA;
    line.pointB = pointB;
    line.colorRGBA = colorRGBA;
    line.depth = depth;

    array::push_back( __dd->lines, line );
}

void AddAxes( const Matrix4& pose )
{
    const Vector3 p = pose.getTranslation();
    AddLine( p, p + pose.getCol0().getXYZ(), 0xFF0000FF, 1 );
    AddLine( p, p + pose.getCol1().getXYZ(), 0x00FF00FF, 1 );
    AddLine( p, p + pose.getCol2().getXYZ(), 0x0000FFFF, 1 );
}
void AddFrustum( const Vector3 corners[8], u32 colorRGBA, int depth )
{
    AddLine( corners[0], corners[1], colorRGBA, true );
    AddLine( corners[2], corners[3], colorRGBA, true );
    AddLine( corners[4], corners[5], colorRGBA, true );
    AddLine( corners[6], corners[7], colorRGBA, true );

    AddLine( corners[0], corners[2], colorRGBA, true );
    AddLine( corners[2], corners[4], colorRGBA, true );
    AddLine( corners[4], corners[6], colorRGBA, true );
    AddLine( corners[6], corners[0], colorRGBA, true );

    AddLine( corners[1], corners[3], colorRGBA, true );
    AddLine( corners[3], corners[5], colorRGBA, true );
    AddLine( corners[5], corners[7], colorRGBA, true );
    AddLine( corners[7], corners[1], colorRGBA, true );

    //AddLine( corners[0], corners[4], colorRGBA, true );
    //AddLine( corners[2], corners[6], colorRGBA, true );
    //AddLine( corners[1], corners[5], colorRGBA, true );
    //AddLine( corners[3], corners[7], colorRGBA, true );
    //
    //AddLine( corners[0], corners[3], colorRGBA, true );
    //AddLine( corners[1], corners[2], colorRGBA, true );
    //AddLine( corners[4], corners[7], colorRGBA, true );
    //AddLine( corners[5], corners[6], colorRGBA, true );
}

void AddFrustum( const Matrix4& viewProj, u32 colorRGBA, int depth )
{
    Vector3 corners[8];
    bx::gfx::viewFrustumExtractCorners( corners, viewProj );
    AddFrustum( corners, colorRGBA, depth );
}

}}}///