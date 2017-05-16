#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

struct bxInput;

namespace bx{ namespace puzzle {

struct Player { u32 i; };

Player PlayerCreate( const char* name );
void PlayerDestroy( Player pl );

void PlayerMove( Player pl, const bxInput& input, const Matrix3F& basis );
void PlayerTick( u64 deltaTimeUS );
void PlayerDraw( Player pl );

}}//
