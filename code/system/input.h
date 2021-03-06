#pragma once

#include <util/type.h>

//////////////////////////////////////////////////////////////////////////
/// pad
struct bxInput_PadState
{
	enum
	{
		/// uni
		eUP		= BIT_OFFSET(1),
		eDOWN	= BIT_OFFSET(2),
		eLEFT	= BIT_OFFSET(3),
		eRIGHT	= BIT_OFFSET(4),

		/// ps3
		eCROSS		= BIT_OFFSET(5),
		eSQUARE		= BIT_OFFSET(6),
		eTRIANGLE	= BIT_OFFSET(7),
		eCIRCLE		= BIT_OFFSET(8),
		eL1			= BIT_OFFSET(9),
		eL2			= BIT_OFFSET(10),
		eL3			= BIT_OFFSET(11),
		eR1			= BIT_OFFSET(12),
		eR2			= BIT_OFFSET(13),
		eR3			= BIT_OFFSET(14),
		eSELECT		= BIT_OFFSET(15),
		eSTART		= BIT_OFFSET(16),
		
		/// xbox
		eA = eCROSS,
		eB = eCIRCLE,
		eX = eSQUARE,
		eY = eTRIANGLE,
		eLSHOULDER = eL1,
		eLTRIGGER = eL2,
		eLTHUMB = eL3,
		eRSHOULDER = eR1,
		eRTRIGGER = eR2,
		eRTHUMB = eR3,
		eBACK = eSELECT,
	};

	struct 
	{
		float left_X;
		float left_Y;
		float right_X;
		float right_Y;
		
		float L2;
		float R2;
	} analog;

	struct  
	{
		u32 buttons;
	} digital;

	u32 connected;
};

struct bxInput_KeyboardState
{
    u8 keys[256];
};

struct bxInput_MouseState
{
    u16 x;
    u16 y;
    i16 dx;
    i16 dy;

    u8 lbutton;
    u8 mbutton;
    u8 rbutton;
    i8 wheel;
};

/////////////////////////////////////////////////////////////////
struct bxInput_Pad
{
    bxInput_PadState state[2];
    u32 currentStateIndex : 1;

          bxInput_PadState* currentState()       { return &state[currentStateIndex]; }
    const bxInput_PadState* currentState() const { return &state[currentStateIndex]; }
};

struct bxInput_Keyboard
{
    bxInput_KeyboardState prevState;
    bxInput_KeyboardState currState;

          bxInput_KeyboardState* currentState()       { return &currState; }
    const bxInput_KeyboardState* currentState() const { return &currState; }
};


struct bxInput_Mouse
{
    bxInput_MouseState prevState;
    bxInput_MouseState currState;

    bxInput_MouseState*       currentState()       { return &currState; }
    const bxInput_MouseState* currentState() const { return &currState; }
    const bxInput_MouseState& previousState() const { return prevState; }
};

/////////////////////////////////////////////////////////////////
void bxInput_swap( bxInput_Pad* pad );
void bxInput_swap( bxInput_Keyboard* kbd );
void bxInput_swap( bxInput_Mouse* mouse );

/////////////////////////////////////////////////////////////////
/// input
struct bxInput
{
	enum
	{
		eKEY_ESC = 27,
        eKEY_LEFT = 37,
        eKEY_UP = 38,
        eKEY_RIGHT = 39,
        eKEY_DOWN = 40,
        eKEY_LSHIFT = 16,
        eKEY_CAPSLOCK = 0x14,
	};

    bxInput_Keyboard kbd;
    bxInput_Mouse mouse;
    bxInput_Pad pad;

    bxInput();
};
inline void bxInput_swap( bxInput* input )
{
    bxInput_swap( &input->kbd );
    bxInput_swap( &input->mouse );
    bxInput_swap( &input->pad );
}


void bxInput_clear( bxInput* input, bool keys = true, bool mouse = true, bool pad = true );
void bxInput_updatePad( bxInput_PadState* pad_states, u32 n );

//////////////////////////////////////////////////////////////////////////
inline bool bxInput_isKeyPressed( const bxInput_Keyboard* input, unsigned char key )
{
    return ( input->currState.keys[key]) != 0;
}
inline bool bxInput_isKeyPressedOnce( const bxInput_Keyboard* input, unsigned char key )
{
    const unsigned char curr_state = input->currState.keys[key];
    const unsigned char prev_state = input->prevState.keys[key];

    return !prev_state && curr_state;
}
void bxInput_computeMouseDelta( bxInput_MouseState* curr, const bxInput_MouseState& prev );
inline const bxInput_Pad& bxInput_getPad( const bxInput& input ) 
{ 
	return input.pad; 
}
inline bool bxInput_isPadButtonPressedOnce( const bxInput_Pad& input, u16 button_mask )
{
	const bxInput_PadState& prev_state = input.state[!input.currentStateIndex];
	const bxInput_PadState& curr_state = input.state[ input.currentStateIndex];

	const u16 prev = prev_state.digital.buttons & button_mask;
	const u16 curr = curr_state.digital.buttons & button_mask;

	return !prev && curr;
}

inline bool bxInput_isPadButtonPressed( const bxInput_Pad& input, u16 button_mask )
{
    const bxInput_PadState& curr_state = input.state[input.currentStateIndex];
    const u16 curr = curr_state.digital.buttons & button_mask;
    return curr != 0;
}
