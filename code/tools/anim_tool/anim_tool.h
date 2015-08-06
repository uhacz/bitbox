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
		std::vector<u16> parentIndices;
		std::vector<Joint> basePose;
		std::vector<u32> jointNames;
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
		float startTime;
		float endTime;
		float sampleFrequency;
		u32 numFrames;

		std::vector< JointAnimation > joints;
	};

	u32 skelTag();
	u32 animTag();

	bool exportSkeleton( const char* out_filename, const Skeleton& in_skeleton );
    bool exportAnimation( const char* out_filename, const Animation& in_animation, const Skeleton& in_skeleton );

    enum EExportFlags
    {
        eEXPORT_REMOVE_ROOT_TRANSLATION_X = BIT_OFFSET( 0 ),
        eEXPORT_REMOVE_ROOT_TRANSLATION_Y = BIT_OFFSET( 1 ),
        eEXPORT_REMOVE_ROOT_TRANSLATION_Z = BIT_OFFSET( 2 ),
    };

    bool exportSkeleton( const char* out_filename, const char* in_filename );
    bool exportAnimation( const char* out_filename, const char* in_filename, unsigned flags );

}//