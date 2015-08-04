#pragma once

#include <util/type.h>

struct BIT_ALIGNMENT_16 bxAnim_Skel
{
	u32 tag;
	u16 numJoints;
	u16 pad0__[1];
	u32 offsetBasePose;
	u32 offsetParentIndices;
	u32 offsetJointNames;
	u32 pad1__[3];
};

struct BIT_ALIGNMENT_16 bxAnim_Clip
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

struct BIT_ALIGNMENT_16 bxAnim_BlendBranch
{
	inline bxAnim_BlendBranch( u16 left_index, u16 right_index, f32 blend_alpha, u16 f = 0 )
		: left( left_index ), right( right_index ), alpha( blend_alpha ), flags(f)
	{}
	
    inline bxAnim_BlendBranch()
		: left(0), right(0), flags(0), alpha(0.f)
	{}
	u16 left;
	u16 right;
	u16 flags;
	u16 pad0__[1];
	f32 alpha;
	u32 pad1__[1];
};

struct BIT_ALIGNMENT_16 bxAnim_BlendLeaf
{
	inline bxAnim_BlendLeaf( bxAnim_Clip* clip, f32 time )
		: anim( (uptr)clip ), evalTime( time )
	{}
	inline bxAnim_BlendLeaf()
		: anim(0), evalTime(0.f)
	{}

	uptr anim;
	f32 evalTime;
	u32 pad0__[1];
};

struct bxAnim_Cmd
{
	u16 command;		/* see AnimCommandOp */
	u16 arg0;			/* helper argument used by some commands */
	union
	{
		const bxAnim_BlendLeaf*	  leaf;
		const bxAnim_BlendBranch* branch;
	};
};

struct bxAnim_Joint;
struct BIT_ALIGNMENT_16 bxAnim_Context
{
	enum
	{
		ePOSE_CACHE_SIZE = 2,
		ePOSE_STACK_SIZE = 4,
		eCMD_ARRAY_SIZE = 64,
	};

	bxAnim_Joint* poseCache[ePOSE_CACHE_SIZE];
	bxAnim_Joint* poseStack[ePOSE_STACK_SIZE];
	bxAnim_Cmd* cmdArray;
	u8 poseCacheIndex;
	u8 poseStackIndex;
	u8 cmdArraySize;
	u8 __pad0[1];
};

namespace bxAnim
{
    enum EBlendTreeIndex
    {
        eBLEND_TREE_LEAF = 0x8000,
        eBLEND_TREE_BRANCH = 0x4000,
    };

    enum ECmdOp
    {
        eCMD_OP_END_LIST = 0,	/* end of command list marker */

        eCMD_OP_EVAL,			/* evaluate anim to top of pose cache*/
        eCMD_OP_PUSH_AND_EVAL,  /* evaluate anim to top of pose stack*/

        eCMD_OP_BLEND_STACK,	/* blend poses from stack*/
        eCMD_OP_BLEND_CACHE,	/* blend poses from cache */
    };


    inline bxAnim_Joint* allocateTmpPose( bxAnim_Context* ctx )
    {
        const u8 current_index = ctx->poseCacheIndex;
        ctx->poseCacheIndex = ( current_index + 1 ) % bxAnim_Context::ePOSE_CACHE_SIZE;
        return ctx->poseCache[current_index];
    }

    inline u8 poseStackPush( bxAnim_Context* ctx )
    {
        ctx->poseStackIndex = ( ctx->poseStackIndex + 1 ) % bxAnim_Context::ePOSE_CACHE_SIZE;
        return ctx->poseStackIndex;
    }
    inline u8 poseStackIndex( const bxAnim_Context* ctx, i8 depth )
    {
        const i8 iindex = (i8)ctx->poseStackIndex + depth;
        const u8 index = (u8)iindex % bxAnim_Context::ePOSE_STACK_SIZE;
        return index;
    }
    inline bxAnim_Joint* poseFromStack( const bxAnim_Context* ctx, i8 depth )
    {
        const u8 index = poseStackIndex( ctx, depth );
        return ctx->poseStack[index];
    }

}///


