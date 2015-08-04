#include "anim_tool.h"

#include <util/hash.h>
#include <util/memory.h>
#include <util/tag.h>
#include <util/filesystem.h>

#include <anim/anim_struct.h>
#include <vectormath/vectormath.h>



#include <iostream>
#include <util/debug.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>

static inline f32x4 toFloat4( const aiVector3D& v, float w = 1.f )
{
	return f32x4( v.x, v.y, v.z, w );
}

static inline f32x4 toFloat4( const aiQuaternion& q )
{
	return f32x4( q.x, q.y, q.z, q.w );
}

static void _read_node( tools::anim::Skeleton* skel, u16* current_index, u16 parent_index, aiNode* node )
{
	const u32 name_hash = utils::hash( node->mName.C_Str() );

	aiVector3D translation;
	aiQuaternion rotation;
	aiVector3D scale;
	node->mTransformation.Decompose( scale, rotation, translation );

	tools::anim::Joint joint;
	joint.translation = toFloat4( translation );
	joint.rotation = toFloat4( rotation );
	joint.scale = toFloat4( scale );

	skel->joint_name_hashes.push_back( name_hash );
	skel->base_pose.push_back( joint );
	skel->parent_indices.push_back( parent_index );

	i32 parent_for_children = *current_index;
	*current_index = parent_for_children + 1;

	for( u32 i = 0; i < node->mNumChildren; ++i )
	{
		_read_node( skel, current_index, parent_for_children, node->mChildren[i] );
	}

}

static void _extract_skeleton( tools::anim::Skeleton* skel, const aiScene* scene )
{
	u16 index = 0;
	_read_node( skel, &index, 0xFFFF, scene->mRootNode );
}

bool tools::export_skeleton( const i8* out_filename, const i8* in_filename )
{
	const unsigned import_flags = 0;
	const aiScene* aiscene = aiImportFile( in_filename, import_flags );
	if( !aiscene )
	{
		std::cout << "export skeleton failed: input file not found\n" << std::endl;
		return false;
	}

	tools::anim::Skeleton skel;
	_extract_skeleton( &skel, aiscene );

	const bool bres = tools::anim::export_skeleton( out_filename, skel );
	return bres;
}

