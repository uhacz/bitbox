#include "anim.h"
#include <util/debug.h>


void bxAnim::localJointsToWorldMatrices4x4( Matrix4* out_matrices, const bxAnim_Joint* in_joints, const unsigned short* parent_indices, unsigned count, const bxAnim_Joint& root_joint )
{
    Transform3 root = Transform3( root_joint.rotation, root_joint.position );
    root.setCol0( root.getCol0() * root_joint.scale.getX() );
    root.setCol1( root.getCol1() * root_joint.scale.getY() );
    root.setCol2( root.getCol2() * root_joint.scale.getZ() );


    Transform3* out_transform = (Transform3*)out_matrices;

    for( unsigned i = 0; i < count; ++i )
    {
        const u32 parent_idx = parent_indices[i];
        const bool is_root = parent_idx == 0xffff;

        const Transform3& parent = ( is_root ) ? root : out_transform[parent_idx];
        
        const bxAnim_Joint& local_joint = in_joints[i];
        Transform3 local( local_joint.rotation, local_joint.position );
        local.setCol0( local.getCol0() * local_joint.scale.getX() );
        local.setCol1( local.getCol1() * local_joint.scale.getY() );
        local.setCol2( local.getCol2() * local_joint.scale.getZ() );

        out_transform[i] = parent * local;
    }
}

