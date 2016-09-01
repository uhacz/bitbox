#include "anim.h"
#include <util/debug.h>

void bxAnim::localJointsToWorldJoints( bxAnim_Joint* outJoints, const bxAnim_Joint* inJoints, const unsigned short* parentIndices, unsigned count, const bxAnim_Joint& rootJoint )
{
	SYS_ASSERT( outJoints != inJoints );
	SYS_ASSERT( count > 0 );

	u32 i = 0;
	do 
	{
		const unsigned short parentIdx = parentIndices[i];

		bxAnim_Joint world;
		const bxAnim_Joint& local = inJoints[i];
		const bxAnim_Joint& worldParent = ( parentIdx != 0xFFFF ) ? outJoints[parentIdx] : rootJoint;

		world.rotation = worldParent.rotation * local.rotation;
		world.rotation = normalize( world.rotation );

		world.scale = mulPerElem( local.scale, worldParent.scale );

		const Vector3 ts = mulPerElem( local.position, worldParent.scale );
		const Vector4 q = Vector4( worldParent.rotation );

		const floatInVec w = q.getW();

		const Vector3 c = cross( q.getXYZ(), ts ) + w * ts;
		const Vector3 qts = ts + 2.0f * cross( q.getXYZ(), c );

		world.position = worldParent.position + qts;

		outJoints[i] = world;

	} while ( ++i < count );
		
}