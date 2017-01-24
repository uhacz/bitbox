#include "ship_player.h"
#include <system/window.h>
#include <system/input.h>
#include <util/signal_filter.h>
#include <util/common.h>
#include <util/curve.h>
#include <rdi/rdi_debug_draw.h>

#include "../imgui/imgui.h"

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
    _analog_y = signalFilter_lowPass( inputPad->analog.right_Y, _analog_y, inputRC, deltaTime );
    _fire = bxInput_isPadButtonPressedOnce( input->pad, bxInput_PadState::eR1 );// signalFilter_lowPass( inputPad->, _fire, 0.01f, deltaTime );
    _thrust = signalFilter_lowPass( inputPad->analog.R2, _thrust, inputRC, deltaTime );
    
}

//////////////////////////////////////////////////////////////////////////
//
void PlayerCamera::Tick( const Player& player, const Terrain& terrain, float deltaTime )
{
    const Matrix4 camera_world = _camera.world;
    const Vector4 up_plane = makePlane( _up_dir, player._pos );
    
    const Vector3 player_dir = normalizeSafe( projectVectorOnPlane( player.GetDirection(), up_plane ) );

    Vector3 direction = normalizeSafe( _up_dir - player_dir * 4 );
    Vector3 new_camera_pos = player._pos + ( direction ) * _distance_from_player;

    Vector3 new_camera_z = normalize( new_camera_pos - player._pos );
    Vector3 new_camera_x = normalize( cross( _up_dir, new_camera_z ) );
    Vector3 new_camera_y = normalize( cross( new_camera_z, new_camera_x ) );

    _camera.world = Matrix4( Matrix3( new_camera_x, new_camera_y, new_camera_z ), new_camera_pos );
    computeMatrices( &_camera );
}

//////////////////////////////////////////////////////////////////////////
//
namespace
{
    struct AttackAngleCurve
    {
        f32 _value[4];
        f32 _knot[4];

        AttackAngleCurve()
        {
            _value[0] =-10.f * PI_OVER_180;
            _value[1] = 2.4f * PI_OVER_180;
            _value[2] = 10.f * PI_OVER_180;
            _value[3] = 12.f * PI_OVER_180;

            _knot[0] = 0.f;
            _knot[1] = 0.5f;
            _knot[2] = 0.8f;
            _knot[3] = 1.f;
        }

        float Value( float t01 )
        {
            return curve::spline( _knot, _value, 4, t01 );
        }
    };

    static AttackAngleCurve attack_angle_curve = {};

    static float ComputeLift( float attackAngle, float altitude, float vSqr, float S, float airDens )
    {
        const float Cl = PI2 * attackAngle;
        const float lift_force_value = ( airDens * vSqr * S * Cl ) * 0.5f;
        return lift_force_value;
    }
};

void Player::Tick( const PlayerCamera& camera, const Terrain& terrain, float deltaTime )
{
    //_input.Collect( bxWindow_get(), deltaTime, 0.1f );
    //Vector3 input_vector( 0.f );
    //input_vector += Vector3::xAxis() * _input._analog_x;
    //input_vector += Vector3::zAxis() * _input._analog_y;
    //input_vector = normalizeSafe( input_vector ) * minf4( oneVec, length( input_vector ) );

    //Vector3 input_vector_ws = camera._camera.world.getUpper3x3() * input_vector;
    //Vector3 force = input_vector_ws;

    //const float min_attack_angle = -10.f * PI_OVER_180;
    //const float max_attack_angle = 12.f * PI_OVER_180;
    //const float base_attack_angle = 2.4f * PI_OVER_180;

    const float g = 9.82f; // 9.82f;
    
    const float input_01_x = ::abs( _input._analog_x ) * 0.25f + 0.75f;
    const float input_01_y = -_input._analog_y * 0.5f + 0.5f;
    
    const float attack_angle_x = attack_angle_curve.Value( input_01_x );
    const float attack_angle_y = attack_angle_curve.Value( input_01_y );

    const float altitude = linearstep( _min_altitude, _max_altitude, _pos.getY().getAsFloat() );
    // curve at t=1 -> air dens. = 0
    const float air_dens_max = 0.002377f; // g/cm3
    const float air_dens_min = 0.000285f; // g/cm3
    const float air_dens = lerp( altitude, air_dens_max, air_dens_min );

    const float S_lift = _wing_width * _wing_depth;
    const float S_drag = _wing_width * _wing_height;
    
    const float v_sqr = lengthSqr( _vel ).getAsFloat();

    const float lift_force_value_y = ComputeLift( attack_angle_y, altitude, v_sqr, S_lift, air_dens );
    const float lift_force_value_x = ComputeLift( attack_angle_x, altitude, v_sqr, S_lift, air_dens );
    
    const float drag_coeff = 0.05f; //Airplane wing, normal position
    const float drag_force_value = air_dens * v_sqr * S_drag * 0.5f;

    const Vector3 lift_dir = _rot.getCol1();
    const Vector3 drag_dir = projectVectorOnPlane( normalizeSafe( _vel ), makePlane( _up_dir, _pos ) );
    
    
    _thrust = _input._thrust;
    _yaw = -attack_angle_y; // _input._analog_y * 0.5f;
    _roll = _input._analog_x * PI_HALF;
    _rot = Matrix3::rotationZYX( Vector3( _yaw, _pitch, _roll ) );
    
    _lift = lift_dir * (lift_force_value_y /*+ lift_force_value_x*/);
    _drag = drag_dir * drag_force_value;
    const Vector3 thrust = _rot.getCol2() * _thrust;
        
    const Vector3 gravity = Vector3( 0.f, -g, 0.f );
    Vector3 force = thrust + _drag + _lift;
    Vector3 acc = force * _mass_inv + gravity;
    _vel += acc * deltaTime;
    //_vel *= ::pow( 1.f - _vel_damping, deltaTime );
    _pos += _vel * deltaTime;

    if( _pos.getY().getAsFloat() < _min_altitude )
    {
        _pos.setY( _min_altitude );
        _vel.setY( 0.f );
    }
}

void Player::Gui()
{

    rdi::debug_draw::AddLine( _pos, _pos + _lift, 0xFF00FF00, 1 );
    rdi::debug_draw::AddLine( _pos, _pos + _drag, 0xFFFF0000, 1 );
    rdi::debug_draw::AddSphere( Vector4( _pos, 0.1f ), 0xFF00FF00, 1 );
    rdi::debug_draw::AddBox( Matrix4( _rot, _pos ), Vector3( _wing_width, _wing_height, _wing_depth ), 0xFF00FF00, 1 );

    const float altitude = linearstep( _min_altitude, _max_altitude, _pos.getY().getAsFloat() );
    if( ImGui::Begin( "player" ) )
    {
        ImGui::Text( "altitude: %f", altitude );
        ImGui::Text( "lift acc: %f", length( _lift ).getAsFloat() * _mass_inv );
        ImGui::Text( "drag acc: %f", length( _drag ).getAsFloat() * _mass_inv );
        ImGui::Text( "velocity: %f", length( _vel ).getAsFloat() );
        ImGui::Text( "position: %f, %f, %f", _pos.getX().getAsFloat(), _pos.getY().getAsFloat(), _pos.getZ().getAsFloat() );
    }
    ImGui::End();
}

}
}//
