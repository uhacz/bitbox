#include "ship_game.h"
#include "..\game_util.h"

#include <util\memory.h>
#include <util\string_util.h>
#include <system\window.h>
#include <resource_manager\resource_manager.h>
#include <rdi\rdi_debug_draw.h>

#include "ship_level.h"
#include "..\imgui\imgui.h"
#include "..\imgui\imgui_impl_dx11.h"

namespace bx{
namespace ship{


ShipGame::ShipGame(){}
ShipGame::~ShipGame(){}

static int ImGui_WinMsgHandler( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    ImGuiIO& io = ImGui::GetIO();
    switch( msg )
    {
    case WM_LBUTTONDOWN:
        io.MouseDown[0] = true;
        break;
    case WM_LBUTTONUP:
        io.MouseDown[0] = false;
        break;
    case WM_RBUTTONDOWN:
        io.MouseDown[1] = true;
        break;
    case WM_RBUTTONUP:
        io.MouseDown[1] = false;
        break;
    case WM_MOUSEWHEEL:
        io.MouseWheel += GET_WHEEL_DELTA_WPARAM( wParam ) > 0 ? +1.0f : -1.0f;
        break;
    case WM_MOUSEMOVE:
        // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        io.MousePos.x = (signed short)( lParam );
        io.MousePos.y = (signed short)( lParam >> 16 );
        break;
    case WM_CHAR:
        // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
        if( wParam > 0 && wParam < 0x10000 )
            io.AddInputCharacter( (unsigned short)wParam );
        break;
    }

    const bool isHovering = ImGui::IsMouseHoveringAnyWindow();
    return ( isHovering ) ? 1 : 0;
}

void ShipGame::StartUpImpl()
{
    {
        ID3D11Device* dev = nullptr;
        ID3D11DeviceContext* ctx = nullptr;
        rdi::device::GetAPIDevice( &dev, &ctx );
        ImGui_ImplDX11_Init( bxWindow_get()->hwnd, dev, ctx );
        bxWindow_addWinMsgCallback( bxWindow_get(), ImGui_WinMsgHandler );
    }

    GameGfxStartUp( &_gfx );

    ResourceManager* resource_manager = GResourceManager();

    {
        gfx::MaterialDesc mat_desc;
        mat_desc.data.diffuse_color = float3_t( 1.f, 0.f, 0.f );
        mat_desc.data.diffuse = 0.5f;
        mat_desc.data.roughness = 0.01f;
        mat_desc.data.specular = 0.9f;
        mat_desc.data.metallic = 0.0f;
        gfx::GMaterialManager()->Create( "red", mat_desc );

        mat_desc.data.diffuse_color = float3_t( 0.f, 1.f, 0.f );
        mat_desc.data.roughness = 0.21f;
        mat_desc.data.specular = 0.91f;
        mat_desc.data.metallic = 0.0f;
        gfx::GMaterialManager()->Create( "green", mat_desc );

        mat_desc.data.diffuse_color = float3_t( 0.f, 0.f, 1.f );
        mat_desc.data.roughness = 0.91f;
        mat_desc.data.specular = 0.19f;
        mat_desc.data.metallic = 0.0f;
        gfx::GMaterialManager()->Create( "blue", mat_desc );


        mat_desc.data.diffuse_color = float3_t( 0.4f, 0.4f, 0.4f );
        mat_desc.data.roughness = 0.5f;
        mat_desc.data.specular = 0.5f;
        mat_desc.data.metallic = 0.0f;
        gfx::GMaterialManager()->Create( "grey", mat_desc );
    }

    LevelState* level_state = BX_NEW( bxDefaultAllocator(), LevelState, &_gfx );
    GameStateId level_state_id = AddState( level_state );
    PushState( level_state_id );
}


void ShipGame::ShutDownImpl()
{
    GameGfxShutDown( &_gfx );
    {
        bxWindow_removeWinMsgCallback( bxWindow_get(), ImGui_WinMsgHandler );
        ImGui_ImplDX11_Shutdown();
    }
}

bool ShipGame::PreUpdateImpl( const GameTime& time )
{
    ImGui_ImplDX11_NewFrame();
    return true;
}

void ShipGame::PreRenderImpl( const GameTime& time, rdi::CommandQueue* cmdq )
{
    _gfx.renderer.BeginFrame( cmdq );
}

void ShipGame::PostRenderImpl( const GameTime& time, rdi::CommandQueue* cmdq )
{
    ImGui::Render();
    _gfx.renderer.EndFrame( cmdq );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LevelState::OnStartUp()
{
    _dev_camera.params.zFar = 1000.f;
    
    _level = BX_NEW( bxDefaultAllocator(), Level );
    _level->StartUp( _gfx, "level" );
}

void LevelState::OnShutDown()
{
    if( _level )
    {
        _level->ShutDown( _gfx );
        BX_DELETE0( bxDefaultAllocator(), _level );
    }
}

void LevelState::OnUpdate( const GameTime& time )
{


    bxWindow* win = bxWindow_get();
    if( bxInput_isKeyPressedOnce( &win->input.kbd, '1' ) )
    {
        _use_dev_camera = !_use_dev_camera;
    }
    if( _use_dev_camera )
    {
        game_util::DevCameraCollectInput( &_dev_camera_input_ctx, time.DeltaTimeSec(), 0.01f );
        _dev_camera.world = _dev_camera_input_ctx.computeMovement( _dev_camera.world, 0.15f );

        gfx::computeMatrices( &_dev_camera );
    }
    
    if( _level )
    {
        _level->Tick( time );
    }
}

void LevelState::OnRender( const GameTime& time, rdi::CommandQueue* cmdq )
{
    gfx::Camera* active_camera = nullptr;
    if( _use_dev_camera )
    {
        active_camera = &_dev_camera;
    }
       
    if( !_level )
        return;
    
    if( !active_camera )
    {
        active_camera = &_level->_player_camera._camera;
    }

    rdi::debug_draw::AddAxes( Matrix4::identity() );

    gfx::Scene gfx_scene = _level->_gfx_scene;

    // ---
    GameGfxDrawScene( cmdq, _gfx, gfx_scene, *active_camera );
    GameGfxPostProcess( cmdq, _gfx, *active_camera, time.DeltaTimeSec() );
    GameGfxRasterize( cmdq, _gfx, *active_camera );
}

}
}//

