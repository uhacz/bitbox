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

        void PrepareScene( rdi::CommandQueue* cmdq, gfx::Scene scene, const gfx::Camera& camera );
        void Draw( rdi::CommandQueue* cmdq );
        void PostProcess( rdi::CommandQueue* cmdq, const gfx::Camera& camera, float deltaTimeSec );
        void Rasterize( rdi::CommandQueue* cmdq, const gfx::Camera& camera );
    };

    void StartUp    ( Deffered* gfx );
    void ShutDown   ( Deffered* gfx );
    
};

}//