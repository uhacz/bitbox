#include "input.h"
#include <memory.h>
#include "util\debug.h"

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

void bxInput_swap( bxInput_Pad* pad )
{
    pad->currentStateIndex = !pad->currentStateIndex;
}

void bxInput_swap( bxInput_Keyboard* kbd )
{
    kbd->prevState = kbd->currState;
}

void bxInput_swap( bxInput_Mouse* mouse )
{
    mouse->prevState = mouse->currState;
}

void bxInput_computeMouseDelta( bxInput_MouseState* curr, const bxInput_MouseState& prev )
{
    curr->dx = curr->x - prev.x;
    curr->dy = curr->y - prev.y;
    //bxLogInfo( "%d, %d, %d, %d", curr->x, curr->y, curr->dx, curr->dy );
}
