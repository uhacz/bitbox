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

//////////////////////////////////////////////////////////////////////////
/// standard input: keyboard and mouse
struct bxInput_State
{
	bxInput_State();
	u8 keys[256];

	struct 
	{
		u16 x;
		u16 y;
		i16 dx;
		i16 dy;

		u8 lbutton;
		u8 mbutton;
		u8 rbutton;
		i8 wheel;
	} mouse;

	enum { eMAX_PAD_CONTROLLERS = 1 };
	bxInput_PadState pad[eMAX_PAD_CONTROLLERS];
};

//////////////////////////////////////////////////////////////////////////
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

	};
	bxInput_State curr;
	bxInput_State prev;
};

bool bxInput_isKeyPressed( const bxInput* input, unsigned char key );
bool bxInput_isPeyPressedOnce( const bxInput* input, unsigned char key );

void bxInput_updatePad( bxInput_PadState* pad_states, u32 n );
void bxInput_clearState( bxInput_State* state, bool keys = true, bool mouse = true, bool pad = true );

//////////////////////////////////////////////////////////////////////////
inline void bxInput_computeMouseDelta( bxInput_State* curr, const bxInput_State& prev )
{
	curr->mouse.dx = curr->mouse.x - prev.mouse.x;
	curr->mouse.dy = curr->mouse.y - prev.mouse.y;
}



inline const bxInput_PadState& input_get_pad0_state( const bxInput& input ) 
{ 
	return input.curr.pad[0]; 
}
inline bool is_pad_button_pressed_once( const bxInput& input, u32 pad_index, u16 button_mask )
{
	const bxInput_PadState& prev_state = input.curr.pad[pad_index];
	const bxInput_PadState& curr_state = input.prev.pad[pad_index];

	const u16 prev = prev_state.digital.buttons & button_mask;
	const u16 curr = curr_state.digital.buttons & button_mask;

	return !prev && curr;
}
inline bool is_pad0_button_pressed_once( const bxInput& input, u16 button_mask ) { return is_pad_button_pressed_once( input, 0, button_mask ); }
