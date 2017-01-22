#include "ship_player.h"
#include <system/window.h>
#include <system/input.h>
#include <util/signal_filter.h>
#include <util/common.h>
#include <rdi/rdi_debug_draw.h>

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
    _fire = bxInput_isPadButtonPressedOnce( input->pad, bxInput_PadState::eR1 );// signalFilter_lowPass( inputPad->, _fire, 0.01f, deltaTime );
    _thrust = signalFilter_lowPass( inputPad->analog.R2, _thrust, inputRC, deltaTime );
    
}

//////////////////////////////////////////////////////////////////////////
//
void PlayerCamera::Tick( const Player& player, const Terrain& terrain, float deltaTime )
{
    const Matrix4 camera_world = _camera.world;

    const Vector3 player_dir = player.GetDirection();

    Vector3 direction = _up_dir - player_dir*3;
    Vector3 new_camera_pos = player._pos + ( direction ) * _distance_from_player;

    Vector3 new_camera_z = normalize( new_camera_pos - player._pos );
    Vector3 new_camera_x = normalize( cross( _up_dir, new_camera_z ) );
    Vector3 new_camera_y = normalize( cross( new_camera_z, new_camera_x ) );

    _camera.world = Matrix4( Matrix3( new_camera_x, new_camera_y, new_camera_z ), new_camera_pos );
    computeMatrices( &_camera );
}

//////////////////////////////////////////////////////////////////////////
//
void Player::Tick( const PlayerCamera& camera, const Terrain& terrain, float deltaTime )
{
    const float maxDeltaTime = 1.f / 60.f;

    while( deltaTime > 0.f )
    {
        const float dt = minOfPair( maxDeltaTime, deltaTime );
        FixedTick( camera, terrain, dt );
        deltaTime = maxOfPair( 0.f, deltaTime - maxDeltaTime );
    }
}

void Player::FixedTick( const PlayerCamera& camera, const Terrain& terrain, float deltaTime )
{
    _input.Collect( bxWindow_get(), deltaTime, 0.3f );
    //Vector3 input_vector( 0.f );
    //input_vector += Vector3::xAxis() * _input._analog_x;
    //input_vector += Vector3::zAxis() * _input._analog_y;
    //input_vector = normalizeSafe( input_vector ) * minf4( oneVec, length( input_vector ) );

    //Vector3 input_vector_ws = camera._camera.world.getUpper3x3() * input_vector;
    //Vector3 force = input_vector_ws;

    const float min_attack_angle = -10.f * PI_OVER_180;
    const float max_attack_angle = 12.f * PI_OVER_180;
    const float input_01 = -_input._analog_y * 0.5f + 0.5f;
    const float attack_angle = lerp( input_01, min_attack_angle, max_attack_angle );

    _thrust = _input._thrust*10.f;
    _yaw = -attack_angle; // _input._analog_y * 0.5f;
    _roll = _input._analog_x * PI_HALF;


    _rot = Matrix3::rotationZYX( Vector3( _yaw, _pitch, _roll ) );
    
    const float g = 0.3f;
    //const float air_dens = 1.2041f;
    const float altitude = linearstep( _max_altitude, _max_altitude * 0.9f, _pos.getY().getAsFloat() );
    
    // curve at t=1 -> air dens. = 0
    const float air_dens = lerp( altitude, 0.002377f, 0.0000285f );
    const float Cl = PI2 * attack_angle;
    const float S = 3.f * 1.f;
    const float v_sqr = lengthSqr( _vel ).getAsFloat();
    //float R = v_sqr / g * ::tan( _roll );

    const float lift_force_value = ( air_dens * v_sqr * S * Cl ) * 0.5f;
    const Vector3 lift_vector = _rot.getCol1() * lift_force_value;

    const Vector3 gravity = Vector3( 0.f, -g, 0.f );
    Vector3 force = _rot.getCol2() * _thrust + lift_vector;
    Vector3 acc = force * _mass_inv + gravity;
    _vel += acc * deltaTime;
    _vel *= ::pow( 1.f - _vel_damping, deltaTime );
    _pos += _vel * deltaTime;

    if( _pos.getY().getAsFloat() < _min_altitude )
    {
        _pos.setY( 0.f );
        _vel.setY( 0.f );
    }
    else if( _pos.getY().getAsFloat() >= _max_altitude )
    {
        _pos.setY( _max_altitude );
        _vel.setY( 0.f );
    }

    rdi::debug_draw::AddLine( _pos, _pos + lift_vector, 0xFF00FFFF, 1 );
    rdi::debug_draw::AddSphere( Vector4( _pos, 0.1f ), 0xFF00FF00, 1 );
    rdi::debug_draw::AddBox( Matrix4( _rot, _pos ), Vector3( 0.3f, 0.05f, 0.1f ), 0xFF00FF00, 1 );
}

}
}//
