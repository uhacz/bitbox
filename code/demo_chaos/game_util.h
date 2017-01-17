#pragma once

#include <util/camera.h>

namespace bx{namespace game_util{

void DevCameraCollectInput( gfx::CameraInputContext* inputCtx, float deltaTime, float sensitivitiInPixels = 0.005f );

}
}//
