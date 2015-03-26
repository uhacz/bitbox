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
struct VoxelData
{
    u32 gridIndex;
    u32 colorRGBA;
};

struct Grid
{
    u32 width;
    u32 height;
    u32 depth;

    f32 scale_x;
    f32 scale_y;
    f32 scale_z;

    //
    Grid()
        : width(0), height(0), depth(0)
        , scale_x(1.f), scale_y(1.f), scale_z(1.f)
    {}
        
    void setSize( unsigned w, unsigned h, unsigned d )
    {
        width = w;
        height = h;
        depth = d;
    }


    //
    int numCells() 
    { 
        return width * height * depth; 
    }
    int index( unsigned x, unsigned y, unsigned z )
    {
        return x + y * width + z*width*height;    
    }
    void coords( unsigned xyz[3], unsigned idx )
    {
        const int wh = width*height;
        const int index_mod_wh = idx % wh;
        xyz[0] = index_mod_wh % width;
        xyz[1] = index_mod_wh / width;
        xyz[2] = idx / wh;
    }
};

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
        
        bxVoxelOctree* octree = bxVoxel::octree_new( 128 );

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
        camera.matrix.world = inverse( Matrix4::lookAt( Point3( -30.f, 10.f, 150.f ), Point3(0.f), Vector3::yAxis() ) );
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
            , cameraInputCtx.leftInputX * 0.25f
            , cameraInputCtx.leftInputY * 0.25f
            , cameraInputCtx.rightInputX * deltaTime * 5.f
            , cameraInputCtx.rightInputY * deltaTime * 5.f
            , cameraInputCtx.upDown * 0.25f );

        bxGfx::cameraMatrix_compute( &currentCamera->matrix, currentCamera->params, currentCamera->matrix.world, 0, 0 );

        fxI->setUniform( "viewProj", currentCamera->matrix.viewProj );

        
        Grid grid;
        grid.setSize( GRID_SIZE,GRID_SIZE,GRID_SIZE );


        const int N_VOXELS_X = 100;
        const int N_VOXELS_Y = 100;
        const int N_VOXELS_Z = 100;
        const int N_VOXELS = N_VOXELS_X * N_VOXELS_Y * N_VOXELS_Z;
        static VoxelData* vxData = new VoxelData[N_VOXELS];

        const u32 colors[] = 
        {
            0xFF0000FF, 0x00FF00FF, 0x0000FFFF,
            0xFFFF00FF, 0xFF00FFFF, 0x00FFFFFF,
            0xFFFFFFFF, 0xF0F0F0FF, 0x0F0F0FFF,
            0xFF0FF0FF, 0x0FF0FFFF, 0xFFF00FFF,
            0xFF000FFF, 0xF000FFFF, 0x000FFFFF
        };
        const int nColors =sizeof(colors)/sizeof(*colors);

        const float cellSize = 10.5f;
        const Vector3 divider = Vector3( 1.f / GRID_SIZE );

        int counter = 0;
        for( int iz = 0; iz < N_VOXELS_Z; ++iz )
        {
            for( int iy = 0; iy < N_VOXELS_Y; ++iy )
            {
                for( int ix = 0; ix < N_VOXELS_X; ++ix )
                {
                    const Vector3 rndOffset = Vector3( bxNoise_perlin( ix * cellSize + time, iy * cellSize + time, iz * cellSize  + time) ) * cellSize;
                    //const Vector3 rndOffset1 = mulPerElem( rndOffset, divider );
                    
                    SSEScalar scalar( rndOffset.get128() );
                    

                    vxData[counter].gridIndex = grid.index( ix, iy, iz ) + grid.index( (int)scalar.x, (int)scalar.y, (int)scalar.z*10.f );
                    vxData[counter].colorRGBA = colors[ counter % nColors ];
                    ++counter;
                }
            }
        }

        u8* mappedVoxelDataBuffer = bxGdi::buffer_map( _engine.gdiContext->backend(), voxelDataBuffer, 0, N_VOXELS );
        memcpy( mappedVoxelDataBuffer, vxData, sizeof(VoxelData) * N_VOXELS );
        
        bxGfxContext* gfxContext = _engine.gfxContext;
        bxGdiContext* gdiContext = _engine.gdiContext;

        gdiContext->backend()->unmap( voxelDataBuffer.rs );

        bxGfxGUI::newFrame( deltaTime );

        gdiContext->clear();
        gdiContext->changeRenderTargets( &fb.textures[0], 1, fb.textures[bxVoxelFramebuffer::eDEPTH] );
        gdiContext->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 1, 1 );
        gdiContext->setBufferRO( voxelDataBuffer, 0, bxGdi::eSTAGE_MASK_VERTEX );
        bxGdi::context_setViewport( gdiContext, fb.textures[0] );
        
        bxGdi::shaderFx_enable( gdiContext, fxI, 0 );
        bxGdi::renderSource_enable( gdiContext, rsource );
        
        const bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
        bxGdi::renderSurface_drawIndexedInstanced( gdiContext, surf, N_VOXELS );

        bxGfxGUI::draw( gdiContext );

        gfxContext->frame_rasterizeFramebuffer( gdiContext, fb.textures[bxVoxelFramebuffer::eCOLOR], *currentCamera );

        

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