#pragma once

#include "renderer.h"

namespace bx
{

struct GameGfxDeffered
{
    gfx::Renderer renderer;

    gfx::GeometryPass geometry_pass;
    gfx::ShadowPass shadow_pass;
    gfx::SsaoPass ssao_pass;
    gfx::LightPass light_pass;
    gfx::PostProcessPass post_pass;
};

void GameGfxStartUp( GameGfxDeffered* gfx );
void GameGfxShutDown( GameGfxDeffered* gfx );
void GameGfxDrawScene( rdi::CommandQueue* cmdq, GameGfxDeffered* gfx, gfx::Scene scene, const gfx::Camera& camera );
void GameGfxPostProcess( rdi::CommandQueue* cmdq, GameGfxDeffered* gfx, const gfx::Camera& camera, float deltaTimeSec );
void GameGfxRasterize( rdi::CommandQueue* cmdq, GameGfxDeffered* gfx, const gfx::Camera& camera );


}//