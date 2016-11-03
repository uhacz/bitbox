#pragma once

#include "anim_struct.h"
#include <util/debug.h>
#include <util/hash.h>
#include <util/tag.h>

namespace bx{ namespace anim{

    static const u32 SKEL_TAG = bxTag32( "SK01" );
    static const u32 ANIM_TAG = bxTag32( "AN01" );

inline int getJointByHash( const Skel* skeleton, u32 joint_hash )
{
	SYS_ASSERT( skeleton != 0 );
	SYS_ASSERT( skeleton->tag == SKEL_TAG );

	const u32* joint_hash_array = TYPE_OFFSET_GET_POINTER( const u32, skeleton->offsetJointNames );
	for( int i = 0; i < (int)skeleton->numJoints; ++i )
	{
		if( joint_hash == joint_hash_array[i] )
			return i;
	}
	return -1;
}
inline int getJointByName( const Skel* skeleton, const char* joint_name )
{
	SYS_ASSERT( joint_name != 0 );
	const u32 joint_hash = simple_hash( joint_name );
	return getJointByHash( skeleton, joint_hash );
}
}}//

