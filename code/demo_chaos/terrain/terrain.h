#pragma once

#include <util/vectormath/vectormath.h>
#include "terrain_create_info.h"

namespace bx{ namespace terrain{

struct Instance;
Instance* Create( const CreateInfo& info );
void      Destroy( Instance** i );

void      Tick( Instance* inst, const Matrix4F& observer );
Vector3F  ClosestPoint( const Instance* inst, const Vector3F& point );

}}//
