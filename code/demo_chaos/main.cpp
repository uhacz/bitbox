#include <system/application.h>
#include <system/window.h>

//#include <engine/engine.h>

//#include <util/time.h>
//#include <util/handle_manager.h>
#include <util/config.h>
#include <resource_manager/resource_manager.h>

//#include <gfx/gfx_camera.h>
//#include <gfx/gfx_debug_draw.h>
//#include <rdi/rdi.h>
//#include <rdi/rdi_debug_draw.h>

//#include <gfx/gfx_gui.h>
//#include <gdi/gdi_shader.h>

//#include "scene.h"
//#include "renderer.h"
//#include "renderer_scene.h"
//#include "util/poly/poly_shape.h"
//#include "renderer_texture.h"

#include "test_game/test_game.h"
#include "ship_game/ship_game.h"
#include "flood_game/flood_game.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
using namespace bx;

// void bx_purecall_handler()
// {
//     SYS_ASSERT( false );
// }

class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bxConfig::global_init( "demo_chaos/global.cfg" );
        const char* assetDir = bxConfig::global_string( "assetDir" );
        bx::ResourceManager::startup( assetDir );
        
        bxWindow* win = bxWindow_get();
        rdi::Startup( (uptr)win->hwnd, win->width, win->height, win->full_screen );
        


        //bxAsciiScript sceneScript;
        //if( _engine._camera_script_callback )
        //    _engine._camera_script_callback->addCallback( &sceneScript );

        //const char* sceneName = bxConfig::global_string( "scene" );
        //bxFS::File scriptFile = _engine.resource_manager->readTextFileSync( sceneName );

        //if( scriptFile.ok() )
        //{
        //    bxScene::script_run( &sceneScript, scriptFile.txt );
        //}
        //scriptFile.release();

        //{
        //    char const* cameraName = bxConfig::global_string( "camera" );
        //    if( cameraName )
        //    {
        //        bx::GfxCamera* camera = _engine.camera_manager->find( cameraName );
        //        if( camera )
        //        {
        //            _engine.camera_manager->stack()->push( camera );
        //            _camera = camera;
        //        }
        //    }
        //}

        //_game = BX_NEW( bxDefaultAllocator(), bx::ship::ShipGame );
        _game = BX_NEW( bxDefaultAllocator(), bx::flood::FloodGame );
        _game->StartUp();

        return true;
    }
    virtual void shutdown()
    {
        //_gfx_scene->Remove( &_spheres );
        //_gfx_scene->Remove( &_boxes );

        _game->ShutDown();
        BX_DELETE0( bxDefaultAllocator(), _game );

        rdi::Shutdown();
        ResourceManager::shutdown();
        bxConfig::global_deinit();
    }

    virtual bool update( u64 deltaTimeUS )
    {
        //rmt_BeginCPUSample( FRAME );
        //const double deltaTimeS = bxTime::toSeconds( deltaTimeUS );
        //const float deltaTime = (float)deltaTimeS;

        //bxWindow* win = bxWindow_get();
        //if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        //{
        //    return false;
        //}


        //const bool useDebugCamera = true; // bxInput_isKeyPressed( &win->input.kbd, bxInput::eKEY_CAPSLOCK );

        ////bxGfxGUI::newFrame( (float)deltaTimeS );

        ////{
        ////    ImGui::Begin( "System" );
        ////    ImGui::Text( "deltaTime: %.5f", deltaTime );
        ////    ImGui::Text( "FPS: %.5f", 1.f / deltaTime );
        ////    ImGui::End();
        ////}

        //rmt_BeginCPUSample( FRAME_UPDATE );

        //{
        //    bxInput* input = &win->input;
        //    bxInput_Mouse* inputMouse = &input->mouse;
        //    bxInput_PadState* inputPad = input->pad.currentState();

        //    if( inputPad->connected && !useDebugCamera )
        //    {
        //        const float sensitivity = 3.f;
        //        _camera_input_ctx.updateInput( -inputPad->analog.right_X * sensitivity, inputPad->analog.right_Y * sensitivity, deltaTime );
        //    }
        //    else
        //    {
        //        _camera_input_ctx.updateInput( inputMouse->currentState()->lbutton
        //                                       , inputMouse->currentState()->mbutton
        //                                       , inputMouse->currentState()->rbutton
        //                                       , inputMouse->currentState()->dx
        //                                       , inputMouse->currentState()->dy
        //                                       , 0.01f
        //                                       , deltaTime );
        //    }
        //    const Matrix4 new_camera_world = _camera_input_ctx.computeMovement( _camera.world, 0.15f );
        //    _camera.world = new_camera_world;
        //}

        //gfx::computeMatrices( &_camera );

        //rdi::CommandQueue* cmdq = nullptr;
        //rdi::frame::Begin( &cmdq );
        //_renderer.BeginFrame( cmdq );

        //_geometry_pass.PrepareScene( cmdq, _gfx_scene, _camera );
        //_geometry_pass.Flush( cmdq );

        //rdi::TextureDepth depthTexture = rdi::GetTextureDepth( _geometry_pass.GBuffer() );
        //rdi::TextureRW normalsTexture = rdi::GetTexture( _geometry_pass.GBuffer(), 2 );
        //_shadow_pass.PrepareScene( cmdq, _gfx_scene, _camera );
        //_shadow_pass.Flush( cmdq, depthTexture, normalsTexture );

        //_ssao_pass.PrepareScene( cmdq, _camera );
        //_ssao_pass.Flush( cmdq, depthTexture, normalsTexture );

        //_light_pass.PrepareScene( cmdq, _gfx_scene, _camera );
        //_light_pass.Flush( cmdq, _renderer.GetFramebuffer( gfx::EFramebuffer::SWAP ), _geometry_pass.GBuffer(), _shadow_pass.ShadowMap(), _ssao_pass.SsaoTexture() );

        //rdi::TextureRW srcColor = _renderer.GetFramebuffer( gfx::EFramebuffer::SWAP );
        //rdi::TextureRW dstColor = _renderer.GetFramebuffer( gfx::EFramebuffer::COLOR );
        //_post_pass.DoToneMapping( cmdq, dstColor, srcColor, deltaTime );

        //rdi::debug_draw::AddAxes( Matrix4::identity() );

        //gfx::Renderer::DebugDraw( cmdq, dstColor, depthTexture, _camera );
        ////rdi::debug_draw::_Flush( cmdq, _camera.view, _camera.proj );

        //rdi::ResourceRO* toRasterize[] =
        //{
        //    &dstColor,
        //    &_ssao_pass.SsaoTexture(),
        //    &_shadow_pass.ShadowMap(),
        //    &_shadow_pass.DepthMap(),
        //};
        //const int toRasterizeN = sizeof( toRasterize ) / sizeof( *toRasterize );
        //static int dstColorSelect = 0;
        //if( bxInput_isKeyPressedOnce( &win->input.kbd, ' ' ) )
        //{
        //    dstColorSelect = (dstColorSelect + 1) % toRasterizeN;
        //}
        ////rdi::TextureRW texture = rdi::GetTexture( _geometry_pass.GBuffer(), 2 );
        ////rdi::TextureRW texture = _post_pass._tm.initial_luminance;
        ////rdi::TextureRW texture = dstColor;
        ////rdi::ResourceRO texture = _shadow_pass.DepthMap();
        //rdi::ResourceRO texture = *toRasterize[dstColorSelect];
        //_renderer.RasterizeFramebuffer( cmdq, texture, _camera, win->width, win->height );

        //
        //_renderer.EndFrame( cmdq );

        //rdi::frame::End( &cmdq );
        //

        //rmt_EndCPUSample();

        ////rmt_BeginCPUSample( FRAME_DRAW );
        ////rmt_BeginCPUSample( scene );
        ////rmt_EndCPUSample();

        ////rmt_BeginCPUSample( gui );
        //////bxGfxGUI::draw( _engine.gdi_context );
        ////rmt_EndCPUSample();
        ////rmt_EndCPUSample();

        ////bx::gfxContextFrameEnd( _engine.gfx_context, _engine.gdi_context );
        ////bx::gfxCommandQueueRelease( &cmdq );

        //_time += deltaTime;
        //rmt_EndCPUSample();

        bxWindow* win = bxWindow_get();
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        rmt_ScopedCPUSample( MainLoop, 0 );

        bool is_game_running = _game->Update();
        if( is_game_running )
        {
            _game->Render();
        }
        return is_game_running;
    }
    bx::Game* _game = nullptr;
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