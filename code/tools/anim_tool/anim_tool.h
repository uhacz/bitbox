#pragma once

#include <util/type.h>
#include <vector>

namespace animTool
{
	struct Joint
	{
		float4_t rotation;
        float4_t translation;
        float4_t scale;
	};

	struct Skeleton
	{
		std::vector<u16> parent_indices;
		std::vector<Joint> base_pose;
		std::vector<u32> joint_name_hashes;
	};

	struct AnimKeyframe
	{
		f32 time;
        float4_t data;
	};

	struct JointAnimation
	{
		u32 name_hash;
		float weight;

		std::vector<AnimKeyframe> rotation;
		std::vector<AnimKeyframe> translation;
		std::vector<AnimKeyframe> scale;
	};

	struct Animation
	{
		float start_time;
		float end_time;
		float sample_frequency;
		u32 num_frames;

		std::vector< JointAnimation > joints;
	};

	u32 generate_skeleton_tag();
	u32 generate_animation_tag();

	bool export_skeleton( const i8* out_filename, const Skeleton& in_skeleton );
	bool export_animation( const i8* out_filename, const Animation& in_animation, const Skeleton& in_skeleton );

    bool export_skeleton( const i8* out_filename, const i8* in_filename );
    bool export_animation( const i8* out_filename, const i8* in_filename );

}//