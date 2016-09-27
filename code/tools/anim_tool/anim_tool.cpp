#include "anim_tool.h"

#include <util/hash.h>
#include <util/memory.h>
#include <util/tag.h>
#include <util/filesystem.h>

#include <anim/anim.h>
//#include <vectormath/vectormath.h>

#include <iostream>
#include <util/debug.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include "util/common.h"

static inline float4_t toFloat4( const aiVector3D& v, float w = 1.f )
{
    return float4_t( v.x, v.y, v.z, w );
}

static inline float4_t toFloat4( const aiQuaternion& q )
{
    return float4_t( q.x, q.y, q.z, q.w );
}

namespace animTool
{
    static void _ReadNode( Skeleton* skel, u16* current_index, u16 parent_index, aiNode* node )
    {
	    const u32 name_hash = simple_hash( node->mName.C_Str() );

	    aiVector3D translation;
	    aiQuaternion rotation;
	    aiVector3D scale;
	    node->mTransformation.Decompose( scale, rotation, translation );

	    Joint joint;
	    joint.translation = toFloat4( translation );
	    joint.rotation = toFloat4( rotation );
	    joint.scale = toFloat4( scale );

	    skel->jointNames.push_back( name_hash );
	    skel->basePose.push_back( joint );
	    skel->parentIndices.push_back( parent_index );

	    i32 parent_for_children = *current_index;
	    *current_index = parent_for_children + 1;

	    for( u32 i = 0; i < node->mNumChildren; ++i )
	    {
		    _ReadNode( skel, current_index, parent_for_children, node->mChildren[i] );
	    }

    }

    static void _ExtractSkeleton( Skeleton* skel, const aiScene* scene )
    {
	    u16 index = 0;
	    _ReadNode( skel, &index, 0xFFFF, scene->mRootNode );
    }

