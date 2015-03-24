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




class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        //testBRDF();
        
        bxDemoEngine_startup( &_engine );
        
        __simpleScene = BX_NEW( bxDefaultAllocator(), bxDemoSimpleScene );
        bxDemoSimpleScene_startUp( &_engine, __simpleScene );
        
        return true;
    }
    virtual void shutdown()
    {
        bxDemoSimpleScene_shutdown( &_engine, __simpleScene );
        BX_DELETE0( bxDefaultAllocator(), __simpleScene );
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

        bxDemoSimpleScene_frame( win, &_engine, __simpleScene, deltaTimeUS );
        bxDemoEngine_frameDraw( win, &_engine, __simpleScene->rList, *__simpleScene->currentCamera_, deltaTime );
        
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