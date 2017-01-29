#include "ship_level.h"
#include "ship_game.h"
#include "../renderer.h"

#include <util/string_util.h>
#include "util/common.h"
#include "system/window.h"

namespace bx{ namespace ship{

void Level::StartUp( Gfx* gfx, const char* levelName )
{
    _name = string::duplicate( (char*)_name, levelName );
    _gfx_scene = gfx->renderer.CreateScene( levelName );
    _gfx_scene->EnableSunSkyLight();
    _gfx_scene->GetSunSkyLight()->sun_direction = normalize( Vector3( -1.f, -1.0f, 0.f ) );
    _gfx_scene->GetSunSkyLight()->sky_cubemap = gfx::GTextureManager()->CreateFromFile( "texture/sky1_cubemap.dds" );

    //{
    //    gfx::ActorID actor = _gfx_scene->Add( "ground", 2 );
    //    _gfx_scene->SetMaterial( actor, gfx::GMaterialManager()->Find( "green" ) );
    //    _gfx_scene->SetMeshHandle( actor, gfx::GMeshManager()->Find( ":box" ) );

    //    const float radius = 750.f;

    //    Matrix4 pose[] =
    //    {
    //        appendScale( Matrix4::translation( Vector3( 0.f, -1.f, radius*0.75f ) ), Vector3( radius, 0.1f, radius * 2.f ) ),
    //        appendScale( Matrix4::translation( Vector3( 0.f, -0.5f, radius*0.75f ) ), Vector3( 1.0f, 0.1f, radius * 2.f ) ),
    //    };
    //    pose[1] *= Matrix4::rotationZ( 3.14f / 4 );

    //    _gfx_scene->SetMatrices( actor, pose, 2 );
    //}
    //{
    //    gfx::ActorID actor = _gfx_scene->Add( "ground1", 1 );
    //    _gfx_scene->SetMaterial( actor, gfx::GMaterialManager()->Find( "red" ) );
    //    _gfx_scene->SetMeshHandle( actor, gfx::GMeshManager()->Find( ":box" ) );

    //    const float radius = 750.f;

    //    Matrix4 pose = Matrix4::translation( Vector3( 0.f, -1.f, radius*0.75f ) );
    //    pose = appendScale( pose, Vector3( radius, 0.1f, radius * 2.f ) );

    //    _gfx_scene->SetMatrices( actor, &pose, 1 );
    //}

    _terrain.CreateFromFile( "model/terrain0.r32", _gfx_scene );

}

void Level::ShutDown( Gfx* gfx )
{
    _terrain.Destroy();

    gfx->renderer.DestroyScene( &_gfx_scene );
    string::free( (char*)_name );
    _name = nullptr;


}

void Level::Tick( const GameTime& time )
{
    const float delta_time_sec = time.DeltaTimeSec();
    const float max_delta_time = 1.f / 60.f;
    float delta_time_acc = delta_time_sec;

    _player_camera.Tick( _player, _terrain, delta_time_sec );
    _player._input.Collect( bxWindow_get(), delta_time_sec, 0.3f );
    while( delta_time_acc > 0.f )
    {
        const float dt = minOfPair( max_delta_time, delta_time_acc );
        
        _player.Tick( _player_camera, _terrain, dt );
        
        delta_time_acc = maxOfPair( 0.f, delta_time_acc - max_delta_time );
    }

    //_terrain.DebugDraw( 0xFF0000FF );

    _player.Gui();
    
}

void Level::Render( rdi::CommandQueue* cmdq, const GameTime& time )
{

}

}
}//