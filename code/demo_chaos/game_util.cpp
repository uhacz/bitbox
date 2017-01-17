#include "game_util.h"
#include <system/window.h>

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

}}