#include "puzzle_player_internal.h"

namespace bx{ namespace puzzle {

//////////////////////////////////////////////////////////////////////////
void SetName( PlayerName* pn, const char* str )
{
    pn->str = string::duplicate( pn->str, str );
}

//////////////////////////////////////////////////////////////////////////
void Collect( PlayerInput* playerInput, const bxInput& input, float deltaTime )
{
    float analogX = 0.f;
    float analogY = 0.f;
    float jump = 0.f;
    float crouch = 0.f;
    float L2 = 0.f;
    float R2 = 0.f;

    const bxInput_Pad& pad = bxInput_getPad( input );
    const bxInput_PadState* padState = pad.currentState();
    if( padState->connected )
    {
        analogX = padState->analog.left_X;
        analogY = padState->analog.left_Y;

        crouch = (f32)bxInput_isPadButtonPressedOnce( pad, bxInput_PadState::eCIRCLE );
        jump = (f32)bxInput_isPadButtonPressedOnce( pad, bxInput_PadState::eCROSS );

        L2 = padState->analog.L2;
        R2 = padState->analog.R2;
    }
    else
    {
        const bxInput_Keyboard* kbd = &input.kbd;

        const int inLeft = bxInput_isKeyPressed( kbd, 'A' );
        const int inRight = bxInput_isKeyPressed( kbd, 'D' );
        const int inFwd = bxInput_isKeyPressed( kbd, 'W' );
        const int inBack = bxInput_isKeyPressed( kbd, 'S' );
        const int inJump = bxInput_isKeyPressedOnce( kbd, ' ' );
        const int inCrouch = bxInput_isKeyPressedOnce( kbd, bxInput::eKEY_LSHIFT );
        const int inL2 = bxInput_isKeyPressed( kbd, 'Q' );
        const int inR2 = bxInput_isKeyPressed( kbd, 'E' );

        analogX = -(float)inLeft + (float)inRight;
        analogY = -(float)inBack + (float)inFwd;

        crouch = (f32)inCrouch;
        jump = (f32)inJump;

        L2 = (float)inL2;
        R2 = (float)inR2;

        //bxLogInfo( "x: %f, y: %f", analogX, analogX );
    }

    const float RC = 0.01f;
    playerInput->analogX = signalFilter_lowPass( analogX, playerInput->analogX, RC, deltaTime );
    playerInput->analogY = signalFilter_lowPass( analogY, playerInput->analogY, RC, deltaTime );
    playerInput->jump = jump; // signalFilter_lowPass( jump, charInput->jump, 0.01f, deltaTime );
    playerInput->crouch = signalFilter_lowPass( crouch, playerInput->crouch, RC, deltaTime );
    playerInput->L2 = L2;
    playerInput->R2 = R2;
}

//////////////////////////////////////////////////////////////////////////
void Clear( PlayerPoseBuffer* ppb )
{
    ppb->_ring.clear();
}

// ---
u32 Write( PlayerPoseBuffer* ppb, const PlayerPose& pose, const PlayerInput& input, const Matrix3F& basis, u64 ts )
{
    if( ppb->_ring.full() )
        ppb->_ring.shift();

    u32 index = ppb->_ring.push();

    ppb->poses[index] = pose;
    ppb->input[index] = input;
    ppb->basis[index] = basis;
    ppb->timestamp[index] = ts;

    return index;
}
// ---
bool Read( PlayerPose* pose, PlayerInput* input, Matrix3F* basis, u64* ts, PlayerPoseBuffer* ppb )
{
    if( ppb->_ring.empty() )
        return false;

    u32 index = ppb->_ring.shift();

    pose[0] = ppb->poses[index];
    input[0] = ppb->input[index];
    basis[0] = ppb->basis[index];
    ts[0] = ppb->timestamp[index];

    return true;
}

}}//
