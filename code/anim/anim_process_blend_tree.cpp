#include "anim.h"

#include <util/debug.h>

namespace bxAnim{

static inline void pushCmdLeaf( bxAnim_Cmd* cmdArray, u8* cmdArraySize, u16 currentLeaf, const bxAnim_BlendLeaf* blendLeaves, unsigned int numLeaves, bxAnim::ECmdOp op )
{
	const u16 leaf_index = currentLeaf & (~bxAnim::eBLEND_TREE_LEAF);
	SYS_ASSERT( leaf_index < numLeaves );
	SYS_ASSERT( *cmdArraySize < bxAnim_Context::eCMD_ARRAY_SIZE );

	const bxAnim_BlendLeaf* leaf = blendLeaves + leaf_index;

	const u8 currentCmdArraySize = *cmdArraySize;
	cmdArray[currentCmdArraySize].leaf = leaf;
	cmdArray[currentCmdArraySize].command = op;
	*cmdArraySize = currentCmdArraySize + 1;
}
static inline void pushCmdBranch(  bxAnim_Cmd* cmdArray, u8* cmdArraySize, const bxAnim_BlendBranch* branch, bxAnim::ECmdOp op )
{
	SYS_ASSERT( *cmdArraySize < bxAnim_Context::eCMD_ARRAY_SIZE );
	SYS_ASSERT( op >= bxAnim::eCMD_OP_BLEND_STACK );

	const u8 currentCmdArraySize = *cmdArraySize;
	cmdArray[currentCmdArraySize].branch = branch;
	cmdArray[currentCmdArraySize].command = op;
	*cmdArraySize = currentCmdArraySize + 1;
}

static void traverseBlendTree( bxAnim_Cmd* cmdArray, u8* cmdArraySize, u16 currentBranch, const bxAnim_BlendBranch* blendBranches, unsigned int numBranches, const bxAnim_BlendLeaf* blendLeaves, unsigned int numLeaves )
{
	SYS_ASSERT( ( currentBranch & bxAnim::eBLEND_TREE_BRANCH ) != 0 );

    const u16 branchIndex = currentBranch & ( ~bxAnim::eBLEND_TREE_BRANCH );

	SYS_ASSERT( branchIndex < numBranches );

	const bxAnim_BlendBranch* branch = blendBranches + branchIndex;

    const u16 isLeftBranch = branch->left & bxAnim::eBLEND_TREE_BRANCH;
    const u16 isLeftLeaf = branch->left & bxAnim::eBLEND_TREE_LEAF;
    const u16 isRightBranch = branch->right & bxAnim::eBLEND_TREE_BRANCH;
    const u16 isRightLeaf = branch->right & bxAnim::eBLEND_TREE_LEAF;

	SYS_ASSERT( isLeftBranch != isLeftLeaf );
	SYS_ASSERT( isRightBranch != isRightLeaf );

	if( isLeftBranch )
	{
		traverseBlendTree( cmdArray, cmdArraySize, branch->left, blendBranches, numBranches, blendLeaves, numLeaves );
	}
	else if( isLeftLeaf )
	{
		const bxAnim::ECmdOp op = ( isRightLeaf ) ? bxAnim::eCMD_OP_EVAL : bxAnim::eCMD_OP_PUSH_AND_EVAL;
		pushCmdLeaf( cmdArray, cmdArraySize, branch->left, blendLeaves, numLeaves, op );
	}
	else
	{
		SYS_ASSERT( false );
	}

	if( isRightBranch )
	{
		traverseBlendTree( cmdArray, cmdArraySize, branch->right, blendBranches, numBranches, blendLeaves, numLeaves );
	}
	else if( isRightLeaf )
	{
        const bxAnim::ECmdOp op = ( isLeftLeaf ) ? bxAnim::eCMD_OP_EVAL : bxAnim::eCMD_OP_PUSH_AND_EVAL;
		pushCmdLeaf( cmdArray, cmdArraySize, branch->right, blendLeaves, numLeaves, op );
	}
	else
	{
		SYS_ASSERT( false );
	}

    const bxAnim::ECmdOp op = ( isLeftLeaf && isRightLeaf ) ? bxAnim::eCMD_OP_BLEND_CACHE : bxAnim::eCMD_OP_BLEND_STACK;
	pushCmdBranch( cmdArray, cmdArraySize, branch, op );

}

}//