static inline u32 _find_joint_animation( u32 joint_name_hash, const aiAnimation* animation )
{
	const u32 n = animation->mNumChannels;
	for( u32 i = 0; i < n; ++i )
	{
		const u32 node_name_hash = utils::hash( animation->mChannels[i]->mNodeName.C_Str() );
		if( node_name_hash == joint_name_hash )
			return i;
	}

	return TYPE_INVALID_ID32;
}
static void _extract_animation( tools::anim::Animation* anim, const tools::anim::Skeleton& skel, const aiScene* scene )
{
	aiAnimation* animation = scene->mAnimations[0];

	anim->start_time = 0.f;
	anim->end_time = (float)animation->mDuration;
	anim->sample_frequency = (float)animation->mTicksPerSecond;

	const u32 num_joints = (u32)skel.joint_name_hashes.size();
	for( u32 i = 0; i < num_joints; ++i )
	{
		anim->joints.push_back( tools::anim::JointAnimation() );
		tools::anim::JointAnimation& janim = anim->joints.back();

		janim.name_hash = skel.joint_name_hashes[i];
		janim.weight = 1.f;
	}

	u32 max_key_frames = 0;

	for( u32 i = 0; i < num_joints; ++i )
	{
		tools::anim::JointAnimation& janim = anim->joints[i];

		const u32 node_index = _find_joint_animation( janim.name_hash, animation );
		if( node_index == TYPE_INVALID_ID32 )
			continue;

		const aiNodeAnim* node_anim = animation->mChannels[node_index];
		//SYS_ASSERT( node_anim->mNumPositionKeys == node_anim->mNumRotationKeys );
		//SYS_ASSERT( node_anim->mNumPositionKeys == node_anim->mNumScalingKeys );
		max_key_frames = utils::max_of_pair( max_key_frames, node_anim->mNumRotationKeys );
		max_key_frames = utils::max_of_pair( max_key_frames, node_anim->mNumPositionKeys );
		max_key_frames = utils::max_of_pair( max_key_frames, node_anim->mNumScalingKeys );

		for( u32 j = 0; j < node_anim->mNumRotationKeys; ++j )
		{
			const aiQuatKey& node_key = node_anim->mRotationKeys[j];

			janim.rotation.push_back( tools::anim::AnimKeyframe() );
			tools::anim::AnimKeyframe& key = janim.rotation.back();

			key.data = toFloat4( node_key.mValue );
			key.time = static_cast<float>( node_key.mTime );
		}

		for( u32 j = 0; j < node_anim->mNumPositionKeys; ++j )
		{
			const aiVectorKey& node_key = node_anim->mPositionKeys[j];

			janim.translation.push_back( tools::anim::AnimKeyframe() );
			tools::anim::AnimKeyframe& key = janim.translation.back();

			key.data = toFloat4( node_key.mValue );
			key.time = static_cast<float>( node_key.mTime );
		}

		for( u32 j = 0; j < node_anim->mNumScalingKeys; ++j )
		{
			const aiVectorKey& node_key = node_anim->mScalingKeys[j];

			janim.scale.push_back( tools::anim::AnimKeyframe() );
			tools::anim::AnimKeyframe& key = janim.scale.back();

			key.data = toFloat4( node_key.mValue );
			key.time = static_cast<float>( node_key.mTime );
		}
	}

	anim->num_frames = max_key_frames;

	for( u32 i = 0; i < num_joints; ++i )
	{
		tools::anim::JointAnimation& janim = anim->joints[i];
		{
			const u32 n_rotations = max_key_frames - (u32)janim.rotation.size();
			const u32 n_translations = max_key_frames - (u32)janim.translation.size();
			const u32 n_scale = max_key_frames - (u32)janim.scale.size();
			const float dt = (float)( 1.0 / animation->mTicksPerSecond );

			tools::anim::AnimKeyframe rotation_keyframe;
			tools::anim::AnimKeyframe translation_keyframe;
			tools::anim::AnimKeyframe scale_keyframe;

			rotation_keyframe.data		= ( janim.rotation.empty() ) ? skel.base_pose[i].rotation : janim.rotation.back().data;
			translation_keyframe.data	= ( janim.translation.empty() ) ? skel.base_pose[i].translation : janim.translation.back().data;
			scale_keyframe.data			= ( janim.scale.empty() ) ? skel.base_pose[i].scale : janim.scale.back().data;

			rotation_keyframe.time		= ( janim.rotation.empty() ) ? 0.f : janim.rotation.back().time + dt;
			translation_keyframe.time	= ( janim.translation.empty() ) ? 0.f : janim.translation.back().time + dt;
			scale_keyframe.time			= ( janim.scale.empty() ) ? 0.f : janim.scale.back().time + dt;



			for( u32 j = 0; j < n_rotations; ++j )
			{
				janim.rotation.push_back( rotation_keyframe );
				rotation_keyframe.time += dt;
			}
			for( u32 j = 0; j < n_translations; ++j )
			{
				janim.translation.push_back( translation_keyframe );
				translation_keyframe.time += dt;
			}
			for( u32 j = 0; j < n_scale; ++j )
			{
				janim.scale.push_back( scale_keyframe );
				scale_keyframe.time += dt;
			}

			SYS_ASSERT( janim.rotation.back().time <= animation->mDuration );
			SYS_ASSERT( janim.translation.back().time <= animation->mDuration );
			SYS_ASSERT( janim.scale.back().time <= animation->mDuration );

		}
	}
}


bool tools::export_animation( const i8* out_filename, const i8* in_filename )
{
	const unsigned import_flags = 0;
	const aiScene* aiscene = aiImportFile( in_filename, import_flags );
	if( !aiscene )
	{
		std::cout << "export skeleton failed: input file not found!\n" << std::endl;
		return false;
	}

	if( !aiscene->HasAnimations() )
	{
		std::cout << "scene does not contain animation!\n" << std::endl;
		return false;
	}
	tools::anim::Skeleton skel;
	tools::anim::Animation anim;

	_extract_skeleton( &skel, aiscene );
	_extract_animation( &anim, skel, aiscene );

	const bool bres = tools::anim::export_animation( out_filename, anim, skel );

	return bres;
}
#endif



//////////////////////////////////////////////////////////////////////////
///

u32 tools::anim::generate_skeleton_tag()
{
	return utils::Tag32( "SK01" );
}
u32 tools::anim::generate_animation_tag()
{
	return utils::Tag32( "AN01" );
}

