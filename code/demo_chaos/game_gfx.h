#pragma once

#include "renderer.h"

namespace bx
{

namespace game_gfx
{
    struct Deffered
    {
        gfx::Renderer renderer;

        gfx::GeometryPass geometry_pass;
        gfx::ShadowPass shadow_pass;
        gfx::SsaoPass ssao_pass;
        gfx::LightPass light_pass;
        gfx::PostProcessPass post_pass;
    };

    void StartUp    ( Deffered* gfx );
    void ShutDown   ( Deffered* gfx );
    void DrawScene  ( rdi::CommandQueue* cmdq, Deffered* gfx, gfx::Scene scene, const gfx::Camera& camera );
    void PostProcess( rdi::CommandQueue* cmdq, Deffered* gfx, const gfx::Camera& camera, float deltaTimeSec );
    void Rasterize  ( rdi::CommandQueue* cmdq, Deffered* gfx, const gfx::Camera& camera );
};

}//