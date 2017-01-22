#include "ship_level.h"
#include "ship_game.h"
#include "../renderer.h"

#include <util/string_util.h>

namespace bx{ namespace ship{

void Level::StartUp( Gfx* gfx, const char* levelName )
{
    _name = string::duplicate( (char*)_name, levelName );
    _gfx_scene = gfx->renderer.CreateScene( levelName );
    _gfx_scene->EnableSunSkyLight();
    _gfx_scene->GetSunSkyLight()->sun_direction = normalize( Vector3( -1.f, -1.0f, 0.f ) );
    _gfx_scene->GetSunSkyLight()->sky_cubemap = gfx::GTextureManager()->CreateFromFile( "texture/sky1_cubemap.dds" );

    {
        gfx::ActorID actor = _gfx_scene->Add( "ground", 1 );
        _gfx_scene->SetMaterial( actor, gfx::GMaterialManager()->Find( "red" ) );
        _gfx_scene->SetMesh( actor, gfx::GMeshManager()->Find( ":box" ) );

        const float radius = 750.f;

        Matrix4 pose = Matrix4::translation( Vector3( 0.f, -1.f, radius*0.75f ) );
        pose = appendScale( pose, Vector3( radius, 0.1f, radius * 2.f ) );

        _gfx_scene->SetMatrices( actor, &pose, 1 );
    }
}

void Level::ShutDown( Gfx* gfx )
{
    gfx->renderer.DestroyScene( &_gfx_scene );
    string::free( (char*)_name );
    _name = nullptr;


}

void Level::Tick( const GameTime& time )
{
    const float delta_time_sec = time.DeltaTimeSec();
    _player_camera.Tick( _player, _terrain, delta_time_sec );
    _player.Tick( _player_camera, _terrain, delta_time_sec );
}

void Level::Render( rdi::CommandQueue* cmdq, const GameTime& time )
{

}

}
}//