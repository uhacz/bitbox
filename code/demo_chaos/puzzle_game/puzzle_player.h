#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

struct bxInput;

namespace bx{ namespace puzzle {

struct Player { u32 i; };

Player PlayerCreate( const char* name );
void PlayerDestroy( Player pl );
bool IsAlive( Player pl );

void PlayerCollectInput( Player pl, const bxInput& input, const Matrix3F& basis, u64 deltaTimeUS );
void PlayerTick( u64 deltaTimeUS );
void PlayerDraw( Player pl );

}}//
