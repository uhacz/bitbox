#pragma once

#include <util/type.h>
#include <util/debug.h>

namespace bx{ namespace anim{

struct BIT_ALIGNMENT_16 Skel
{
	u32 tag;
	u16 numJoints;
	u16 pad0__[1];
	u32 offsetBasePose;
	u32 offsetParentIndices;
	u32 offsetJointNames;
	u32 pad1__[3];
};

struct BIT_ALIGNMENT_16 Clip
{
	u32 tag;
	f32 duration;
	f32 sampleFrequency;
	u16 numJoints;
	u16 numFrames;
	u32 offsetRotationData;
	u32 offsetTranslationData;
	u32 offsetScaleData;
	u32 pad0__[1];
};

struct BIT_ALIGNMENT_16 BlendBranch
{
	inline BlendBranch( u16 left_index, u16 right_index, f32 blend_alpha, u16 f = 0 )
		: left( left_index ), right( right_index ), alpha( blend_alpha ), flags(f)
	{}
	
    inline BlendBranch()
		: left(0), right(0), flags(0), alpha(0.f)
	{}
	u16 left;
	u16 right;
	u16 flags;
	u16 pad0__[1];
	f32 alpha;
	u32 pad1__[1];
};

struct BIT_ALIGNMENT_16 BlendLeaf
{
	inline BlendLeaf( const Clip* clip, f32 time )
		: anim( (uptr)clip ), evalTime( time )
	{}
	inline BlendLeaf()
		: anim(0), evalTime(0.f)
	{}

	uptr anim;
	f32 evalTime;
	u32 pad0__[1];
};

struct Cmd
{
	u16 command;		/* see AnimCommandOp */
	u16 arg0;			/* helper argument used by some commands */
	union
	{
		const BlendLeaf*	  leaf;
		const BlendBranch* branch;
	};
};

struct Joint;
struct BIT_ALIGNMENT_16 Context
{
	enum
	{
		ePOSE_CACHE_SIZE = 2,
		ePOSE_STACK_SIZE = 4,
		eCMD_ARRAY_SIZE = 64,
	};

	Joint* poseCache[ePOSE_CACHE_SIZE];
	Joint* poseStack[ePOSE_STACK_SIZE];
	Cmd* cmdArray = nullptr;
	u8 poseCacheIndex = 0;
	u8 poseStackIndex = 0;
	u8 cmdArraySize = 0;
	u8 __pad0[1];
    u16 numJoints = 0;
};

 
namespace EBlendTreeIndex
{
    enum Enum
    {
        LEAF = 0x8000,
        BRANCH = 0x4000,
    };
};

namespace ECmdOp
{
    enum Enum
    {
        END_LIST = 0,	/* end of command list marker */

        EVAL,			/* evaluate anim to top of pose cache*/
        PUSH_AND_EVAL,  /* evaluate anim to top of pose stack*/

        BLEND_STACK,	/* blend poses from stack*/
        BLEND_CACHE,	/* blend poses from cache */
    };
};


inline Joint* allocateTmpPose( Context* ctx )
{
    const u8 current_index = ctx->poseCacheIndex;
    ctx->poseCacheIndex = ( current_index + 1 ) % Context::ePOSE_CACHE_SIZE;
    return ctx->poseCache[current_index];
}

inline void poseStackPop( Context* ctx )
{
    SYS_ASSERT( ctx->poseStackIndex > 0 );
    --ctx->poseStackIndex;
}

inline u8 poseStackPush( Context* ctx )
{
    ctx->poseStackIndex = ( ctx->poseStackIndex + 1 ) % Context::ePOSE_STACK_SIZE;
    return ctx->poseStackIndex;
}

/// depth: how many poses we going back in stack (stackIndex decrease)
inline u8 poseStackIndex( const Context* ctx, i8 depth )
{
    const i8 iindex = (i8)ctx->poseStackIndex - depth;
    SYS_ASSERT( iindex < Context::ePOSE_STACK_SIZE );
    SYS_ASSERT( iindex >= 0 );
    //const u8 index = (u8)iindex % bxAnim_Context::ePOSE_STACK_SIZE;
    return iindex;
}
inline Joint* poseFromStack( const Context* ctx, i8 depth )
{
    const u8 index = poseStackIndex( ctx, depth );
    return ctx->poseStack[index];
}

}}///
