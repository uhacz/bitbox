#pragma once

#include "anim_joint_transform.h"
#include "anim_struct.h"
#include "anim_common.h"

namespace bx{ namespace anim{

Context* contextInit( const Skel& skel );
void contextDeinit( Context** ctx );

void evaluateBlendTree( Context* ctx, const u16 root_index , const BlendBranch* blend_branches, unsigned int num_branches, const BlendLeaf* blend_leaves, unsigned int num_leaves );
void evaluateCommandList( Context* ctx,  const BlendBranch* blend_branches, unsigned int num_branches, const BlendLeaf* blend_leaves, unsigned int num_leaves );
void blendJointsLinear( Joint* out_joints, const Joint* left_joints, const Joint* right_joints, float blend_factor, unsigned short num_joints);

void evaluateClip( Joint* out_joints, const Clip* anim, f32 eval_time, u32 beginJoint = UINT32_MAX, u32 endJoint = UINT32_MAX );
void evaluateClip( Joint* out_joints, const Clip* anim, u32 frame_integer, f32 frame_fraction, u32 beginJoint = UINT32_MAX, u32 endJoint = UINT32_MAX );

void evaluateClipIndexed( Joint* out_joints, const Clip* anim, f32 eval_time, const i16* indices, u32 numIndices );
void evaluateClipIndexed( Joint* out_joints, const Clip* anim, u32 frame_integer, f32 frame_fraction, const i16* indices, u32 numIndices );


void localJointsToWorldMatrices4x4( Matrix4* out_matrices, const Joint* in_joints, const unsigned short* parent_indices, unsigned count, const Joint& root_joint );
void localJointsToWorldJoints( Joint* out_joints, const Joint* in_joints, const unsigned short* parent_indices, unsigned count, const Joint& root_joint );
//void localJointToWorldJoint( Joint* out_joints, const Joint* in_joints, const unsigned short* parent_indices, unsigned count, const Joint& root_joint );

}}///

namespace bx{ 

    class ResourceManager;
    
namespace anim_ext{

    anim::Skel* loadSkelFromFile( ResourceManager* resourceManager, const char* relativePath );
    anim::Clip* loadAnimFromFile( ResourceManager* resourceManager, const char* relativePath );

    void unloadSkelFromFile( ResourceManager* resourceManager, anim::Skel** skel );
    void unloadAnimFromFile( ResourceManager* resourceManager, anim::Clip** clip );

    void localJointsToWorldJoints( anim::Joint* outJoints, const anim::Joint* inJoints, const anim::Skel* skel, const anim::Joint& rootJoint );
    void localJointsToWorldMatrices( Matrix4* outMatrices, const anim::Joint* inJoints, const anim::Skel* skel, const anim::Joint& rootJoint );

    void processBlendTree( anim::Context* ctx, const u16 root_index, const anim::BlendBranch* blend_branches, unsigned int num_branches, const anim::BlendLeaf* blend_leaves, unsigned int num_leaves );

}}///
