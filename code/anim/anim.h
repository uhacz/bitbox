#pragma once

#include "anim_joint_transform.h"
#include "anim_struct.h"
#include "anim_common.h"

namespace bxAnim
{

bxAnim_Context* context_init( const bxAnim_Skel& skel );
void context_deinit( bxAnim_Context** ctx );

void blendTree_process( bxAnim_Context* ctx, const u16 root_index , const bxAnim_BlendBranch* blend_branches, unsigned int num_branches, const bxAnim_BlendLeaf* blend_leaves, unsigned int num_leaves, const bxAnim_Skel* skeleton );
void commandList_process( bxAnim_Context* ctx, const bxAnim_Cmd* command_list,  const bxAnim_BlendBranch* blend_branches, unsigned int num_branches, const bxAnim_BlendLeaf* blend_leaves, unsigned int num_leaves, const bxAnim_Skel* skeleton );
void blendJointsLinear( bxAnim_Joint* out_joints, const bxAnim_Joint* left_joints, const bxAnim_Joint* right_joints, float blend_factor, unsigned short num_joints);

void evaluate( bxAnim_Joint* out_joints, const bxAnim_Clip* anim, f32 eval_time );
void evaluate( bxAnim_Joint* out_joints, const bxAnim_Clip* anim, u32 frame_integer, f32 frame_fraction );

void localJointsToWorldMatrices4x4( Matrix4* out_matrices, const bxAnim_Joint* in_joints, const unsigned short* parent_indices, unsigned count, const bxAnim_Joint& root_joint );
void localJointsToWorldJoints( bxAnim_Joint* out_joints, const bxAnim_Joint* in_joints, const unsigned short* parent_indices, unsigned count, const bxAnim_Joint& root_joint );

}///

namespace bxAnimExt
{
    bxAnim_Skel* loadSkelFromFile( bxResourceManager* resourceManager, const char* relativePath );
    bxAnim_Clip* loadAnimFromFile( bxResourceManager* resourceManager, const char* relativePath );
}///
