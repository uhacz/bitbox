#include "ship_player.h"
#include <system/window.h>
#include <system/input.h>
#include <util/signal_filter.h>
#include "../../util/common.h"

namespace bx{namespace ship{

void CameraInput::Collect( bxWindow* win, float deltaTime, float inputRC /*= 0.1f */ )
{
    bxInput* input = &win->input;
    bxInput_PadState* inputPad = input->pad.currentState();

    _analog_x = signalFilter_lowPass( inputPad->analog.right_X, _analog_x, inputRC, deltaTime );
    _analog_y = signalFilter_lowPass( inputPad->analog.right_Y, _analog_y, inputRC, deltaTime );

}

void PlayerInput::Collect( bxWindow* win, float deltaTime, float inputRC /*= 0.1f */ )
{
    bxInput* input = &win->input;
    bxInput_PadState* inputPad = input->pad.currentState();

    _analog_x = signalFilter_lowPass( inputPad->analog.left_X, _analog_x, inputRC, deltaTime );
    _analog_y = signalFilter_lowPass( inputPad->analog.left_Y, _analog_y, inputRC, deltaTime );
    _fire     = signalFilter_lowPass( inputPad->analog.R2, _fire, 0.01f, deltaTime );
    _altitude = signalFilter_lowPass( inputPad->analog.L2, _altitude, inputRC, deltaTime );
}

//////////////////////////////////////////////////////////////////////////
//
void PlayerCamera::Tick( const Player& player, const Terrain& terrain, float deltaTime )
{
    const Matrix4 camera_world = _camera.world;

    const Vector3 player_dir = player.GetDirection();

    Vector3 direction = _up_dir - player_dir;
    Vector3 new_camera_pos = player._pos + ( direction ) * _distance_from_player;

    Vector3 new_camera_z = normalize( new_camera_pos - player._pos );
    Vector3 new_camera_x = normalize( cross( _up_dir, new_camera_z ) );
    Vector3 new_camera_y = normalize( cross( new_camera_z, new_camera_x ) );

    _camera.world = Matrix4( Matrix3( new_camera_x, new_camera_y, new_camera_z ), new_camera_pos );

}

//////////////////////////////////////////////////////////////////////////
//
void Player::Tick( const PlayerCamera& camera, const Terrain& terrain, float deltaTime )
{
    const float maxDeltaTime = 1.f / 60.f;

    while( deltaTime > 0.f )
    {
        FixedTick( camera, terrain, maxDeltaTime );
        deltaTime = maxOfPair( 0.f, deltaTime - maxDeltaTime );
    }
}

void Player::FixedTick( const PlayerCamera& camera, const Terrain& terrain, float deltaTime )
{
    Vector3 input_vector( 0.f );
    input_vector += Vector3::xAxis() * _input._analog_x;
    input_vector += Vector3::zAxis() * _input._analog_y;
    input_vector = normalizeSafe( input_vector ) * minf4( oneVec, length( input_vector ) );

    


}

}
}//
