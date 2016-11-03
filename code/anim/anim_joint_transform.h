#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>

namespace bx{ namespace anim{

struct BIT_ALIGNMENT_16 Joint
{
	Quat rotation;
	Vector3 position;
	Vector3 scale;

    static Joint identity()
    {
        Joint joint = { Quat::identity(), Vector3( 0.f ), Vector3( 1.f ) };
        return joint;
    }
};

inline Joint toAnimJoint_noScale( const Matrix4& RT )
{
	Joint joint;
	joint.position = RT.getTranslation();
	joint.rotation = Quat( RT.getUpper3x3() );
	joint.scale = Vector3( 1.f );
	return joint;
}

inline Matrix4 toMatrix4( const Joint& j )
{
	Matrix4 m4x4( j.rotation, j.position );
    m4x4.setCol0(m4x4.getCol0() * j.scale.getX());
	m4x4.setCol1(m4x4.getCol1() * j.scale.getY());
	m4x4.setCol2(m4x4.getCol2() * j.scale.getZ());
	return m4x4;
}

}}///