namespace bxAnim{

void evaluateBlendTree( bxAnim_Context* ctx, const u16 root_index , const bxAnim_BlendBranch* blend_branches, unsigned int num_branches, const bxAnim_BlendLeaf* blend_leaves, unsigned int num_leaves, const bxAnim_Skel* skeleton )
{
	const u16 is_root_branch = root_index & bxAnim::eBLEND_TREE_BRANCH;
    const u16 is_root_leaf = root_index & bxAnim::eBLEND_TREE_LEAF;

	SYS_ASSERT( is_root_leaf != is_root_branch );

	u8 cmd_array_size = 0;
	bxAnim_Cmd* cmd_array = ctx->cmdArray;
	memset( cmd_array, 0, sizeof(bxAnim_Cmd) * bxAnim_Context::eCMD_ARRAY_SIZE );

	if( is_root_leaf )
	{
        pushCmdLeaf( cmd_array, &cmd_array_size, root_index, blend_leaves, num_leaves, bxAnim::eCMD_OP_PUSH_AND_EVAL );
	}
	else
	{
		traverseBlendTree( cmd_array, &cmd_array_size, root_index, blend_branches, num_branches, blend_leaves, num_leaves );
	}

	SYS_ASSERT( cmd_array_size < bxAnim_Context::eCMD_ARRAY_SIZE );

	bxAnim_Cmd* last_cmd = cmd_array + cmd_array_size;
    last_cmd->command = bxAnim::eCMD_OP_END_LIST;
	++cmd_array_size;

	ctx->cmdArraySize = cmd_array_size;
}

void evaluateCommandList( bxAnim_Context* ctx, const bxAnim_Cmd* cmdList,  const bxAnim_BlendBranch* blendBranches, unsigned int numBranches, const bxAnim_BlendLeaf* blendLeaves, unsigned int numLeaves, const bxAnim_Skel* skeleton )
{
	u8 counter = 0;

	ctx->poseStackIndex = -1;
	
    while( cmdList->command != bxAnim::eCMD_OP_END_LIST )
	{
		const u16 cmd = cmdList->command;

		switch( cmd )
		{
        case bxAnim::eCMD_OP_EVAL:
			{
				bxAnim_Joint* joints = allocateTmpPose( ctx );
                bxAnim_Clip* clip = (bxAnim_Clip*)cmdList->leaf->anim;
				evaluateClip( joints, clip, cmdList->leaf->evalTime );
				break;
			}
        case bxAnim::eCMD_OP_PUSH_AND_EVAL:
			{
				poseStackPush( ctx );
				bxAnim_Joint* joints = poseFromStack( ctx, 0 );
                bxAnim_Clip* clip = (bxAnim_Clip*)cmdList->leaf->anim;
				evaluateClip( joints, clip, cmdList->leaf->evalTime );
				break;
			}
        case bxAnim::eCMD_OP_BLEND_STACK:
			{
				bxAnim_Joint* leftJoints  = poseFromStack( ctx, -1 );
				bxAnim_Joint* rightJoints = poseFromStack( ctx, 0 );

				poseStackPush( ctx );
				bxAnim_Joint* outJoints = poseFromStack( ctx, 0 );

				blendJointsLinear( outJoints, leftJoints, rightJoints, cmdList->branch->alpha, skeleton->numJoints );

				break;
			}
        case bxAnim::eCMD_OP_BLEND_CACHE:
			{
				bxAnim_Joint* leftJoints  = ctx->poseCache[0];
				bxAnim_Joint* rightJoints = ctx->poseCache[1];
				
				poseStackPush( ctx );
				bxAnim_Joint* outJoints = poseFromStack( ctx, 0 );

				blendJointsLinear( outJoints, leftJoints, rightJoints, cmdList->branch->alpha, skeleton->numJoints );
				break;
			}
		}
		++cmdList;
	}
}

}///

