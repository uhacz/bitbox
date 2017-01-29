#include "test_game.h"
#include <resource_manager\resource_manager.h>
#include <util\common.h>
#include <system\window.h>
#include <rdi\rdi_debug_draw.h>

namespace bx
{

TestGame::TestGame()
{
}
TestGame::~TestGame()
{
}

void TestGame::StartUpImpl()
{
    gfx::RendererDesc renderer_desc = {};
    _data.renderer.StartUp( renderer_desc, bx::GResourceManager() );

    gfx::GeometryPass::_StartUp   ( &_data.geometry_pass, _data.renderer.GetDesc() );
    gfx::ShadowPass::_StartUp     ( &_data.shadow_pass, _data.renderer.GetDesc(), 1024 * 8 );
    gfx::SsaoPass::_StartUp       ( &_data.ssao_pass, _data.renderer.GetDesc(), false );
    gfx::LightPass::_StartUp      ( &_data.light_pass );
    gfx::PostProcessPass::_StartUp( &_data.post_pass );
    
    GameStateId id = AddState( new TestMainState( &_data ) );
    PushState( id );    
}
void TestGame::ShutDownImpl()
{
    gfx::PostProcessPass::_ShutDown( &_data.post_pass );
    gfx::LightPass::_ShutDown      ( &_data.light_pass );
    gfx::SsaoPass::_ShutDown       ( &_data.ssao_pass );
    gfx::ShadowPass::_ShutDown     ( &_data.shadow_pass );
    gfx::GeometryPass::_ShutDown   ( &_data.geometry_pass );
    _data.renderer.ShutDown( bx::GResourceManager() );
}

void TestMainState::OnStartUp()
{
    _camera.world = Matrix4( Matrix3::rotationY( PI ), Vector3( -4.21f, 5.f, -20.6f ) );
    _camera.world *= Matrix4::rotationX( -0.3f );
    _camera.params.zFar = 100.f;

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

    _gfx_scene = _data->renderer.CreateScene( "test" );
    _gfx_scene->EnableSunSkyLight();
    _gfx_scene->GetSunSkyLight()->sun_direction = normalize( Vector3( -1.f, -1.0f, 0.f ) );
    _gfx_scene->GetSunSkyLight()->sky_cubemap = gfx::GTextureManager()->CreateFromFile( "texture/sky1_cubemap.dds" );
    //_gfx_scene->GetSunSkyLight()->sky_intensity = 1.f;
    //_gfx_scene->GetSunSkyLight()->sun_intensity = 1.f;

    {
        gfx::ActorID actor = _gfx_scene->Add( "ground", 1 );
        _gfx_scene->SetMaterial( actor, gfx::GMaterialManager()->Find( "grey" ) );
        _gfx_scene->SetMeshHandle( actor, gfx::GMeshManager()->Find( ":box" ) );

        Matrix4 pose = Matrix4::translation( Vector3( 0.f, -1.0f, 0.f ) );
        pose = appendScale( pose, Vector3( 100.f, 1.f, 100.f ) );

        _gfx_scene->SetMatrices( actor, &pose, 1 );
    }

    gfx::MeshHandle mesh_handle = gfx::GMeshManager()->Find( ":sphere" );

    for( u32 ir = 0; ir <= 10; ++ir )
    {
        for( u32 is = 0; is <= 10; ++is )
        {
            gfx::MaterialDesc mat_desc;
            mat_desc.data.diffuse_color = float3_t( 0.2f, 0.2f, 0.2f );
            mat_desc.data.diffuse = 0.95f;
            mat_desc.data.roughness = ir * 0.1f;
            mat_desc.data.specular = is * 0.1f;
            mat_desc.data.metallic = 0.0f;

            char nameBuff[32];
            sprintf_s( nameBuff, 32, "mat%u%u", ir, is );
            gfx::MaterialHandle hmat = gfx::GMaterialManager()->Create( nameBuff, mat_desc );

            sprintf_s( nameBuff, 32, "actor%u%u", ir, is );
            gfx::ActorID actor = _gfx_scene->Add( nameBuff, 1 );

            const Vector3 pos = Vector3( -10.f + (float)is, 0.f, -10.f + (float)ir );
            const Matrix4 pose( Matrix3::identity(), pos );

            _gfx_scene->SetMeshHandle( actor, mesh_handle );
            _gfx_scene->SetMaterial( actor, hmat );
            _gfx_scene->SetMatrices( actor, &pose, 1 );
        }
    }

    //_boxes = _gfx_scene->Add( "boxes", NUM_INSTANCES );
    //_spheres = _gfx_scene->Add( "spheres", NUM_INSTANCES );
    //
    //Matrix4 box_instances[ NUM_INSTANCES ] = 
    //{
    //    Matrix4( Matrix3::rotationZYX( Vector3( 0.f, PI / 2, 0.f ) ), Vector3( 2.f, -1.f, 2.f ) ),
    //    Matrix4( Matrix3::rotationZYX( Vector3( 0.f, PI / 3, 0.f ) ), Vector3( 2.f, -2.f, 2.f ) ),
    //    appendScale( Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.f ) ), Vector3( 0.f, -2.f, 0.f ) ), Vector3( 10.f, 0.1f, 10.f ) ),
    //};
    //
    //Matrix4 sph_instances[ NUM_INSTANCES ] = 
    //{
    //    Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.f ) ), Vector3( -2.f, 1.f,-2.f ) ),
    //    Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.f ) ), Vector3( -2.f, 0.f,-2.f ) ),
    //    Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.f ) ), Vector3( -2.f,-2.f,-2.f ) ),
    //};
    //
    //_gfx_scene->SetMatrices( _boxes, box_instances, NUM_INSTANCES );
    //_gfx_scene->SetMatrices( _spheres, sph_instances, NUM_INSTANCES );
    //
    //_gfx_scene->SetMesh( _boxes, gfx::GMeshManager()->Find( ":box" ));
    //_gfx_scene->SetMesh( _spheres, gfx::GMeshManager()->Find( ":sphere" ) );
    //
    //gfx::MaterialHandle material_id = gfx::GMaterialManager()->Find( "red" );
    //_gfx_scene->SetMaterial( _boxes, material_id );
    //
    //material_id = gfx::GMaterialManager()->Find( "green" );
    //_gfx_scene->SetMaterial( _spheres, material_id );

    //gfx::TextureHandle htex0 = gfx::GTextureManager()->CreateFromFile( "texture/kozak.dds" );
    //gfx::TextureHandle htex1 = gfx::GTextureManager()->CreateFromFile( "texture/sky_cubemap.DDS" );
}

void TestMainState::OnShutDown()
{
    _data->renderer.DestroyScene( &_gfx_scene );
}

void TestMainState::OnUpdate( const GameTime& time )
{
    const float deltaTime = time.DeltaTimeSec();

    bxWindow* win = bxWindow_get();
    {
        bxInput* input = &win->input;
        bxInput_Mouse* inputMouse = &input->mouse;
        bxInput_PadState* inputPad = input->pad.currentState();

        if( inputPad->connected /*&& !useDebugCamera*/ )
        {
            const float sensitivity = 3.f;
            _camera_input_ctx.updateInput( -inputPad->analog.right_X * sensitivity, inputPad->analog.right_Y * sensitivity, deltaTime );
        }
        else
        {
            _camera_input_ctx.updateInput( inputMouse->currentState()->lbutton
                                           , inputMouse->currentState()->mbutton
                                           , inputMouse->currentState()->rbutton
                                           , inputMouse->currentState()->dx
                                           , inputMouse->currentState()->dy
                                           , 0.01f
                                           , deltaTime );
        }
        const Matrix4 new_camera_world = _camera_input_ctx.computeMovement( _camera.world, 0.15f );
        _camera.world = new_camera_world;
    }

    gfx::computeMatrices( &_camera );
}

void TestMainState::OnRender( const GameTime& time, rdi::CommandQueue* cmdq )
{
    bxWindow* win = bxWindow_get();

    _data->renderer.BeginFrame( cmdq );

    _data->geometry_pass.PrepareScene( cmdq, _gfx_scene, _camera );
    _data->geometry_pass.Flush( cmdq );

    rdi::TextureDepth depthTexture = rdi::GetTextureDepth( _data->geometry_pass.GBuffer() );
    rdi::TextureRW normalsTexture = rdi::GetTexture( _data->geometry_pass.GBuffer(), 2 );
    _data->shadow_pass.PrepareScene( cmdq, _gfx_scene, _camera );
    _data->shadow_pass.Flush( cmdq, depthTexture, normalsTexture );

    _data->ssao_pass.PrepareScene( cmdq, _camera );
    _data->ssao_pass.Flush( cmdq, depthTexture, normalsTexture );

    _data->light_pass.PrepareScene( cmdq, _gfx_scene, _camera );
    _data->light_pass.Flush( cmdq,
                             _data->renderer.GetFramebuffer( gfx::EFramebuffer::SWAP ),
                             _data->geometry_pass.GBuffer(),
                             _data->shadow_pass.ShadowMap(),
                             _data->ssao_pass.SsaoTexture() );

    rdi::TextureRW srcColor = _data->renderer.GetFramebuffer( gfx::EFramebuffer::SWAP );
    rdi::TextureRW dstColor = _data->renderer.GetFramebuffer( gfx::EFramebuffer::COLOR );
    _data->post_pass.DoToneMapping( cmdq, dstColor, srcColor, time.DeltaTimeSec() );

    rdi::debug_draw::AddAxes( Matrix4::identity() );

    gfx::Renderer::DebugDraw( cmdq, dstColor, depthTexture, _camera );
    //rdi::debug_draw::_Flush( cmdq, _camera.view, _camera.proj );

    rdi::ResourceRO* toRasterize[] =
    {
        &dstColor,
        &_data->ssao_pass.SsaoTexture(),
        &_data->shadow_pass.ShadowMap(),
        &_data->shadow_pass.DepthMap(),
    };
    const int toRasterizeN = sizeof( toRasterize ) / sizeof( *toRasterize );
    static int dstColorSelect = 0;
    
    if( bxInput_isKeyPressedOnce( &win->input.kbd, ' ' ) )
    {
        dstColorSelect = ( dstColorSelect + 1 ) % toRasterizeN;
    }
    //rdi::TextureRW texture = rdi::GetTexture( _geometry_pass.GBuffer(), 2 );
    //rdi::TextureRW texture = _post_pass._tm.initial_luminance;
    //rdi::TextureRW texture = dstColor;
    //rdi::ResourceRO texture = _shadow_pass.DepthMap();
    rdi::ResourceRO texture = *toRasterize[dstColorSelect];
    _data->renderer.RasterizeFramebuffer( cmdq, texture, _camera, win->width, win->height );
    _data->renderer.EndFrame( cmdq );
}

}//
