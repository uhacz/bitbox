#include <system/application.h>

#include <util/time.h>
#include <util/handle_manager.h>

#include "test_console.h"
#include "entity.h"
//#include "scene.h"

#include "demo_engine.h"
#include "simple_scene.h"
#include "util/perlin_noise.h"
#include "voxel.h"
#include "grid.h"
#include "../gfx/gfx_debug_draw.h"
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//union bxVoxelGrid_Coords
//{
//    u64 hash;
//    struct
//    {
//        u64 x : 22;
//        u64 y : 21;
//        u64 z : 21;
//    };
//};
//
//struct bxVoxelGrid_Item
//{
//    uptr data;
//    u32  next;
//};
//
//struct bxStaticVoxelGrid
//{
//    void create( int nX, int nY, int nZ, bxAllocator* allocator = bxDefaultAllocator() );
//    void release();
//
//    void 
//
//    bxAllocator* _allocator;
//    bxVoxelGrid_Item* _items;
//    i32 _nX;
//    i32 _nY;
//    i32 _nZ;
//};

static bxGdiShaderFx_Instance* fxI = 0;
static bxGdiRenderSource* rsource = 0;
static bxGdiBuffer voxelDataBuffer;
static const u32 GRID_SIZE = 512;


struct bxVoxelFramebuffer
{
    enum
    {
        eCOLOR = 0,
        eDEPTH,
        eCOUNT,
    };

    bxGdiTexture textures[eCOUNT];

    int width()  const { return textures[0].width; }
    int height() const { return textures[0].height; }
};

struct bxVoxelObject
{
    u16 width;
    u16 height;
    u16 depth;
};





