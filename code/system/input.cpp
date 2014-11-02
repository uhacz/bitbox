#include "input.h"
#include <memory.h>

bxInput::bxInput()
{
	memset( this, 0, sizeof(*this) );
}

void bxInput_clear( bxInput* state, bool keys, bool mouse, bool pad )
{
	if( keys )
	{
		memset( state->kbd.currentState(), 0, sizeof(bxInput_KeyboardState) );
	}
	if( mouse )
	{
		memset( state->mouse.currentState(), 0, sizeof(bxInput_MouseState) );
	}
	if( pad )
	{
		memset( state->pad.currentState(), 0, sizeof(bxInput_PadState) );
	}

}
