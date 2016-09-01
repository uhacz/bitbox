#pragma once

#include "anim_joint_transform.h"
#include "anim_struct.h"
#include "anim_common.h"

namespace bxAnim
{

bxAnim_Context* contextInit( const bxAnim_Skel& skel );
void contextDeinit( bxAnim_Context** ctx );

void evaluateBlendTree( bxAnim_Context* ctx, const u16 root_index , const bxAnim_BlendBranch* blend_branches, unsigned int num_branches, const bxAnim_BlendLeaf* blend_leaves, unsigned int num_leaves, const bxAnim_Skel* skeleton );
void evaluateCommandList( bxAnim_Context* ctx,  const bxAnim_BlendBranch* blend_branches, unsigned int num_branches, const bxAnim_BlendLeaf* blend_leaves, unsigned int num_leaves, const bxAnim_Skel* skeleton );
void blendJointsLinear( bxAnim_Joint* out_joints, const bxAnim_Joint* left_joints, const bxAnim_Joint* right_joints, float blend_factor, unsigned short num_joints);

void evaluateClip( bxAnim_Joint* out_joints, const bxAnim_Clip* anim, f32 eval_time );
void evaluateClip( bxAnim_Joint* out_joints, const bxAnim_Clip* anim, u32 frame_integer, f32 frame_fraction );

void localJointsToWorldMatrices4x4( Matrix4* out_matrices, const bxAnim_Joint* in_joints, const unsigned short* parent_indices, unsigned count, const bxAnim_Joint& root_joint );
void localJointsToWorldJoints( bxAnim_Joint* out_joints, const bxAnim_Joint* in_joints, const unsigned short* parent_indices, unsigned count, const bxAnim_Joint& root_joint );

}///

class bxResourceManager;
namespace bxAnimExt
{
    bxAnim_Skel* loadSkelFromFile( bxResourceManager* resourceManager, const char* relativePath );
    bxAnim_Clip* loadAnimFromFile( bxResourceManager* resourceManager, const char* relativePath );

    void unloadSkelFromFile( bxResourceManager* resourceManager, bxAnim_Skel** skel );
    void unloadAnimFromFile( bxResourceManager* resourceManager, bxAnim_Clip** clip );

    void localJointsToWorldJoints( bxAnim_Joint* outJoints, const bxAnim_Joint* inJoints, const bxAnim_Skel* skel, const bxAnim_Joint& rootJoint );
    void localJointsToWorldMatrices( Matrix4* outMatrices, const bxAnim_Joint* inJoints, const bxAnim_Skel* skel, const bxAnim_Joint& rootJoint );
}///
