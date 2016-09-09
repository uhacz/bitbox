#include "anim.h"

void bxAnim::evaluateClip( bxAnim_Joint* out_joints, const bxAnim_Clip* anim, f32 evalTime, u32 beginJoint, u32 endJoint )
{
	f32 frame = evalTime * anim->sampleFrequency;
	if( frame < 0.f )
		frame = 0.f;

	u32 frameInteger = (u32)frame;
	f32 frameFraction = frame - (f32)frameInteger;

	evaluateClip( out_joints, anim, frameInteger, frameFraction, beginJoint, endJoint );
}

void bxAnim::evaluateClip( bxAnim_Joint* out_joints, const bxAnim_Clip* anim, u32 frameInteger, f32 frameFraction, u32 beginJoint, u32 endJoint )
{
	const u16 numJoints    = anim->numJoints;
	const u32 currentFrame = ( frameInteger ) % anim->numFrames;
	const u32 nextFrame    = ( frameInteger + 1 ) % anim->numFrames;

	const Quat*    rotations    = TYPE_OFFSET_GET_POINTER( const Quat, anim->offsetRotationData );
	const Vector3* translations = TYPE_OFFSET_GET_POINTER( const Vector3, anim->offsetTranslationData );
	const Vector3* scales       = TYPE_OFFSET_GET_POINTER( const Vector3, anim->offsetScaleData );

	const Quat*    frame0_rotations    = rotations + currentFrame * numJoints;
	const Vector3* frame0_translations = translations + currentFrame * numJoints;
	const Vector3* frame0_scales       = scales + currentFrame * numJoints;

	const Quat*    frame1_rotations     = rotations + nextFrame * numJoints;
	const Vector3* frame1_translations  = translations + nextFrame * numJoints;
	const Vector3* frame1_scales        = scales + nextFrame * numJoints;

    u16 i = ( beginJoint == UINT32_MAX ) ? 0 : beginJoint;
    endJoint = ( endJoint == UINT32_MAX ) ? numJoints : endJoint+1;
	
    const floatInVec alpha( frameFraction );
	do 
	{
		const Quat& q0 = frame0_rotations[i];
		const Quat& q1 = frame1_rotations[i];

		const Quat q = slerp( alpha, q0, q1 );

		out_joints[i].rotation = q;

	} while ( ++i < endJoint );

	i = 0;
	do 
	{
		const Vector3& t0 = frame0_translations[i];
		const Vector3& t1 = frame1_translations[i];

		const Vector3 t = lerp( alpha, t0, t1 );

		out_joints[i].position = t;

    } while( ++i < endJoint );

	i = 0;
	do 
	{
		const Vector3& s0 = frame0_scales[i];
		const Vector3& s1 = frame1_scales[i];

		const Vector3 s = lerp( alpha, s0, s1 );

		out_joints[i].scale = s;

    } while( ++i < endJoint );
}