    static inline u32 _FindJointAnimation( u32 joint_name_hash, const aiAnimation* animation )
    {
        const u32 n = animation->mNumChannels;
        for( u32 i = 0; i < n; ++i )
        {
            const u32 node_name_hash = simple_hash( animation->mChannels[i]->mNodeName.C_Str() );
            if( node_name_hash == joint_name_hash )
                return i;
        }

        return UINT32_MAX;
    }
    static void _ExtractAnimation( Animation* anim, const Skeleton& skel, const aiScene* scene, unsigned flags )
    {
        aiAnimation* animation = scene->mAnimations[0];

        anim->startTime = 0.f;
        
		/// because animation->mDuration is number of frames
		anim->endTime = (float)( animation->mDuration / animation->mTicksPerSecond );
        anim->sampleFrequency = (float)animation->mTicksPerSecond;

        const u32 num_joints = (u32)skel.jointNames.size();
        for( u32 i = 0; i < num_joints; ++i )
        {
            anim->joints.push_back( JointAnimation() );
            JointAnimation& janim = anim->joints.back();

            janim.name_hash = skel.jointNames[i];
            janim.weight = 1.f;
        }

        u32 max_key_frames = 0;

        for( u32 i = 0; i < num_joints; ++i )
        {
            JointAnimation& janim = anim->joints[i];

            const u32 node_index = _FindJointAnimation( janim.name_hash, animation );
            if( node_index == UINT32_MAX )
                continue;

            const aiNodeAnim* node_anim = animation->mChannels[node_index];
            //SYS_ASSERT( node_anim->mNumPositionKeys == node_anim->mNumRotationKeys );
            //SYS_ASSERT( node_anim->mNumPositionKeys == node_anim->mNumScalingKeys );
            max_key_frames = maxOfPair( max_key_frames, node_anim->mNumRotationKeys );
            max_key_frames = maxOfPair( max_key_frames, node_anim->mNumPositionKeys );
            max_key_frames = maxOfPair( max_key_frames, node_anim->mNumScalingKeys );

            for( u32 j = 0; j < node_anim->mNumRotationKeys; ++j )
            {
                const aiQuatKey& node_key = node_anim->mRotationKeys[j];

                janim.rotation.push_back( AnimKeyframe() );
                AnimKeyframe& key = janim.rotation.back();

                key.data = toFloat4( node_key.mValue );
                key.time = static_cast<float>( node_key.mTime );
            }

            for( u32 j = 0; j < node_anim->mNumPositionKeys; ++j )
            {
                const aiVectorKey& node_key = node_anim->mPositionKeys[j];

                janim.translation.push_back( AnimKeyframe() );
                AnimKeyframe& key = janim.translation.back();

                key.data = toFloat4( node_key.mValue );
                key.time = static_cast<float>( node_key.mTime );
            }

            for( u32 j = 0; j < node_anim->mNumScalingKeys; ++j )
            {
                const aiVectorKey& node_key = node_anim->mScalingKeys[j];

                janim.scale.push_back( AnimKeyframe() );
                AnimKeyframe& key = janim.scale.back();

                key.data = toFloat4( node_key.mValue );
                key.time = static_cast<float>( node_key.mTime );
            }
        }

        anim->numFrames = max_key_frames;

        for( u32 i = 0; i < num_joints; ++i )
        {
            JointAnimation& janim = anim->joints[i];
            {
                const u32 n_rotations = max_key_frames - (u32)janim.rotation.size();
                const u32 n_translations = max_key_frames - (u32)janim.translation.size();
                const u32 n_scale = max_key_frames - (u32)janim.scale.size();
                const float dt = (float)( 1.0 / animation->mTicksPerSecond );

                AnimKeyframe rotation_keyframe;
                AnimKeyframe translation_keyframe;
                AnimKeyframe scale_keyframe;

                rotation_keyframe.data = ( janim.rotation.empty() ) ? skel.basePose[i].rotation : janim.rotation.back().data;
                translation_keyframe.data = ( janim.translation.empty() ) ? skel.basePose[i].translation : janim.translation.back().data;
                scale_keyframe.data = ( janim.scale.empty() ) ? skel.basePose[i].scale : janim.scale.back().data;

                rotation_keyframe.time = ( janim.rotation.empty() ) ? 0.f : janim.rotation.back().time + dt;
                translation_keyframe.time = ( janim.translation.empty() ) ? 0.f : janim.translation.back().time + dt;
                scale_keyframe.time = ( janim.scale.empty() ) ? 0.f : janim.scale.back().time + dt;



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

        //const unsigned removeRootMotionMask = eEXPORT_REMOVE_ROOT_TRANSLATION_X | eEXPORT_REMOVE_ROOT_TRANSLATION_Y | eEXPORT_REMOVE_ROOT_TRANSLATION_Z;
        //const unsigned removeRootMotionFlag = flags & removeRootMotionMask;
        //if( removeRootMotionFlag )
        //{
        //    JointAnimation& janim = anim->joints[0];
        //    int nFrames = (int)janim.translation.size();
        //    for( int iframe = 0; iframe < nFrames; ++iframe )
        //    {
        //        float4_t& translation = janim.translation[iframe].data;
        //        if( removeRootMotionFlag & eEXPORT_REMOVE_ROOT_TRANSLATION_X )
        //        {
        //            translation.x = 0.f;
        //        }
        //        if( removeRootMotionFlag & eEXPORT_REMOVE_ROOT_TRANSLATION_Y )
        //        {
        //            translation.y = 0.f;
        //        }
        //        if( removeRootMotionFlag & eEXPORT_REMOVE_ROOT_TRANSLATION_Z )
        //        {
        //            translation.z = 0.f;
        //        }
        //    }
        //}
    }
}///

namespace animTool
{
//////////////////////////////////////////////////////////////////////////
///

inline void* allocateMemory( u32 memSize, u32 alignment )
{
    return BX_MALLOC( bxDefaultAllocator(), memSize, alignment );
}
inline void freeMemory0( u8*& mem )
{
    BX_FREE0( bxDefaultAllocator(), mem );
}
u32 skelTag()
{
	return bxTag32( "SK01" );
}
u32 animTag()
{
	return bxTag32( "AN01" );
}
////
////
bool exportSkeleton( const char* out_filename, const Skeleton& in_skeleton )
{
    static_assert( sizeof( Joint ) == sizeof( bxAnim_Joint ), "joint size mismatch" );

    const u32 num_joints = (u32)in_skeleton.jointNames.size();
	const u32 parent_indices_size = num_joints * sizeof( u16 );
	const u32 base_pose_size = num_joints * sizeof( Joint );
	const u32 joint_name_hashes_size = num_joints * sizeof( u32 );

	u32 memory_size = 0;
	memory_size += sizeof( bxAnim_Skel );
	memory_size += parent_indices_size;
	memory_size += base_pose_size;
	memory_size += joint_name_hashes_size;

    u8* memory = (u8*)allocateMemory( memory_size, 16 );

	bxAnim_Skel* out_skeleton = (bxAnim_Skel*)memory;
	memset( out_skeleton, 0, sizeof(bxAnim_Skel) );

	u8* base_pose_address = (u8*)( out_skeleton + 1 );
	u8* parent_indices_address = base_pose_address + base_pose_size;
	u8* joint_name_hashes_address = parent_indices_address + parent_indices_size;

	out_skeleton->tag = skelTag();
	out_skeleton->numJoints = num_joints;
    out_skeleton->offsetBasePose = TYPE_POINTER_GET_OFFSET( &out_skeleton->offsetBasePose, base_pose_address );
    out_skeleton->offsetParentIndices = TYPE_POINTER_GET_OFFSET( &out_skeleton->offsetParentIndices, parent_indices_address );
    out_skeleton->offsetJointNames = TYPE_POINTER_GET_OFFSET( &out_skeleton->offsetJointNames, joint_name_hashes_address );

    std::vector< bxAnim_Joint > world_bind_pose;
    world_bind_pose.resize( num_joints );
    const bxAnim_Joint* local_bind_pose = (bxAnim_Joint*)in_skeleton.basePose.data();
    const u16* parent_indices = in_skeleton.parentIndices.data();
    bxAnim::localJointsToWorldJoints( world_bind_pose.data(), local_bind_pose, parent_indices, num_joints, bxAnim_Joint::identity() );

	memcpy( base_pose_address, world_bind_pose.data(), base_pose_size );
	memcpy( parent_indices_address, &in_skeleton.parentIndices[0], parent_indices_size );
	memcpy( joint_name_hashes_address, &in_skeleton.jointNames[0], joint_name_hashes_size );

	int write_result = bxIO::writeFile( out_filename, memory, memory_size );

    freeMemory0( memory );

	return ( write_result == -1 ) ? false : true;
}
////
////
bool exportAnimation( const char* out_filename, const Animation& in_animation, const Skeleton& in_skeleton )
{
	const u32 num_joints = (u32)in_skeleton.jointNames.size();
	const u32 num_frames = in_animation.numFrames;
	const u32 channel_data_size = num_joints * num_frames * sizeof(float4_t);

	u32 memory_size = 0;
	memory_size += sizeof( bxAnim_Clip );
	memory_size += channel_data_size; // rotation
	memory_size += channel_data_size; // translation
	memory_size += channel_data_size; // scale

    u8* memory = (u8*)allocateMemory( memory_size, 16 );

    bxAnim_Clip* clip = (bxAnim_Clip*)memory;
    memset( clip, 0, sizeof( bxAnim_Clip ) );

	u8* rotation_address = (u8*)( clip + 1 );
	u8* translation_address = rotation_address + channel_data_size;
	u8* scale_address = translation_address + channel_data_size;

	clip->tag = animTag();
	clip->duration = in_animation.endTime - in_animation.startTime;
	clip->sampleFrequency = in_animation.sampleFrequency;
	clip->numJoints = num_joints;
	clip->numFrames = num_frames;
    clip->offsetRotationData = TYPE_POINTER_GET_OFFSET( &clip->offsetRotationData, rotation_address );
    clip->offsetTranslationData = TYPE_POINTER_GET_OFFSET( &clip->offsetTranslationData, translation_address );
    clip->offsetScaleData = TYPE_POINTER_GET_OFFSET( &clip->offsetScaleData, scale_address );

	std::vector<float4_t> rotation_vector;
	std::vector<float4_t> translation_vector;
	std::vector<float4_t> scale_vector;
	
	for( u32 i = 0 ; i < num_frames; ++i )
	{
		for( u32 j = 0; j < num_joints; ++j )
		{
			const AnimKeyframe& rotation_keyframe = in_animation.joints[j].rotation[i];
			const AnimKeyframe& translation_keyframe = in_animation.joints[j].translation[i];
			const AnimKeyframe& scale_keyframe = in_animation.joints[j].scale[i];

			rotation_vector.push_back( rotation_keyframe.data );
			translation_vector.push_back( translation_keyframe.data );
			scale_vector.push_back( scale_keyframe.data );
		}
	}

	SYS_ASSERT( channel_data_size == (rotation_vector.size() * sizeof(float4_t)) );
    SYS_ASSERT( channel_data_size == ( translation_vector.size() * sizeof( float4_t ) ) );
    SYS_ASSERT( channel_data_size == ( scale_vector.size() * sizeof( float4_t ) ) );

	memcpy( rotation_address, &rotation_vector[0], channel_data_size );
	memcpy( translation_address, &translation_vector[0], channel_data_size );
	memcpy( scale_address, &scale_vector[0], channel_data_size );

	bxIO::writeFile( out_filename, memory, memory_size );

    freeMemory0( memory );

	return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

inline void initAssImp()
{
    struct aiLogStream stream;
    stream = aiGetPredefinedLogStream( aiDefaultLogStream_STDOUT, NULL );
    aiAttachLogStream( &stream );
}
inline void deinitAssImp( const aiScene* scene )
{
    aiReleaseImport( scene );
    aiDetachAllLogStreams();
}

bool exportSkeleton( const char* out_filename, const char* in_filename )
{
    const unsigned import_flags = 0;
    initAssImp();
    const aiScene* aiscene = aiImportFile( in_filename, import_flags );
    if( !aiscene )
    {
        std::cout << "export skeleton failed: input file not found\n" << std::endl;
        return false;
    }

    Skeleton skel;
    _ExtractSkeleton( &skel, aiscene );

    const bool bres = exportSkeleton( out_filename, skel );
    deinitAssImp( aiscene );
    
    return bres;
}

bool exportAnimation( const char* out_filename, const char* in_filename, unsigned flags )
{
    const unsigned import_flags = 0;
    initAssImp(); 
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
    Skeleton skel;
    Animation anim;

    _ExtractSkeleton( &skel, aiscene );
    _ExtractAnimation( &anim, skel, aiscene, flags );

    const bool bres = exportAnimation( out_filename, anim, skel );
    deinitAssImp( aiscene );
    return bres;
}

}///
