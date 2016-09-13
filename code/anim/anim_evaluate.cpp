#include "anim.h"

static inline void _ComputeFrame( u32* frameInt, f32* frameFrac, f32 evalTime, f32 sampleFrequency )
{
    f32 frame = evalTime * sampleFrequency;
    if( frame < 0.f )
        frame = 0.f;

    const u32 i = (u32)frame;

    frameInt[0] = i;
    frameFrac[0] = frame - (f32)i;
}

struct FrameInfo
{
    const Quat*    rotations;
    const Vector3* translations;
    const Vector3* scales;

    const Quat*    rotations0;
    const Vector3* translations0;
    const Vector3* scales0;

    const Quat*    rotations1;
    const Vector3* translations1;
    const Vector3* scales1;

    FrameInfo( const bxAnim_Clip* anim, u32 frameInteger )
    {
        const u16 numJoints = anim->numJoints;
        const u32 currentFrame = ( frameInteger ) % anim->numFrames;
        const u32 nextFrame = ( frameInteger + 1 ) % anim->numFrames;

        rotations = TYPE_OFFSET_GET_POINTER( const Quat, anim->offsetRotationData );
        translations = TYPE_OFFSET_GET_POINTER( const Vector3, anim->offsetTranslationData );
        scales = TYPE_OFFSET_GET_POINTER( const Vector3, anim->offsetScaleData );

        rotations0 = rotations + currentFrame * numJoints;
        translations0 = translations + currentFrame * numJoints;
        scales0 = scales + currentFrame * numJoints;

        rotations1 = rotations + nextFrame * numJoints;
        translations1 = translations + nextFrame * numJoints;
        scales1 = scales + nextFrame * numJoints;
    }
};

void bxAnim::evaluateClip( bxAnim_Joint* out_joints, const bxAnim_Clip* anim, f32 evalTime, u32 beginJoint, u32 endJoint )
{
    u32 frameInteger = 0;
    f32 frameFraction = 0.f;
    _ComputeFrame( &frameInteger, &frameFraction, evalTime, anim->sampleFrequency );
	evaluateClip( out_joints, anim, frameInteger, frameFraction, beginJoint, endJoint );
}

void bxAnim::evaluateClip( bxAnim_Joint* out_joints, const bxAnim_Clip* anim, u32 frameInteger, f32 frameFraction, u32 beginJoint, u32 endJoint )
{
	//const u16 numJoints    = anim->numJoints;
	//const u32 currentFrame = ( frameInteger ) % anim->numFrames;
	//const u32 nextFrame    = ( frameInteger + 1 ) % anim->numFrames;

	//const Quat*    rotations    = TYPE_OFFSET_GET_POINTER( const Quat, anim->offsetRotationData );
	//const Vector3* translations = TYPE_OFFSET_GET_POINTER( const Vector3, anim->offsetTranslationData );
	//const Vector3* scales       = TYPE_OFFSET_GET_POINTER( const Vector3, anim->offsetScaleData );

	//const Quat*    frame0_rotations    = rotations + currentFrame * numJoints;
	//const Vector3* frame0_translations = translations + currentFrame * numJoints;
	//const Vector3* frame0_scales       = scales + currentFrame * numJoints;

	//const Quat*    frame1_rotations     = rotations + nextFrame * numJoints;
	//const Vector3* frame1_translations  = translations + nextFrame * numJoints;
	//const Vector3* frame1_scales        = scales + nextFrame * numJoints;

    const FrameInfo frame( anim, frameInteger );

    u16 i = ( beginJoint == UINT32_MAX ) ? 0 : beginJoint;
    endJoint = ( endJoint == UINT32_MAX ) ? anim->numJoints : endJoint+1;
	
    const floatInVec alpha( frameFraction );
	do 
	{
		const Quat& q0 = frame.rotations0[i];
		const Quat& q1 = frame.rotations1[i];

		const Quat q = slerp( alpha, q0, q1 );

		out_joints[i].rotation = q;

	} while ( ++i < endJoint );

	i = 0;
	do 
	{
		const Vector3& t0 = frame.translations0[i];
		const Vector3& t1 = frame.translations1[i];

		const Vector3 t = lerp( alpha, t0, t1 );

		out_joints[i].position = t;

    } while( ++i < endJoint );

	i = 0;
	do 
	{
		const Vector3& s0 = frame.scales0[i];
		const Vector3& s1 = frame.scales1[i];

		const Vector3 s = lerp( alpha, s0, s1 );

		out_joints[i].scale = s;

    } while( ++i < endJoint );
}


void bxAnim::evaluateClipIndexed( bxAnim_Joint* out_joints, const bxAnim_Clip* anim, f32 evalTime, const i16* indices, u32 numIndices )
{
    u32 frameInteger = 0;
    f32 frameFraction = 0.f;
    _ComputeFrame( &frameInteger, &frameFraction, evalTime, anim->sampleFrequency );
    evaluateClipIndexed( out_joints, anim, frameInteger, frameFraction, indices, numIndices );

}

void bxAnim::evaluateClipIndexed( bxAnim_Joint* out_joints, const bxAnim_Clip* anim, u32 frameInteger, f32 frameFraction, const i16* indices, u32 numIndices )
{
    const FrameInfo frame( anim, frameInteger );
    const floatInVec alpha( frameFraction );

    for( u32 ii = 0; ii < numIndices; ++ii )
    {
        const i16 i = indices[ii];
        
        const Quat& q0 = frame.rotations0[i];
        const Quat& q1 = frame.rotations1[i];

        const Quat q = slerp( alpha, q0, q1 );

        out_joints[i].rotation = q;
    }

    for( u32 ii = 0; ii < numIndices; ++ii )
    {
        const i16 i = indices[ii];

        const Vector3& t0 = frame.translations0[i];
        const Vector3& t1 = frame.translations1[i];

        const Vector3 t = lerp( alpha, t0, t1 );

        out_joints[i].position = t;
    }

    for( u32 ii = 0; ii < numIndices; ++ii )
    {
        const i16 i = indices[ii];

        const Vector3& s0 = frame.scales0[i];
        const Vector3& s1 = frame.scales1[i];

        const Vector3 s = lerp( alpha, s0, s1 );

        out_joints[i].scale = s;
    }


}