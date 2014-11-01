#include "input.h"
#include <memory.h>

bxInput_State::bxInput_State()
{
	memset( this, 0, sizeof(*this) );
}

void bxInput_clearState( bxInput_State* state, bool keys, bool mouse, bool pad )
{
	if( keys )
	{
		memset( state->keys, 0, sizeof(state->keys) );
	}
	if( mouse )
	{
		memset( &state->mouse, 0, sizeof(state->mouse) );
	}
	if( pad )
	{
		memset( state->pad, 0, sizeof(state->pad) );
	}

}

bool bxInput_isKeyPressed( const bxInput* input, unsigned char key )
{
    return input->curr.keys[key] > 0;
}

bool bxInput_isPeyPressedOnce( const bxInput* input, unsigned char key )
{
    const unsigned char curr_state = input->curr.keys[key];
    const unsigned char prev_state = input->prev.keys[key];

    return !prev_state && curr_state;
}
