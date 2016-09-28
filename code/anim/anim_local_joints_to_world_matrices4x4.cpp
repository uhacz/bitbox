#include "anim.h"
#include <util/debug.h>


void bxAnim::localJointsToWorldMatrices4x4( Matrix4* out_matrices, const bxAnim_Joint* in_joints, const unsigned short* parent_indices, unsigned count, const bxAnim_Joint& root_joint )
{
    Matrix4 root = Matrix4( root_joint.rotation, root_joint.position );
    root.setCol0( root.getCol0() * root_joint.scale.getX() );
    root.setCol1( root.getCol1() * root_joint.scale.getY() );
    root.setCol2( root.getCol2() * root_joint.scale.getZ() );


    Matrix4* out_transform = (Matrix4*)out_matrices;

    for( unsigned i = 0; i < count; ++i )
    {
        const u32 parent_idx = parent_indices[i];
        const bool is_root = parent_idx == 0xffff;

        const Matrix4& parent = ( is_root ) ? root : out_transform[parent_idx];
        
        const bxAnim_Joint& local_joint = in_joints[i];
        Vector3 scale_compensate = ( is_root ) ? root_joint.scale : in_joints[parent_idx].scale;
        scale_compensate = recipPerElem( scale_compensate );

        // Compute Output Matrix
        // worldMatrix = worldParent * localTranslate * localScaleCompensate * localRotation * localScale

        const Point3 local_translation( local_joint.position );

        Transform3 world(
            parent.getCol0().getXYZ() * scale_compensate.getX(),
            parent.getCol1().getXYZ() * scale_compensate.getY(),
            parent.getCol2().getXYZ() * scale_compensate.getZ(),
            ( parent * local_translation ).getXYZ()
            );

        Transform3 local_rotation_scale = Transform3::rotation( local_joint.rotation );
        local_rotation_scale.setCol0( local_rotation_scale.getCol0() * local_joint.scale.getX() );
        local_rotation_scale.setCol1( local_rotation_scale.getCol1() * local_joint.scale.getY() );
        local_rotation_scale.setCol2( local_rotation_scale.getCol2() * local_joint.scale.getZ() );

        world *= local_rotation_scale;
        out_transform[i] = Matrix4( world.getUpper3x3(), world.getTranslation() );

        //Transform3 local( local_joint.rotation, local_joint.position );
        //local.setCol0( local.getCol0() * local_joint.scale.getX() );
        //local.setCol1( local.getCol1() * local_joint.scale.getY() );
        //local.setCol2( local.getCol2() * local_joint.scale.getZ() );

        //out_transform[i] = parent * local;
    }
}

