#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>


struct bxInput;

namespace bx{ namespace puzzle {

struct SceneCtx;

namespace physics
{
    struct Solver;
};

struct Player { u32 i; };

Player PlayerCreate( const char* name, SceneCtx* sctx );
void PlayerDestroy( Player pl );
bool IsAlive( Player pl );

void PlayerCollectInput( Player pl, const bxInput& input, const Matrix3F& basis, u64 deltaTimeUS );
void PlayerTick( SceneCtx* sctx, u64 deltaTimeUS );
void PlayerPostPhysicsTick( SceneCtx* sctx );
void PlayerDraw( Player pl );

}}//
