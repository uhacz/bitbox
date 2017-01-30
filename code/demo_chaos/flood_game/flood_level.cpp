#include "flood_level.h"

#include "../renderer.h"
#include "../game_gfx.h"

#include <util\string_util.h>

namespace bx{ namespace flood{


void Level::StartUp( game_gfx::Deffered* gfx, const char* levelName )
{
    _name = string::duplicate( (char*)_name, levelName );
    _gfx_scene = gfx->renderer.CreateScene( levelName );
    _gfx_scene->EnableSunSkyLight();
    _gfx_scene->GetSunSkyLight()->sun_direction = normalize( Vector3( -1.f, -1.0f, 0.f ) );
    _gfx_scene->GetSunSkyLight()->sky_cubemap = gfx::GTextureManager()->CreateFromFile( "texture/sky1_cubemap.dds" );
}

void Level::ShutDown( game_gfx::Deffered* gfx )
{
    gfx->renderer.DestroyScene( &_gfx_scene );
    string::free( (char*)_name );
    _name = nullptr;
}

void Level::Tick( const GameTime& time )
{

}

void Level::Render( rdi::CommandQueue* cmdq, const GameTime& time )
{

}

}
}//
