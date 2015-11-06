#pragma once

#include <util/vectormath/vectormath.h>

#include <foundation/PxVec2.h>
#include <foundation/PxVec3.h>
#include <foundation/PxVec4.h>
#include <characterkinematic/PxExtended.h>

using namespace physx;

namespace bx
{

inline Vector3 toVector3( const PxVec3& v )
{
	return Vector3( v.x, v.y, v.z );
}
inline Vector3 toVector3( const PxExtendedVec3& v )
{
	return Vector3(
			static_cast<float>( v.x ),
			static_cast<float>( v.y ),
			static_cast<float>( v.z )
		);
}

inline Vector4 toVector4( const PxVec4& v )
{
	return Vector4( v.x, v.y, v.z, v.w );
}
inline Quat toQuat( const PxQuat& q )
{
	return Quat( q.x, q.y, q.z, q.w );
}
inline Matrix4 toMatrix4( const PxTransform& tr )
{
	const Vector3 pos = toVector3( tr.p );
	const Quat rot = toQuat( tr.q );
	return Matrix4( rot, pos );
}

inline PxVec3 toPxVec3( const Vector3& v )
{
	const float* f = reinterpret_cast<const float*>( &v );
	return PxVec3( f[0], f[1], f[2] );
}
inline PxExtendedVec3 toPxExtendedVec3( const Vector3& v )
{
	const float* f = reinterpret_cast<const float*>( &v );
	return PxExtendedVec3( 
		static_cast<double>(f[0]),
		static_cast<double>(f[1]),
		static_cast<double>(f[2])
		);
}
inline PxVec4 toPxVec4( const Vector4& v )
{
	const float* f = reinterpret_cast<const float*>( &v );
	return PxVec4( f[0], f[1], f[2], f[3] );
}
inline PxQuat toPxQuat( const Quat& q )
{
	const float* f = reinterpret_cast<const float*>( &q );
	return PxQuat( f[0], f[1], f[2], f[3] );
}
inline PxTransform toPxTransform( const Matrix4& pose )
{
	const PxVec3 pos = toPxVec3( pose.getTranslation() );
	const PxQuat rot = toPxQuat( Quat( pose.getUpper3x3() ) );
	return PxTransform( pos, rot.getNormalized() );
}
inline PxTransform toPxTransform( const Transform3& pose )
{
    const PxVec3 pos = toPxVec3( pose.getTranslation() );
    const PxQuat rot = toPxQuat( Quat( pose.getUpper3x3() ) );
    return PxTransform( pos, rot.getNormalized() );
}

#define releasePhysxObject( ob ) if( ob ) { ob->release(); ob = 0; }

}///