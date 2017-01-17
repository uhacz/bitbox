#include "ship_game.h"
#include "../game_util.h"
#include <util\memory.h>
#include <resource_manager\resource_manager.h>
#include "util\string_util.h"
#include "system\window.h"
#include "../../rdi/rdi_debug_draw.h"

namespace bx{
namespace ship{


ShipGame::ShipGame(){}
ShipGame::~ShipGame(){}

void ShipGame::StartUpImpl()
{
    ResourceManager* resource_manager = GResourceManager();
    {
        gfx::RendererDesc rdesc = {};
        rdesc.framebuffer_width = 1920;
        rdesc.framebuffer_height = 1080;
        _gfx.renderer.StartUp( rdesc, resource_manager );

        gfx::GeometryPass::_StartUp   ( &_gfx.geometry_pass );
        gfx::ShadowPass::_StartUp     ( &_gfx.shadow_pass, _gfx.renderer.GetDesc(), 1024 * 8 );
        gfx::SsaoPass::_StartUp       ( &_gfx.ssao_pass  , _gfx.renderer.GetDesc(), false );
        gfx::LightPass::_StartUp      ( &_gfx.light_pass );
        gfx::PostProcessPass::_StartUp( &_gfx.post_pass );
    }

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
    gfx::PostProcessPass::_ShutDown( &_gfx.post_pass );
    gfx::LightPass::_ShutDown      ( &_gfx.light_pass );
    gfx::SsaoPass::_ShutDown       ( &_gfx.ssao_pass );
    gfx::ShadowPass::_ShutDown     ( &_gfx.shadow_pass );
    gfx::GeometryPass::_ShutDown   ( &_gfx.geometry_pass );
    _gfx.renderer.ShutDown         ( bx::GResourceManager() );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void Level::StartUp( Gfx* gfx, const char* levelName )
{
    _name = string::duplicate( (char*)_name, levelName );
    _gfx_scene = gfx->renderer.CreateScene( levelName );
    _gfx_scene->EnableSunSkyLight();
    _gfx_scene->GetSunSkyLight()->sun_direction = normalize( Vector3( -1.f, -1.0f, 0.f ) );
    _gfx_scene->GetSunSkyLight()->sky_cubemap = gfx::GTextureManager()->CreateFromFile( "texture/sky1_cubemap.dds" );

    {
        gfx::ActorID actor = _gfx_scene->Add( "ground", 1 );
        _gfx_scene->SetMaterial( actor, gfx::GMaterialManager()->Find( "grey" ) );
        _gfx_scene->SetMesh( actor, gfx::GMeshManager()->Find( ":box" ) );

        Matrix4 pose = Matrix4::translation( Vector3( 0.f, -1.0f, 0.f ) );
        pose = appendScale( pose, Vector3( 100.f, 1.f, 100.f ) );

        _gfx_scene->SetMatrices( actor, &pose, 1 );
    }
}

void Level::ShutDown( Gfx* gfx )
{
    gfx->renderer.DestroyScene( &_gfx_scene );
    string::free( (char*)_name );
    _name = nullptr;


}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LevelState::OnStartUp()
{
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
    if( _use_dev_camera )
    {
        game_util::DevCameraCollectInput( &_dev_camera_input_ctx, time.DeltaTimeSec() );
        const Matrix4 new_camera_world = _dev_camera_input_ctx.computeMovement( _dev_camera.world, 0.15f );
        gfx::computeMatrices( &_dev_camera );
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
    
    gfx::Scene gfx_scene = _level->_gfx_scene;

    // ---
    _gfx->renderer.BeginFrame( cmdq );

    _gfx->geometry_pass.PrepareScene( cmdq, gfx_scene, *active_camera );
    _gfx->geometry_pass.Flush( cmdq );

    rdi::TextureDepth depthTexture = rdi::GetTextureDepth( _gfx->geometry_pass.GBuffer() );
    rdi::TextureRW normalsTexture = rdi::GetTexture( _gfx->geometry_pass.GBuffer(), 2 );
    _gfx->shadow_pass.PrepareScene( cmdq, gfx_scene, *active_camera );
    _gfx->shadow_pass.Flush( cmdq, depthTexture, normalsTexture );

    _gfx->ssao_pass.PrepareScene( cmdq, *active_camera );
    _gfx->ssao_pass.Flush( cmdq, depthTexture, normalsTexture );

    _gfx->light_pass.PrepareScene( cmdq, gfx_scene, *active_camera );
    _gfx->light_pass.Flush( cmdq,
        _gfx->renderer.GetFramebuffer( gfx::EFramebuffer::SWAP ),
        _gfx->geometry_pass.GBuffer(),
        _gfx->shadow_pass.ShadowMap(),
        _gfx->ssao_pass.SsaoTexture() );

    rdi::TextureRW srcColor = _gfx->renderer.GetFramebuffer( gfx::EFramebuffer::SWAP );
    rdi::TextureRW dstColor = _gfx->renderer.GetFramebuffer( gfx::EFramebuffer::COLOR );
    _gfx->post_pass.DoToneMapping( cmdq, dstColor, srcColor, time.DeltaTimeSec() );

    rdi::debug_draw::AddAxes( Matrix4::identity() );

    gfx::Renderer::DebugDraw( cmdq, dstColor, depthTexture, *active_camera );
    

    rdi::ResourceRO* toRasterize[] =
    {
        &dstColor,
        &_gfx->ssao_pass.SsaoTexture(),
        &_gfx->shadow_pass.ShadowMap(),
        &_gfx->shadow_pass.DepthMap(),
    };
    const int toRasterizeN = sizeof( toRasterize ) / sizeof( *toRasterize );
    static int dstColorSelect = 0;

    bxWindow* win = bxWindow_get();
    if( bxInput_isKeyPressedOnce( &win->input.kbd, ' ' ) )
    {
        dstColorSelect = ( dstColorSelect + 1 ) % toRasterizeN;
    }

    rdi::ResourceRO texture = *toRasterize[dstColorSelect];
    _gfx->renderer.RasterizeFramebuffer( cmdq, texture, *active_camera, win->width, win->height );
    _gfx->renderer.EndFrame( cmdq );

}



}
}//

