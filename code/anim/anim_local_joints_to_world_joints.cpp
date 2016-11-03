#include "anim.h"
#include <util/debug.h>

namespace bx{ namespace anim{

void localJointsToWorldJoints( Joint* outJoints, const Joint* inJoints, const unsigned short* parentIndices, unsigned count, const Joint& rootJoint )
{
	SYS_ASSERT( outJoints != inJoints );
	SYS_ASSERT( count > 0 );

	u32 i = 0;
	do 
	{
		const unsigned short parentIdx = parentIndices[i];

		Joint world;
		const Joint& local = inJoints[i];
		const Joint& worldParent = ( parentIdx != 0xFFFF ) ? outJoints[parentIdx] : rootJoint;

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

}}///
