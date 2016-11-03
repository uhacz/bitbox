#include "anim.h"
#include <util/debug.h>

namespace bx{ namespace anim{

static inline void pushCmdLeaf( Cmd* cmdArray, u8* cmdArraySize, u16 currentLeaf, const BlendLeaf* blendLeaves, unsigned int numLeaves, ECmdOp::Enum op )
{
	const u16 leaf_index = currentLeaf & (~EBlendTreeIndex::LEAF);
	SYS_ASSERT( leaf_index < numLeaves );
	SYS_ASSERT( *cmdArraySize < Context::eCMD_ARRAY_SIZE );

	const BlendLeaf* leaf = blendLeaves + leaf_index;

	const u8 currentCmdArraySize = *cmdArraySize;
	cmdArray[currentCmdArraySize].leaf = leaf;
	cmdArray[currentCmdArraySize].command = op;
	*cmdArraySize = currentCmdArraySize + 1;
}
static inline void pushCmdBranch(  Cmd* cmdArray, u8* cmdArraySize, const BlendBranch* branch, ECmdOp::Enum op )
{
	SYS_ASSERT( *cmdArraySize < Context::eCMD_ARRAY_SIZE );
	SYS_ASSERT( op >= ECmdOp::BLEND_STACK );

	const u8 currentCmdArraySize = *cmdArraySize;
	cmdArray[currentCmdArraySize].branch = branch;
	cmdArray[currentCmdArraySize].command = op;
	*cmdArraySize = currentCmdArraySize + 1;
}

static void traverseBlendTree( Cmd* cmdArray, u8* cmdArraySize, u16 currentBranch, const BlendBranch* blendBranches, unsigned int numBranches, const BlendLeaf* blendLeaves, unsigned int numLeaves )
{
	SYS_ASSERT( ( currentBranch & EBlendTreeIndex::BRANCH ) != 0 );

    const u16 branchIndex = currentBranch & ( ~EBlendTreeIndex::BRANCH );

	SYS_ASSERT( branchIndex < numBranches );

	const BlendBranch* branch = blendBranches + branchIndex;

    const u16 isLeftBranch  = branch->left  & EBlendTreeIndex::BRANCH;
    const u16 isLeftLeaf    = branch->left  & EBlendTreeIndex::LEAF;
    const u16 isRightBranch = branch->right & EBlendTreeIndex::BRANCH;
    const u16 isRightLeaf   = branch->right & EBlendTreeIndex::LEAF;

	SYS_ASSERT( isLeftBranch != isLeftLeaf );
	SYS_ASSERT( isRightBranch != isRightLeaf );

	if( isLeftBranch )
	{
		traverseBlendTree( cmdArray, cmdArraySize, branch->left, blendBranches, numBranches, blendLeaves, numLeaves );
	}
	else if( isLeftLeaf )
	{
		const ECmdOp::Enum op = ( isRightLeaf ) ? ECmdOp::EVAL : ECmdOp::PUSH_AND_EVAL;
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
        const ECmdOp::Enum op = ( isLeftLeaf ) ? ECmdOp::EVAL : ECmdOp::PUSH_AND_EVAL;
		pushCmdLeaf( cmdArray, cmdArraySize, branch->right, blendLeaves, numLeaves, op );
	}
	else
	{
		SYS_ASSERT( false );
	}

    const ECmdOp::Enum op = ( isLeftLeaf && isRightLeaf ) ? ECmdOp::BLEND_CACHE : ECmdOp::BLEND_STACK;
	pushCmdBranch( cmdArray, cmdArraySize, branch, op );

}

void evaluateBlendTree( Context* ctx, const u16 root_index , const BlendBranch* blend_branches, unsigned int num_branches, const BlendLeaf* blend_leaves, unsigned int num_leaves )
{
	const u16 is_root_branch = root_index & EBlendTreeIndex::BRANCH;
    const u16 is_root_leaf   = root_index & EBlendTreeIndex::LEAF;

	SYS_ASSERT( is_root_leaf != is_root_branch );

	u8 cmd_array_size = 0;
	Cmd* cmd_array = ctx->cmdArray;
	memset( cmd_array, 0, sizeof(Cmd) * Context::eCMD_ARRAY_SIZE );

	if( is_root_leaf )
	{
        pushCmdLeaf( cmd_array, &cmd_array_size, root_index, blend_leaves, num_leaves, ECmdOp::PUSH_AND_EVAL );
	}
	else
	{
		traverseBlendTree( cmd_array, &cmd_array_size, root_index, blend_branches, num_branches, blend_leaves, num_leaves );
	}

	SYS_ASSERT( cmd_array_size < Context::eCMD_ARRAY_SIZE );

	Cmd* last_cmd = cmd_array + cmd_array_size;
    last_cmd->command = ECmdOp::END_LIST;
	++cmd_array_size;

	ctx->cmdArraySize = cmd_array_size;
}

void evaluateCommandList( Context* ctx,  const BlendBranch* blendBranches, unsigned int numBranches, const BlendLeaf* blendLeaves, unsigned int numLeaves )
{
    const Cmd* cmdList = ctx->cmdArray;
    u8 counter = 0;
    
    ctx->poseStackIndex = -1;
	
    while( cmdList->command != ECmdOp::END_LIST )
	{
		const u16 cmd = cmdList->command;

		switch( cmd )
		{
        case ECmdOp::EVAL:
			{
				Joint* joints = allocateTmpPose( ctx );
                Clip* clip = (Clip*)cmdList->leaf->anim;
				evaluateClip( joints, clip, cmdList->leaf->evalTime );
				break;
			}
        case ECmdOp::PUSH_AND_EVAL:
			{
				poseStackPush( ctx );
				Joint* joints = poseFromStack( ctx, 0 );
                Clip* clip = (Clip*)cmdList->leaf->anim;
				evaluateClip( joints, clip, cmdList->leaf->evalTime );
				break;
			}
        case ECmdOp::BLEND_STACK:
			{
				Joint* leftJoints  = poseFromStack( ctx, 1 );
				Joint* rightJoints = poseFromStack( ctx, 0 );
                //poseStackPush( ctx );
				Joint* outJoints = poseFromStack( ctx, 1 );

				blendJointsLinear( outJoints, leftJoints, rightJoints, cmdList->branch->alpha, ctx->numJoints );
                
                poseStackPop( ctx );
				break;
			}
        case ECmdOp::BLEND_CACHE:
			{
				Joint* leftJoints  = ctx->poseCache[0];
				Joint* rightJoints = ctx->poseCache[1];
				
				poseStackPush( ctx );
				Joint* outJoints = poseFromStack( ctx, 0 );

                blendJointsLinear( outJoints, leftJoints, rightJoints, cmdList->branch->alpha, ctx->numJoints );
				break;
			}
		}
		++cmdList;
	}
}

}}///

