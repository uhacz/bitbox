#pragma once

#include <util/type.h>

namespace bx {namespace puzzle {
namespace physics
{

struct Solver;
struct Gfx;
struct GUIContext;
struct BodyId { u32 i; };
inline BodyId BodyIdInvalid() { return { 0 }; }
static inline bool operator == ( const BodyId a, const BodyId b ) { return a.i == b.i; }

}}}//
