#include "anim.h"

void bxAnim::blendJointsLinear( bxAnim_Joint* outJoints, const bxAnim_Joint* leftJoints, const bxAnim_Joint* rightJoints, float blendFactor, unsigned short numJoints )
{
	u16 i = 0;

	const floatInVec alpha( blendFactor );
	do 
	{
		const Quat& q0 = leftJoints[i].rotation;
		const Quat& q1 = rightJoints[i].rotation;

		const Quat q = slerp( alpha, q0, q1 );

		outJoints[i].rotation = q;

	} while ( ++i < numJoints );

	i = 0;
	do 
	{
		const Vector3& t0 = leftJoints[i].position;
		const Vector3& t1 = rightJoints[i].position;

		const Vector3 t = lerp( alpha, t0, t1 );

		outJoints[i].position = t;

	} while ( ++i < numJoints );

	i = 0;
	do 
	{
		const Vector3& s0 = leftJoints[i].scale;
		const Vector3& s1 = rightJoints[i].scale;

		const Vector3 s = lerp( alpha, s0, s1 );

		outJoints[i].scale = s;

	} while ( ++i < numJoints );
}

