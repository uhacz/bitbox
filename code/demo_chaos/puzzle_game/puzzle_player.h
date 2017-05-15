#pragma once

#include <util/type.h>

namespace bx
{
    struct bxInput;
}//

namespace bx{ namespace puzzle {

struct Player { u32 i; };

Player PlayerCreate( const char* name );
void PlayerDestroy( Player pl );

void PlayerMove( Player pl, const bxInput& input );
void PlayerTick( f32 deltaTime );
void PlayerDraw( Player pl );

}}//
