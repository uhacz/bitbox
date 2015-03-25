#include <system/application.h>

#include <util/time.h>
#include <util/handle_manager.h>

#include "test_console.h"
#include "entity.h"
//#include "scene.h"

#include "demo_engine.h"
#include "simple_scene.h"


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
static const u32 GRID_SIZE = 1024;
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
class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        //testBRDF();
        
        bxDemoEngine_startup( &_engine );
        
        fxI = bxGdi::shaderFx_createWithInstance( _engine.gdiDevice, _engine.resourceManager, "voxel" );
        rsource = _engine.gfxContext->shared()->rsource.box;

        voxelDataBuffer = _engine.gdiDevice->createBuffer( GRID_SIZE*GRID_SIZE*GRID_SIZE, bxGdiFormat( bxGdi::eTYPE_UINT, 2 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );

        //__simpleScene = BX_NEW( bxDefaultAllocator(), bxDemoSimpleScene );
        //bxDemoSimpleScene_startUp( &_engine, __simpleScene );
        
        return true;
    }
    virtual void shutdown()
    {
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


        
        Grid grid;
        grid.setSize( GRID_SIZE,GRID_SIZE,GRID_SIZE );

        VoxelData vxData;
        vxData.gridIndex = grid.index( 1, 1, 1 );
        vxData.colorRGBA = 0xFF0000FF;
       
        u8* mappedVoxelDataBuffer = bxGdi::buffer_map( _engine.gdiContext->backend(), voxelDataBuffer, 0, 1 );
        memcpy( mappedVoxelDataBuffer, &vxData, sizeof(VoxelData) );
        _engine.gdiContext->backend()->unmap( voxelDataBuffer.rs );

        _engine.gfxContext->frame_begin( _engine.gdiContext );

        _engine.gfxContext->frame_end( _engine.gdiContext );


        //bxDemoSimpleScene_frame( win, &_engine, __simpleScene, deltaTimeUS );
        //bxDemoEngine_frameDraw( win, &_engine, __simpleScene->rList, *__simpleScene->currentCamera_, deltaTime );
        
        return true;
    }

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