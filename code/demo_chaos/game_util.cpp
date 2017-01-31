#include "game_util.h"
#include <system/window.h>
#include "renderer_material.h"

namespace bx{namespace game_util{

void DevCameraCollectInput( gfx::CameraInputContext* inputCtx, float deltaTime, float sensitivitiInPixels )
{
    bxWindow* win = bxWindow_get();
    bxInput* input = &win->input;
    bxInput_Mouse* inputMouse = &input->mouse;

    inputCtx->updateInput( inputMouse->currentState()->lbutton
                         , inputMouse->currentState()->mbutton
                         , inputMouse->currentState()->rbutton
                         , inputMouse->currentState()->dx
                         , inputMouse->currentState()->dy
                         , sensitivitiInPixels
                         , deltaTime );
}

void CreateDebugMaterials()
{
    {
        gfx::MaterialDesc mat_desc;
        mat_desc.data.diffuse_color = float3_t( 1.f, 0.f, 0.f );
        mat_desc.data.diffuse = 0.5f;
        mat_desc.data.roughness = 0.9f;
        mat_desc.data.specular = 0.1f;
        mat_desc.data.metallic = 0.0f;
        gfx::GMaterialManager()->Create( "red", mat_desc );

        mat_desc.data.diffuse_color = float3_t( 0.f, 1.f, 0.f );
        mat_desc.data.roughness = 0.9f;
        mat_desc.data.specular = 0.1f;
        mat_desc.data.metallic = 0.0f;
        gfx::GMaterialManager()->Create( "green", mat_desc );

        mat_desc.data.diffuse_color = float3_t( 0.f, 0.f, 1.f );
        mat_desc.data.roughness = 0.9f;
        mat_desc.data.specular = 0.1f;
        mat_desc.data.metallic = 0.0f;
        gfx::GMaterialManager()->Create( "blue", mat_desc );

        mat_desc.data.diffuse_color = float3_t( 0.4f, 0.4f, 0.4f );
        mat_desc.data.roughness = 0.9f;
        mat_desc.data.specular = 0.1f;
        mat_desc.data.metallic = 0.0f;
        gfx::GMaterialManager()->Create( "grey", mat_desc );
    }
}

}}