bool tools::anim::export_skeleton( const i8* out_filename, const Skeleton& in_skeleton )
{
	const u32 num_joints = (u32)in_skeleton.joint_name_hashes.size();
	const u32 parent_indices_size = num_joints * sizeof( u16 );
	const u32 base_pose_size = num_joints * sizeof( anim::Joint );
	const u32 joint_name_hashes_size = num_joints * sizeof( u32 );

	u32 memory_size = 0;
	memory_size += sizeof( st8::AnimSkeleton );
	memory_size += parent_indices_size;
	memory_size += base_pose_size;
	memory_size += joint_name_hashes_size;

	u8* memory = (u8*)utils::memory_alloc_aligned( memory_size, 16 );

	st8::AnimSkeleton* out_skeleton = (st8::AnimSkeleton*)memory;
	memset( out_skeleton, 0, sizeof(st8::AnimSkeleton) );

	u8* base_pose_address = (u8*)( out_skeleton + 1 );
	u8* parent_indices_address = base_pose_address + base_pose_size;
	u8* joint_name_hashes_address = parent_indices_address + parent_indices_size;

	out_skeleton->tag = generate_skeleton_tag();
	out_skeleton->num_joints = num_joints;
	out_skeleton->offset_base_pose = TYPE_POINTER_GET_OFFSET( &out_skeleton->offset_base_pose, base_pose_address );
	out_skeleton->offset_parent_indices_array = TYPE_POINTER_GET_OFFSET( &out_skeleton->offset_parent_indices_array, parent_indices_address );
	out_skeleton->offset_joint_namehash_array = TYPE_POINTER_GET_OFFSET( &out_skeleton->offset_joint_namehash_array, joint_name_hashes_address );

	memcpy( base_pose_address, &in_skeleton.base_pose[0], base_pose_size );
	memcpy( parent_indices_address, &in_skeleton.parent_indices[0], parent_indices_size );
	memcpy( joint_name_hashes_address, &in_skeleton.joint_name_hashes[0], joint_name_hashes_size );

	i32 write_result = utils::write_file( out_filename, memory, memory_size );

	memory_free_aligned_and_null( memory );

	return ( write_result == TYPE_FAILURE ) ? false : true;
}



//////////////////////////////////////////////////////////////////////////
///



static inline u32 _find_joint_animation( u32 joint_name_hash, const tools::anim::Skeleton& skel )
{
	const u32 n = (u32)skel.joint_name_hashes.size();
	for( u32 i = 0; i < n; ++i )
	{
		if( skel.joint_name_hashes[i] == joint_name_hash )
			return i;
	}

	return TYPE_INVALID_ID32;
} 

bool tools::anim::export_animation( const i8* out_filename, const Animation& in_animation, const Skeleton& in_skeleton )
{
	const u32 num_joints = (u32)in_skeleton.joint_name_hashes.size();
	const u32 num_frames = in_animation.num_frames;
	const u32 channel_data_size = num_joints * num_frames * sizeof(f32x4);

	u32 memory_size = 0;
	memory_size += sizeof( st8::AnimClip );
	memory_size += channel_data_size; // rotation
	memory_size += channel_data_size; // translation
	memory_size += channel_data_size; // scale

	u8* memory = (u8*)utils::memory_alloc_aligned( memory_size, 16 );

	st8::AnimClip* clip = (st8::AnimClip*)memory;
	memset( clip, 0, sizeof(st8::AnimClip) );

	u8* rotation_address = (u8*)( clip + 1 );
	u8* translation_address = rotation_address + channel_data_size;
	u8* scale_address = translation_address + channel_data_size;

	clip->tag = generate_animation_tag();
	clip->duration = in_animation.end_time - in_animation.start_time;
	clip->sample_frequency = in_animation.sample_frequency;
	clip->num_joints = num_joints;
	clip->num_frames = num_frames;
	clip->offset_rotation_data = TYPE_POINTER_GET_OFFSET( &clip->offset_rotation_data, rotation_address );
	clip->offset_translation_data = TYPE_POINTER_GET_OFFSET( &clip->offset_translation_data, translation_address );
	clip->offset_scale_data = TYPE_POINTER_GET_OFFSET( &clip->offset_scale_data, scale_address );

	std::vector<f32x4> rotation_vector;
	std::vector<f32x4> translation_vector;
	std::vector<f32x4> scale_vector;
	
	for( u32 i = 0 ; i < num_frames; ++i )
	{
		for( u32 j = 0; j < num_joints; ++j )
		{
			const tools::anim::AnimKeyframe& rotation_keyframe = in_animation.joints[j].rotation[i];
			const tools::anim::AnimKeyframe& translation_keyframe = in_animation.joints[j].translation[i];
			const tools::anim::AnimKeyframe& scale_keyframe = in_animation.joints[j].scale[i];

			rotation_vector.push_back( rotation_keyframe.data );
			translation_vector.push_back( translation_keyframe.data );
			scale_vector.push_back( scale_keyframe.data );
		}
	}

	SYS_ASSERT( channel_data_size == (rotation_vector.size() * sizeof(f32x4)) );
	SYS_ASSERT( channel_data_size == (translation_vector.size() * sizeof(f32x4)) );
	SYS_ASSERT( channel_data_size == (scale_vector.size() * sizeof(f32x4)) );

	memcpy( rotation_address, &rotation_vector[0], channel_data_size );
	memcpy( translation_address, &translation_vector[0], channel_data_size );
	memcpy( scale_address, &scale_vector[0], channel_data_size );

	utils::write_file( out_filename, memory, memory_size );

	memory_free_aligned_and_null( memory );

	return false;
}