static bxVoxelFramebuffer fb;
static bxGfxCamera camera;
static bxGfxCamera_InputContext cameraInputCtx;
class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        //testBRDF();
        
        bxVoxel_Octree* octree = bxVoxel::octree_new( 128 );

        bxVoxel::octree_delete( &octree );

        bxDemoEngine_startup( &_engine );
        
        fxI = bxGdi::shaderFx_createWithInstance( _engine.gdiDevice, _engine.resourceManager, "voxel" );
        rsource = _engine.gfxContext->shared()->rsource.box;

        voxelDataBuffer = _engine.gdiDevice->createBuffer( (GRID_SIZE*GRID_SIZE*GRID_SIZE), bxGdiFormat( bxGdi::eTYPE_UINT, 2 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );

        const int fbWidth = 1920;
        const int fbHeight = 1080;
        fb.textures[bxVoxelFramebuffer::eCOLOR] = _engine.gdiDevice->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET|bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        fb.textures[bxVoxelFramebuffer::eDEPTH] = _engine.gdiDevice->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL|bxGdi::eBIND_SHADER_RESOURCE );

        //__simpleScene = BX_NEW( bxDefaultAllocator(), bxDemoSimpleScene );
        //bxDemoSimpleScene_startUp( &_engine, __simpleScene );
        
        //camera.matrix.world = Matrix4( Matrix3::identity(), Vector3( 0.f, 0.f, 150.f ) );
        camera.matrix.world = inverse( Matrix4::lookAt( Point3( 0.f, 10.f, 50.f ), Point3(0.f), Vector3::yAxis() ) );
        time = 0.f;
        return true;
    }
    virtual void shutdown()
    {
        for( int ifb = 0; ifb < bxVoxelFramebuffer::eCOUNT; ++ifb )
        {
            _engine.gdiDevice->releaseTexture( &fb.textures[ifb] );
        }
        _engine.gdiDevice->releaseBuffer( &voxelDataBuffer );
        rsource = 0;
        bxGdi::shaderFx_releaseWithInstance( _engine.gdiDevice, &fxI );
        //bxDemoSimpleScene_shutdown( &_engine, __simpleScene );
        //BX_DELETE0( bxDefaultAllocator(), __simpleScene );
        bxDemoEngine_shutdown( &_engine );

    }
    virtual bool update( u64 deltaTimeUS )
    {
        const double deltaTimeS = bxTime::toSeconds( deltaTimeUS );
        const float deltaTime = (float)deltaTimeS;
        
        bxWindow* win = bxWindow_get();
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        bxGfxGUI::newFrame( (float)deltaTimeS );

        {
            ImGui::Begin( "System" );
            ImGui::Text( "deltaTime: %.5f", deltaTime );
            ImGui::Text( "FPS: %.5f", 1.f / deltaTime );
            ImGui::End();
        }

        bxGfxCamera* currentCamera = &camera;

        bxGfx::cameraUtil_updateInput( &cameraInputCtx, &win->input, 1.f, deltaTime );
        currentCamera->matrix.world = bxGfx::cameraUtil_movement( currentCamera->matrix.world
            , cameraInputCtx.leftInputX * 0.5f
            , cameraInputCtx.leftInputY * 0.5f
            , cameraInputCtx.rightInputX * deltaTime * 10.f
            , cameraInputCtx.rightInputY * deltaTime * 10.f
            , cameraInputCtx.upDown * 0.5f );

        bxGfx::cameraMatrix_compute( &currentCamera->matrix, currentCamera->params, currentCamera->matrix.world, 0, 0 );

        fxI->setUniform( "viewProj", currentCamera->matrix.viewProj );

        bxGfxContext* gfxContext = _engine.gfxContext;
        bxGdiContext* gdiContext = _engine.gdiContext;

        bxGrid grid( GRID_SIZE,GRID_SIZE,GRID_SIZE );

        const int N_VOXELS_X = 128;
        const int N_VOXELS_Y = 128;
        const int N_VOXELS_Z = 128;
        const int N_VOXELS = N_VOXELS_X * N_VOXELS_Y * N_VOXELS_Z;
        static array_t<bxVoxel_GpuData> vxData;
        static bxVoxel_Octree* vxOctree = 0;
        static int nValidVoxels = 0;
        if( array::empty( vxData ) )
        {
            vxOctree = bxVoxel::octree_new( 32 );
            //vxData = new VoxelData[N_VOXELS];

            const u32 colors[] = 
            {
                0xFF0000FF, 0x00FF00FF, 0x0000FFFF,
                0xFFFF00FF, 0xFF00FFFF, 0x00FFFFFF,
                0xFFFFFFFF, 0xF0F0F0FF, 0x0F0F0FFF,
                0xFF0FF0FF, 0x0FF0FFFF, 0xFFF00FFF,
                0xFF000FFF, 0xF000FFFF, 0x000FFFFF
            };
            const int nColors =sizeof(colors)/sizeof(*colors);

            const float cellSize = 5.5f;
            const Vector3 divider = Vector3( 1.f / GRID_SIZE );

            int counter = 0;
            const float angleXStep = PI2 / (N_VOXELS_X - 1);
            const float angleYStep = PI / (N_VOXELS_X - 1);
            const float radius = 15.f;
            const float center = 15.f;
            float zAngle = 0.f;
            for( int iz = 1; iz < (int)radius; ++iz )
            {
                float yAngle = 0.1f;
                for( int iy = 0; iy <= N_VOXELS_Y; ++iy )
                {
                    float xAngle = 0.1f;
                    for( int ix = 0; ix <= N_VOXELS_X; ++ix )
                    {
                        const float x = center + iz * cos( xAngle ) * sin( yAngle );
                        const float y = center + iz * sin( xAngle ) * sin( yAngle );
                        const float z = center + iz * cos( yAngle );
                        
                        //const Vector3 rndOffset = Vector3( bxNoise_perlin( ix * cellSize + time, iy * cellSize + time, iz * cellSize  + time) ) * cellSize;
                        //const Vector3 rndOffset1 = mulPerElem( rndOffset, divider );
                    
                        //SSEScalar scalar( rndOffset.get128() );
                        
                        //const Vector3 point( ix * cellSize, iy * cellSize, iz * cellSize );
                        const Vector3 point( x, y, z );
                        const u32 color = colors[ counter % nColors ];
                        //bxGfxDebugDraw::addSphere( Vector4( point, 0.1f ), 0xFF00FF00, true );
                        bxVoxel::octree_insert( vxOctree, point, color );
                        
                        //vxData[counter].gridIndex = grid.index( ix * cellSize, iy * cellSize, iz * cellSize ) ; // + grid.index( (int)scalar.x * cellSize, (int)scalar.y * cellSize, (int)scalar.z*cellSize );
                        //vxData[counter].gridIndex = grid.index( (int)x, (int)y, (int)z );
                        //vxData[counter].colorRGBA = colors[ counter % nColors ];
                        ++counter;
                        xAngle += angleXStep;
                    }
                    yAngle += angleYStep;
                }
                //zAngle += angleStep;
            }
            nValidVoxels = counter;

            bxVoxel::octree_getShell( vxData, vxOctree );
            if( !array::empty( vxData ) )
            {
                u8* mappedVoxelDataBuffer = bxGdi::buffer_map( _engine.gdiContext->backend(), voxelDataBuffer, 0, counter );
                memcpy( mappedVoxelDataBuffer, array::begin( vxData ), sizeof(bxVoxel_GpuData) * array::size( vxData ) );
                gdiContext->backend()->unmap( voxelDataBuffer.rs );    
            }
            
        }
        
        bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::xAxis(), 0xFF0000FF, true );
        bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::yAxis(), 0x00FF00FF, true );
        bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::zAxis(), 0x0000FFFF, true );
        //bxVoxel::octree_debugDraw( vxOctree );


        gdiContext->clear();
        gdiContext->changeRenderTargets( &fb.textures[0], 1, fb.textures[bxVoxelFramebuffer::eDEPTH] );
        gdiContext->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 1, 1 );
        gdiContext->setBufferRO( voxelDataBuffer, 0, bxGdi::eSTAGE_MASK_VERTEX );
        bxGdi::context_setViewport( gdiContext, fb.textures[0] );
        
        bxGdi::shaderFx_enable( gdiContext, fxI, 0 );
        bxGdi::renderSource_enable( gdiContext, rsource );
        
        const bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
        bxGdi::renderSurface_drawIndexedInstanced( gdiContext, surf, array::size( vxData ) );

        

        bxGfxDebugDraw::flush( gdiContext, camera.matrix.viewProj );
        gfxContext->frame_rasterizeFramebuffer( gdiContext, fb.textures[bxVoxelFramebuffer::eCOLOR], *currentCamera );

        
        bxGfxGUI::draw( gdiContext );
        gdiContext->backend()->swap();


        //gfxContext->frame_begin( _engine.gdiContext );

        


        //gfxContext->frame_end( _engine.gdiContext );


        //bxDemoSimpleScene_frame( win, &_engine, __simpleScene, deltaTimeUS );
        //bxDemoEngine_frameDraw( win, &_engine, __simpleScene->rList, *__simpleScene->currentCamera_, deltaTime );
        
        time += deltaTime;

        return true;
    }
    float time;
    bxDemoEngine _engine;
};

int main( int argc, const char* argv[] )
{
    bxWindow* window = bxWindow_create( "demo", 1280, 720, false, 0 );
    if( window )
    {
        bxDemoApp app;
        if( bxApplication_startup( &app, argc, argv ) )
        {
            bxApplication_run( &app );
        }

        bxApplication_shutdown( &app );
        bxWindow_release();
    }
    
    
    return 0;
}