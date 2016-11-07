#include "motion_matching.h"
#include <anim/anim.h>
#include <resource_manager/resource_manager.h>
#include <algorithm>
#include <xfunctional>
#include <gfx/gfx_debug_draw.h>

#include <util/common.h>
#include <util/buffer_utils.h>
#include "util/signal_filter.h"
#include "gfx/gui/imgui/imgui.h"

namespace bx{
namespace motion_matching{

namespace utils
{
    void debugDrawJoints( const anim::Joint* joints, const u16* parentIndices, u32 numJoints, u32 color, float radius, float scale )
    {
        for( u32 i = 0; i < numJoints; ++i )
        {
            Vector4 sph( joints[i].position * scale, radius );
            //bxGfxDebugDraw::addSphere( sph, color, 1 );

            const u16 parent_idx = parentIndices[i];
            if( parent_idx != 0xFFFF )
            {
                const Vector3 parent_pos = joints[parent_idx].position;
                bxGfxDebugDraw::addLine( parent_pos, sph.getXYZ(), color, 1 );
            }
        }
    }
    void debugDrawMatrices( const Matrix4* matrices, const u16* parentIndices, u32 numMatrices, u32 color, float radius )
    {
        for( u32 i = 0; i < numMatrices; ++i )
        {
            Vector4 sph( matrices[i].getTranslation(), radius );
            //bxGfxDebugDraw::addSphere( sph, color, 1 );
            bxGfxDebugDraw::addAxes( appendScale( matrices[i], Vector3(radius) ) );

            const u16 parent_idx = parentIndices[i];
            if( parent_idx != 0xFFFF )
            {
                const Vector3 parent_pos = matrices[parent_idx].getTranslation();
                bxGfxDebugDraw::addLine( parent_pos, sph.getXYZ(), color, 1 );
            }
        }
    }

    void computeTrajectoryDerivatives( Vector3 v[eNUM_TRAJECTORY_POINTS - 1], Vector3 a[eNUM_TRAJECTORY_POINTS - 2], const Vector3 x[eNUM_TRAJECTORY_POINTS], float deltaTimeInv )
    {
        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS - 1; ++i )
            v[i] = ( x[i + 1] - x[i] ) * deltaTimeInv;
        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS - 2; ++i )
            a[i] = ( v[i + 1] - v[i] ) * deltaTimeInv;
    }

    float computePositionCost( const Vector3* pos0, const Vector3* pos1, u32 numPos )
    {
        floatInVec result( 0 );
        for( u32 i = 0; i < numPos; ++i )
        {
            result += length( pos0[i] - pos1[i] );
        }

        return ( result.getAsFloat() );
    }
    float computeDirectionCost( const Vector3* dir0, const Vector3* dir1, u32 n )
    {
        floatInVec result( 0.f );
        for( u32 i = 0; i < n; ++i )
        {
            result += lengthSqr( normalizeSafe( dir0[i] ) - normalizeSafe( dir1[i] ) );
        }

        return ::sqrt( result.getAsFloat() );
    }

    float computeVelocityCost( const Vector3* vel0, const Vector3* vel1, u32 numPos, float deltaTime )
    {
        floatInVec result( 0 );
        const floatInVec dt( deltaTime );
        for( u32 i = 0; i < numPos; ++i )
        {
            result += length( vel0[i] - vel1[i] ) * dt;
        }

        return ( result.getAsFloat() );
    }
    inline float computeVelocityCost( const Vector3 vA, const Vector3 vB )
    {
        floatInVec cost = length( vA - vB );
        //cost += oneVec - dot( normalizeSafe( vA ), normalizeSafe( vB ) );
        return cost.getAsFloat();
    }
    void resampleTrajectory( Vector3* dstPoints, const Vector3* srcPoints, Curve3D& tmpCurve )
    {
        curve::clear( tmpCurve );
        float lengths[eNUM_TRAJECTORY_POINTS - 1] = {};
        float trajectory_length = 0.f;
        for( u32 j = 1; j < eNUM_TRAJECTORY_POINTS; ++j )
        {
            lengths[j - 1] = length( srcPoints[j] - srcPoints[j - 1] ).getAsFloat();
            trajectory_length += lengths[j - 1];
        }

        if( trajectory_length < FLT_EPSILON )
        {
            for( u32 j = 0; j < eNUM_TRAJECTORY_POINTS; ++j )
            {
                dstPoints[j] = srcPoints[j];
            }
        }
        else
        {
            const float trajectory_length_inv = ( trajectory_length > FLT_EPSILON ) ? 1.f / trajectory_length : 0.f;
            float current_length = 0.f;
            for( u32 j = 0; j < eNUM_TRAJECTORY_POINTS; ++j )
            {
                if( j > 0 )
                {
                    current_length += lengths[j-1];
                }
                float t = current_length * trajectory_length_inv;
                curve::push_back( tmpCurve, srcPoints[j], t );
            }
            for( u32 j = 0; j < eNUM_TRAJECTORY_POINTS; ++j )
            {
                dstPoints[j] = curve::evaluate_catmullrom( tmpCurve, (float)j / (float)( eNUM_TRAJECTORY_POINTS - 1 ) );
            }
        }
    }

    Matrix3 computeBaseMatrix3( const Vector3 direction, const Vector3 acc, const Vector3 upReference )
    {
        if( lengthSqr( direction ).getAsFloat() > FLT_EPSILON )
        {
            Vector3 y = upReference;
            Vector3 x = normalize( cross( y, direction ) );
            Vector3 z = normalize( cross( x, y ) );
            Matrix3 rotation = Matrix3( x, y, z );

            floatInVec d = dot( acc, x );
            Matrix3 tilt_rotation = Matrix3::rotationZ( -d );
            rotation *= tilt_rotation;

            d = dot( acc, z );
            tilt_rotation = Matrix3::rotationX( d );
            rotation *= tilt_rotation;

            return rotation;
        }
        return Matrix3::identity();
    }

    float computeTrajectoryCost( const Vector3 pointsA[eNUM_TRAJECTORY_POINTS], const Vector3 pointsB[eNUM_TRAJECTORY_POINTS] )
    {
        float cost = computePositionCost( pointsA+1, pointsB+1, eNUM_TRAJECTORY_POINTS-1 );
        return cost;
    }
    inline Vector3 removeRootMotion( const Vector3 pos )
    {
        return mulPerElem( pos, Vector3( 0.f, 1.f, 0.f ) );
    }

}///

State::State()
{
    //memset( clip_index, 0xff, sizeof( clip_index ) );
    //memset( clip_eval_time, 0x00, sizeof( clip_eval_time ) );
    //memset( anim_blend, 0x00, sizeof( anim_blend ) );
}

void poseAllocate( Pose* pose, u32 numJoints, bxAllocator* allocator )
{
    /// I've left this function for future use
}

void poseFree( Pose* pose, bxAllocator* allocator )
{
    pose[0] = {};
}

void posePrepare( Pose* pose, const PosePrepareInfo& info )
{
    const anim::Clip* clip = info.clip;
    const anim::Skel* skel = info.skel;
    const u32 frameNo = info.frameNo;
    const Vector4 plane = makePlane( Vector3::yAxis(), Vector3( 0.f ) );

    std::vector< anim::Joint > frame_joints_0;
    std::vector< anim::Joint > frame_joints_1;
    std::vector< anim::Joint > tmp_joints;
    frame_joints_0.resize( skel->numJoints );
    frame_joints_1.resize( skel->numJoints );
    tmp_joints.resize( skel->numJoints );

    anim::Joint j0[_eMATCH_JOINT_COUNT_], j1[_eMATCH_JOINT_COUNT_];
    const u32 lastFrame = info.clip->numFrames - 1;
    if( frameNo == lastFrame )
    {
        evaluateClip( frame_joints_0.data(), clip, frameNo - 1, 0.f );
        evaluateClip( frame_joints_1.data(), clip, frameNo, 0.f );
    }
    else
    {
        evaluateClip( frame_joints_0.data(), clip, frameNo, 0.f );
        evaluateClip( frame_joints_1.data(), clip, frameNo + 1, 0.f );
    }

    {
        anim::Joint root_joint = anim::Joint::identity();
        {
            const Vector3 p0 = projectVectorOnPlane( frame_joints_0[0].position, plane );
            const Vector3 p1 = projectVectorOnPlane( frame_joints_1[0].position, plane );
            pose->local_velocity = ( p1 - p0 ) * clip->sampleFrequency;
        }

        frame_joints_0[0].position = utils::removeRootMotion( frame_joints_0[0].position );
        frame_joints_1[0].position = utils::removeRootMotion( frame_joints_1[0].position );

        anim_ext::localJointsToWorldJoints( tmp_joints.data(), frame_joints_0.data(), skel, root_joint );
        for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
            j0[i] = tmp_joints[info.joint_indices[i]];

        anim_ext::localJointsToWorldJoints( tmp_joints.data(), frame_joints_1.data(), skel, root_joint );
        for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
            j1[i] = tmp_joints[info.joint_indices[i]];
    }

    //bxAnim_Joint current_joint;
    //bxAnim::evaluateClip( &current_joint, clip, frameNo, 0.f, 0, 0 );
    
    for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
    {
        const Vector3 v0 = j0[i].position; // - current_displacement_xz;
        const Vector3 v1 = j1[i].position; // - current_displacement_xz;

        if( frameNo == lastFrame )
        {
            pose->pos[i] = v1;
        }
        else
        {
            pose->pos[i] = v0;
        }

        
        pose->vel[i] = ( v1 - v0 ) * clip->sampleFrequency;
    }
        
    const float frameTime = 0.f; // ( info.is_loop ) ? 0.f : (float)frameNo / clip->sampleFrequency;
    const float duration = clip->duration; // ( info.is_loop ) ? clip->duration : maxOfPair( clip->duration - frameTime, 1.f / clip->sampleFrequency );
    computeClipTrajectory( &pose->trajectory, clip, frameTime, duration );

    /// flags
    {
        const float ZERO_VELOCITY_THRESHOLD = 0.1f;
        const u32 frameA = info.frameNo;
        const u32 frameB = ( frameA + 1 ) % clip->numFrames;

        anim::evaluateClip( frame_joints_0.data(), clip, frameA, 0.f );
        anim::evaluateClip( frame_joints_1.data(), clip, frameB, 0.f );
        
        Vector3 pos0[_eMATCH_JOINT_COUNT_];
        Vector3 pos1[_eMATCH_JOINT_COUNT_];
        anim_ext::localJointsToWorldJoints( tmp_joints.data(), frame_joints_0.data(), skel, anim::Joint::identity() );
        const Vector3 frame0_displacement_xz = projectVectorOnPlane( tmp_joints[0].position, plane );
        
        for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
            pos0[i] = tmp_joints[info.joint_indices[i]].position;
                
        anim_ext::localJointsToWorldJoints( tmp_joints.data(), frame_joints_1.data(), skel, anim::Joint::identity() );
        for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
            pos1[i] = tmp_joints[info.joint_indices[i]].position;


        for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
            pose->flags[i] = 0;

        for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
        {
            const Vector3 p0 = pos0[i] - frame0_displacement_xz;
            const Vector3 p1 = pos1[i] - frame0_displacement_xz;
            const Vector3 v = ( p1 - p0 ) * clip->sampleFrequency;

            const float vlen = length( v ).getAsFloat();
            if( vlen < ZERO_VELOCITY_THRESHOLD )
            {
                pose->flags[i] = 1;
            }

        }

    }

    //Curve3D curve3D = {};
    //curve::allocate( curve3D, eNUM_TRAJECTORY_POINTS );
    //utils::resampleTrajectory( pose->trajectory.pos, pose->trajectory.pos, curve3D );
    //curve::deallocate( curve3D );
}
//-------------------------------------------------------------------
void stateAllocate( State* state, u32 numJoints, bxAllocator* allocator )
{
    u32 mem_size = 0;
    mem_size = numJoints * ( sizeof( *state->scratch_joints) + sizeof( *state->joint_world ) + sizeof( *state->matrix_world ) );

    state->_memory_handle = BX_MALLOC( allocator, mem_size, 16 );
    memset( state->_memory_handle, 0x00, mem_size );

    bxBufferChunker chunker( state->_memory_handle, mem_size );
    state->joint_world = chunker.add< anim::Joint >( numJoints );
    state->scratch_joints = chunker.add< anim::Joint >( numJoints );
    state->matrix_world = chunker.add< Matrix4 >( numJoints );

    chunker.check();
}

void stateFree( State* state, bxAllocator* allocator )
{
    BX_FREE( allocator, state->_memory_handle );
    state[0] = {};
}

void computeClipTrajectory( ClipTrajectory* ct, const anim::Clip* clip, float trajectoryStartTime, float trajectoryDuration )
{
    const Vector4 plane = makePlane( Vector3::yAxis(), Vector3( 0.f ) );
    anim::Joint root_joint;
    anim::evaluateClip( &root_joint, clip, 0, 0.f, 0, 0 );
    const Vector3 root_displacement_xz = projectVectorOnPlane( root_joint.position, plane );
    const Vector3 root_displacement_y = utils::removeRootMotion( root_joint.position ); // mulPerElem( root_joint.position, Vector3( 0.f, 1.f, 0.f ) );

    anim::Joint trajectory_joint;
    const float duration = trajectoryDuration;
    const float dt = ( duration ) / (float)( eNUM_TRAJECTORY_POINTS - 1 );
    const float start_time = trajectoryStartTime;

    Vector3 extrapolation_dpos( 0.f );
    bool end_clip_reached = false;
    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        float trajectory_time = start_time + dt * i;
        if( trajectory_time <= ( clip->duration + FLT_EPSILON ) )
        {
            trajectory_time = minOfPair( clip->duration, trajectory_time );
            anim::evaluateClip( &trajectory_joint, clip, trajectory_time, 0, 0 );
            trajectory_joint.position -= root_displacement_xz;
            trajectory_joint.position -= root_displacement_y;
            ct->pos[i] = trajectory_joint.position;
        }
        else
        {
            trajectory_time = ::fmodf( trajectory_time, 1.f );

            anim::Joint last_joint, prev_last_joint;
            anim::evaluateClip( &last_joint, clip, clip->numFrames - 1, 0.f, 0, 0 );
            anim::evaluateClip( &prev_last_joint, clip, clip->numFrames - 2, 0.f, 0, 0 );
            anim::evaluateClip( &trajectory_joint, clip, trajectory_time, 0, 0 );

            Vector3 v = last_joint.position - prev_last_joint.position;

            if( i == 0 )
            {
                ct->pos[i] = last_joint.position;
            }
            else
            {
                const floatInVec displacement = length( trajectory_joint.position );

                ct->pos[i] = ct->pos[i - 1] + normalizeSafe( v ) * displacement;
            }
        }
    }
    utils::computeTrajectoryDerivatives( ct->vel, ct->acc, ct->pos, 1.f / dt );
}



//-------------------------------------------------------------------

void Context::load( const char* skelFile, const AnimClipInfo* animInfo, unsigned numAnims )
{
    if( !_allocator )
    {
        _allocator = bxDefaultAllocator();
    }

    ResourceManager* resource_manager = getResourceManager();

    _data.skel = anim_ext::loadSkelFromFile( resource_manager, skelFile );
    SYS_ASSERT( _data.skel != nullptr );

    _data.clips.reserve( numAnims );
    _data.clip_infos.reserve( numAnims );
    for( unsigned i = 0; i < numAnims; ++i )
    {
        anim::Clip* clip = anim_ext::loadAnimFromFile( resource_manager, animInfo[i].name.c_str() );
        if( !clip )
        {
            continue;
        }

        _data.clip_infos.push_back( animInfo[i] );
        _data.clips.push_back( clip );
    }
}

void Context::unload()
{
    ResourceManager* resource_manager = getResourceManager();
    while( !_data.clips.empty() )
    {
        anim::Clip* c = _data.clips.back();
        anim_ext::unloadAnimFromFile( resource_manager, &c );
        _data.clips.pop_back();
    }
        
    anim::Skel* skel = ( anim::Skel*)_data.skel;
    anim_ext::unloadSkelFromFile( resource_manager, &skel );
    _data.skel = nullptr;
}

void Context::prepare( const ContextPrepareInfo& info )
{
    const u32 num_match_joints_names = _eMATCH_JOINT_COUNT_;
    _data.match_joints_indices.resize( num_match_joints_names, -1 );

    for( u32 i = 0; i < num_match_joints_names; ++i )
    {
        _data.match_joints_indices[i] = anim::getJointByName( _data.skel, info.matching_joint_names[i] );
    }

    prepare_evaluateClips();
        
    stateAllocate( &_state, _data.skel->numJoints, _allocator );
    _anim_player.prepare( _data.skel, _allocator );

    curve::allocate( _state.input_trajectory_curve, eNUM_TRAJECTORY_POINTS );
    curve::allocate( _state.candidate_trajectory_curve, eNUM_TRAJECTORY_POINTS );

    const char* left_ik_joint_names[3] =
    {
        "LeftUpLeg",
        "LeftLeg",
        "LeftFoot",
    };

    const char* right_ik_joint_names[3] = 
    {
        "RightUpLeg",
        "RightLeg",
        "RightFoot",
    };

    anim::ikNodeInit( &_left_foot_ik, _data.skel, left_ik_joint_names );
    anim::ikNodeInit( &_right_foot_ik, _data.skel, right_ik_joint_names );
}

void Context::prepare_evaluateClips()
{
    const anim::Skel* skel = _data.skel;
    std::vector< float > velocity;

    //_data.clip_trajectiories.resize( _data.clips.size() );

    for( size_t i = 0; i < _data.clips.size(); ++i )
    {
        const anim::Clip* clip = _data.clips[i];
        u32 frame_no = 0;

        float velocity_sum = 0.f;
        u32 velocity_count = 0;

        const float frame_dt = 1.f / clip->sampleFrequency;
        const size_t first_pose = _data.poses.size();
        while( frame_no < clip->numFrames )
        {
            Pose pose;
            PosePrepareInfo pose_prepare_info = {};
            pose_prepare_info.clip = clip;
            pose_prepare_info.skel = skel;
            pose_prepare_info.frameNo = frame_no;
            pose_prepare_info.joint_indices = _data.match_joints_indices.data();
            pose_prepare_info.is_loop = _data.clip_infos[i].is_loop != 0;

            poseAllocate( &pose, _data.skel->numJoints, _allocator );
            posePrepare( &pose, pose_prepare_info );

            pose.params.clip_index = (u32)i;
            pose.params.clip_start_time = frame_no * frame_dt;
            _data.poses.push_back( pose );

            const float pose_speed = length( pose.local_velocity ).getAsFloat();
            velocity_sum += pose_speed;

            velocity.push_back( pose_speed );
            ++velocity_count;
            ++frame_no;
        }
        
        {
            const size_t end_pose = _data.poses.size();
            const size_t n_poses = end_pose - first_pose;
            const float tstep = 1.f / (float)( n_poses - 1 );
            float t = 0.f;

            AnimFootPlaceInfo fpi = {};
            curve::allocate( fpi.foot_curve, (u32)n_poses );

            for( size_t i = first_pose; i < end_pose; ++i )
            {
                const Pose& pose = _data.poses[i];
                const float left_value = (float)pose.flags[eMATCH_JOINT_LEFT_FOOT];
                const float right_value = -(float)pose.flags[eMATCH_JOINT_RIGHT_FOOT];
                const float value = left_value + right_value;
                
                curve::push_back( fpi.foot_curve, value, t );

                t += tstep;
            }
            _data.clip_foot_place_info.push_back( fpi );
        }

        //ClipTrajectory& clipTrajectory = _data.clip_trajectiories[i];
        //computeClipTrajectory( &clipTrajectory, clip, 0.f, clip->duration );
    }

        
    std::sort( velocity.begin(), velocity.end(), std::less<float>() );
    curve::allocate( _data.velocity_curve, (u32)velocity.size() );
    for( u32 i = 0; i < velocity.size(); ++i )
    {
        const float t = (float)i / float( velocity.size() - 1 );
        curve::push_back( _data.velocity_curve, velocity[i], t );
    }
}

void Context::unprepare()
{
    curve::deallocate( _data.velocity_curve );
    curve::deallocate( _state.candidate_trajectory_curve );
    curve::deallocate( _state.input_trajectory_curve );

    _anim_player.unprepare( _allocator );

    stateFree( &_state, _allocator );

    while( !_data.clip_foot_place_info.empty() )
    {
        AnimFootPlaceInfo& fpi = _data.clip_foot_place_info.back();
        curve::deallocate( fpi.foot_curve );
        //curve::deallocate( fpi.right_curve );
        _data.clip_foot_place_info.pop_back();
    }

    while( !_data.poses.empty() )
    {
        Pose& pose = _data.poses.back();
        poseFree( &pose, _allocator );
        _data.poses.pop_back();
    }
}

void Context::tick( const Input& input, float deltaTime )
{
    const float delta_time_inv = ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f;
    const float delta_time_sqr = deltaTime * deltaTime;
    const float desired_anim_speed = bx::curve::evaluate_catmullrom( _data.velocity_curve, input.speed01 );
    const float desired_anim_speed_inv = ( desired_anim_speed > FLT_EPSILON ) ? 1.f / desired_anim_speed : 0.f;


    const Matrix4 to_local_space = inverse( input.base_matrix_aligned );
    Vector3 local_trajectory[eNUM_TRAJECTORY_POINTS];
    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        local_trajectory[i] = mulAsVec4( to_local_space, input.trajectory[i] );
    }
    const Vector3 local_velocity = to_local_space.getUpper3x3() * input.velocity;
    const Vector3 local_acceleration = to_local_space.getUpper3x3() * input.acceleration;
    const Vector3 desired_anim_velocity = normalizeSafe( local_velocity ) * desired_anim_speed;
    const float acceleration_factor = length( local_acceleration ).getAsFloat();

    //utils::resampleTrajectory( local_trajectory, local_trajectory, _state.input_trajectory_curve );
    Vector3 local_trajectory_v[eNUM_TRAJECTORY_POINTS - 1];
    Vector3 local_trajectory_a[eNUM_TRAJECTORY_POINTS - 2];
    utils::computeTrajectoryDerivatives( local_trajectory_v, local_trajectory_a, local_trajectory, input.trajectory_integration_time / (float)( eNUM_TRAJECTORY_POINTS - 1) );
    
    {
        Vector3 tmp_trajectory_pos[eNUM_TRAJECTORY_POINTS];
        Vector3 tmp_trajectory_vel[eNUM_TRAJECTORY_POINTS - 1];
        Vector3 tmp_trajectory_acc[eNUM_TRAJECTORY_POINTS - 2];

        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        {
            tmp_trajectory_pos[i] = mulAsVec4( input.base_matrix_aligned, local_trajectory[i] + Vector3::yAxis()*0.1f );
            if( i < eNUM_TRAJECTORY_POINTS - 1 )
                tmp_trajectory_vel[i] = input.base_matrix_aligned.getUpper3x3() * local_trajectory_v[i];
        }

        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS - 1; ++i )
        {
            //bxGfxDebugDraw::addSphere( Vector4( mulAsVec4( input.base_matrix, pose_trajectory_pos[i] ), 0.03f ), 0xffffffff, 1 );
            bxGfxDebugDraw::addLine( tmp_trajectory_pos[i], tmp_trajectory_pos[i + 1], 0xFFFFFFFF, 1 );
            bxGfxDebugDraw::addLine( tmp_trajectory_pos[i], tmp_trajectory_pos[i] + tmp_trajectory_vel[i], 0xFF00FFFF, 1 );
            //bxGfxDebugDraw::addSphere( Vector4( mulAsVec4( input.base_matrix, pose_trajectory_pos[i] ), 0.03f ), 0xffffffff, 1 );
        }
    }

    //const Matrix3 desired_future_rotation = utils::computeBaseMatrix3( normalizeSafe( local_trajectory_v[2] ), local_trajectory_a[1] * deltaTime*deltaTime, Vector3::yAxis() );
    //bxGfxDebugDraw::addAxes( input.base_matrix_aligned * Matrix4( desired_future_rotation, local_trajectory[3] ) );
    const u32 num_joints = _data.skel->numJoints;

    anim::Joint* curr_local_joints = nullptr;

    Vector3 prev_matching_pos[_eMATCH_JOINT_COUNT_];
    Vector3 curr_matching_pos[_eMATCH_JOINT_COUNT_];
    Vector3 curr_matching_vel[_eMATCH_JOINT_COUNT_];

    memset( prev_matching_pos, 0x00, sizeof( prev_matching_pos ) );
    memset( curr_matching_pos, 0x00, sizeof( curr_matching_pos ) );
    memset( curr_matching_vel, 0x00, sizeof( curr_matching_vel ) );
    
    //if( _state.num_clips )
    if( !_anim_player.empty() )
    {
        anim::Joint* scratch = _state.scratch_joints;
        anim::Joint* prev_local_joints = _anim_player.prevLocalJoints();
        curr_local_joints = _anim_player.localJoints();

        prev_local_joints[0].position = utils::removeRootMotion( prev_local_joints[0].position );
        curr_local_joints[0].position = utils::removeRootMotion( curr_local_joints[0].position );

        const Vector4 yplane = makePlane( Vector3::yAxis(), Vector3( 0.f ) );
        anim::Joint root_joint = anim::Joint::identity();
        
        //
        anim_ext::localJointsToWorldJoints( scratch, prev_local_joints, _data.skel, root_joint );
        for( u32 i = 0; i < _data.match_joints_indices.size(); ++i )
        {
            i16 index = _data.match_joints_indices[i];
            prev_matching_pos[i] = scratch[index].position;
        }

        //
        anim_ext::localJointsToWorldJoints( scratch, curr_local_joints, _data.skel, root_joint );
        for( u32 i = 0; i < _data.match_joints_indices.size(); ++i )
        {
            i16 index = _data.match_joints_indices[i];
            const Vector3 pos = scratch[index].position; // -root_displacement_xz;
            curr_matching_pos[i] = pos;
            curr_matching_vel[i] = ( pos - prev_matching_pos[i] ) * delta_time_inv;

            //bxGfxDebugDraw::addSphere( Vector4( curr_matching_pos[i], 0.01f ), 0xFF0000FF, 1 );
            bxGfxDebugDraw::addLine( pos, pos + curr_matching_vel[i], 0x0000FFFF, 1 );
        }
    }

        
    _debug.pose_indices.clear();
    bool pose_changed = false;
    if( curr_local_joints )
    {
        //bxAnim_Joint* tmp_joints = _state.joint_world;
        float change_cost = FLT_MAX;
        u32 change_index = UINT32_MAX;

        for( size_t i = 0; i < _data.poses.size(); ++i )
        {
            const Pose& pose = _data.poses[i];
            if( i == _state.pose_index )
                continue;

            const AnimClipInfo& clip_info = _data.clip_infos[pose.params.clip_index];
            const anim::Clip* clip = _data.clips[pose.params.clip_index];
            const bool is_pose_clip_looped = clip_info.is_loop == 1;

            const ClipTrajectory& pose_trajectory = pose.trajectory; // _data.clip_trajectiories[pose.params.clip_index];

            const float pose_position_cost = utils::computePositionCost( pose.pos, curr_matching_pos, _eMATCH_JOINT_COUNT_ );
            const float pose_velocity_cost = utils::computeVelocityCost( pose.vel, curr_matching_vel, _eMATCH_JOINT_COUNT_, deltaTime );
            const float velocity_cost = utils::computeVelocityCost( desired_anim_velocity, pose.local_velocity );
            const float trajectory_position_cost = utils::computeTrajectoryCost( local_trajectory, pose_trajectory.pos );
            const float trajectory_velocity_cost = utils::computeVelocityCost( local_trajectory_v, pose_trajectory.vel, eNUM_TRAJECTORY_POINTS - 1, deltaTime );
            const float trajectory_acceleration_cost = utils::computeVelocityCost( local_trajectory_a, pose_trajectory.acc, eNUM_TRAJECTORY_POINTS - 2, delta_time_sqr );

            float cost = 0.f;
            cost += pose_position_cost;
            cost += pose_velocity_cost;// *minOfPair( 1.f, acceleration_factor );
            
            cost += velocity_cost;
            cost += trajectory_position_cost;
            cost += trajectory_velocity_cost;
            cost += trajectory_acceleration_cost;
            checkFloat( cost );

            if( change_cost > cost )
            {
                change_cost = cost;
                change_index = (u32)i;
                _debug.pose_indices.push_back( change_index );
            }
        }

        const Pose& winner_pose = _data.poses[change_index];

        u64 current_clip_index = UINT64_MAX;
        bool valid = _anim_player.userData( &current_clip_index, 0 );
        SYS_ASSERT( valid );
        
        float curret_clip_time = 0.f;
        valid = _anim_player.evalTime( &curret_clip_time, 0 );
        SYS_ASSERT( valid );

        const bool winner_is_at_the_same_location = ( (u32)current_clip_index == winner_pose.params.clip_index ) && ::abs( curret_clip_time - winner_pose.params.clip_start_time ) < 0.2f;

        _state.pose_index = change_index;

        if( !winner_is_at_the_same_location )
        {
            anim::Clip* clip = _data.clips[winner_pose.params.clip_index];
            _anim_player.play( clip, winner_pose.params.clip_start_time, 0.25f, winner_pose.params.clip_index );
            _state.pose_index = change_index;
            pose_changed = true;
        }

        //{
        //    const ClipTrajectory& pose_trajectory = winner_pose.trajectory; // _data.clip_trajectiories[winner_pose.params.clip_index];
        //    Vector3 pose_trajectory_pos[eNUM_TRAJECTORY_POINTS];
        //    Vector3 pose_trajectory_vel[eNUM_TRAJECTORY_POINTS - 1];
        //    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        //    {
        //        pose_trajectory_pos[i] = mulAsVec4( input.base_matrix_aligned, pose_trajectory.pos[i] + Vector3::yAxis()*0.1f );
        //        if( i < eNUM_TRAJECTORY_POINTS-1 )
        //            pose_trajectory_vel[i] = input.base_matrix_aligned.getUpper3x3() * pose_trajectory.vel[i];
        //    }

        //    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS-1; ++i )
        //    {
        //        bxGfxDebugDraw::addLine( pose_trajectory_pos[i], pose_trajectory_pos[i + 1], 0xFF0000FF, 1 );
        //        //bxGfxDebugDraw::addLine( pose_trajectory_pos[i], pose_trajectory_pos[i] + pose_trajectory_vel[i], 0xFFFF00FF, 1 );
        //    }
        //}

        
    }
    else if( _anim_player.empty() )
    {
        const u32 change_index = 0;
        const Pose& winner_pose = _data.poses[change_index];
        anim::Clip* clip = _data.clips[winner_pose.params.clip_index];
        _anim_player.play( clip, winner_pose.params.clip_start_time, 0.25f, winner_pose.params.clip_index );
        //_anim_player.tick( deltaTime );
        _state.pose_index = change_index;
        pose_changed = true;
    }

    const float anim_delta_time = deltaTime; // *_state.anim_delta_time_scaler;
    _anim_player.tick( anim_delta_time );
    curr_local_joints = _anim_player.localJoints();
    curr_local_joints[0].position = utils::removeRootMotion( curr_local_joints[0].position );

    const Matrix4 root_matrix = /*Matrix4::translation( pivot ) * */input.base_matrix_aligned;
    const anim::Joint root = anim::toAnimJoint_noScale( root_matrix );
    const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _data.skel->offsetParentIndices );
    if( curr_local_joints )
    {
        anim_ext::localJointsToWorldMatrices( _state.matrix_world, curr_local_joints, _data.skel, root );
        anim_ext::localJointsToWorldJoints( _state.joint_world, curr_local_joints, _data.skel, root );
        //utils::debugDrawMatrices( _state.matrix_world, parent_indices, num_joints, 0x666666FF, 0.015f );
    }

    u64 current_clip_index = 0;
    Vector3 pivot( 0.f );
    if( _anim_player.userData( &current_clip_index, 0 ) )
    {
        //float foot_raw = 0.f;
        //if( _anim_player._num_clips == 1 )
//         {
//             const AnimFootPlaceInfo& foot_info = _data.clip_foot_place_info[current_clip_index];
//             float eval_time = 0.f;
//             _anim_player.evalTime( &eval_time, 0 );
//             const float eval_phase = eval_time / _data.clips[current_clip_index]->duration;
//             foot_raw = curve::evaluate_catmullrom( foot_info.foot_curve, eval_phase );
//         }
        //else if( _anim_player._num_clips == 2 )
        //{
        //    float foot_value[2] = { 0.f, 0.f };
        //    for( u32 i = 0; i < 2; ++i )
        //    {
        //        u64 clip_index = 0;
        //        f32 eval_time = 0.f;
        //        _anim_player.userData( &clip_index, i );
        //        _anim_player.evalTime( &eval_time, i );

        //        const float eval_phase = eval_time / _data.clips[clip_index]->duration;

        //        const AnimFootPlaceInfo& foot_info = _data.clip_foot_place_info[clip_index];
        //        foot_value[i] = curve::evaluate_catmullrom( foot_info.foot_curve, eval_phase );
        //    }

        //    float blend_alpha;
        //    _anim_player.blendAlpha( &blend_alpha );
        //    foot_raw = lerp( blend_alpha, foot_value[0], foot_value[1] );
        //}

        //{
        //    const float turn = ::abs( dot( normalizeSafe( local_acceleration * delta_time_sqr ), Vector3::xAxis() ).getAsFloat() );
        //    const float foot = curve::evaluate_catmullrom( foot_info.foot_curve, eval_phase );
        //    const float converge = 0.002f;
        //    const float t = 1.f - ::pow( converge, deltaTime );
        //    _state.foot_value = lerp( t, _state.foot_value, foot );
        //    _state.turn_value = lerp( t, _state.turn_value, turn );
        //    foot_raw = foot;
        //}
        
        

        //foot_raw = -(float)pose.flags[eMATCH_JOINT_RIGHT_FOOT];
        
        //const Vector3 rfoot_pos = _state.joint_world[_data.match_joints_indices[eMATCH_JOINT_RIGHT_FOOT]].position;
        //
        //if( !_state.right_foot_lock && foot_raw < -0.9f && _state.right_foot_blend_alpha <= 0.f )
        //{
        //    _state.right_foot_lock = 1;
        //    _state.right_foot_pos = rfoot_pos;
        //    _state.right_foot_blend_alpha = 1.f;
        //}
        //else if( _state.right_foot_lock && foot_raw > -0.5f )
        //{
        //    _state.right_foot_lock = 0;
        //    
        //}
        //
        //Vector3 right_foot_pos = lerp( _state.right_foot_blend_alpha, rfoot_pos, _state.right_foot_pos );
        //
        //if( !_state.right_foot_lock )
        //{
        //    const float delta = deltaTime * 5.f; // 0.2s
        //    _state.right_foot_blend_alpha = maxOfPair( _state.right_foot_blend_alpha - delta, 0.f );
        //}
        //bxGfxDebugDraw::addSphere( Vector4( right_foot_pos, 0.1f ), 0xFFFF00FF, 1 );
        //if( ImGui::Begin( "FootDebug" ) )
        //{
        //    ImGui::Text( "foot_raw %f", foot_raw );
        //    ImGui::Text( "blend alpha %f", _state.right_foot_blend_alpha );
        //}
        //ImGui::End();

        //Vector3 lfoot_pos = scratch_joints[_data.match_joints_indices[eMATCH_JOINT_LEFT_FOOT]].position;
        //

        //float fa = _state.foot_value;
        //fa = fa*fa*fa;
        ////fa = fa*fa*fa;
        //float foot01 = ( fa ) * 0.5f + 0.5f;

        //const Vector4 up_plane = makePlane( input.base_matrix_aligned.getCol1().getXYZ(), input.base_matrix_aligned.getTranslation() );
        //const Vector4 front_plane = makePlane( input.base_matrix_aligned.getCol2().getXYZ(), input.base_matrix_aligned.getTranslation() );
        //pivot = lerp( foot01, rfoot_pos, lfoot_pos );
        //pivot = projectVectorOnPlane( pivot, up_plane );
        //pivot = projectVectorOnPlane( pivot, front_plane );

        //float sa = _state.turn_value;
        //sa = sa*sa*sa;
        //pivot *= sa;

        //bxGfxDebugDraw::addSphere( Vector4( mulAsVec4( input.base_matrix_aligned, pivot ), 0.1f ), 0xFFFF00FF, 1 );


        //pivot = mulAsVec4( to_local_space, pivot );
    }

    if( curr_local_joints )
    {
        const Vector3 rfoot_pos = _state.joint_world[_data.match_joints_indices[eMATCH_JOINT_RIGHT_FOOT]].position;
        const Vector3 lfoot_pos = _state.joint_world[_data.match_joints_indices[eMATCH_JOINT_LEFT_FOOT]].position;

        if( pose_changed )
        {
            _state.lfoot_lock.unlock( 0.1f );
            _state.rfoot_lock.unlock( 0.1f );
        }
        else
        {
            const Pose& pose = _data.poses[_state.pose_index];
            const float rfoot_value = (float)pose.flags[eMATCH_JOINT_RIGHT_FOOT];
            const float lfoot_value = (float)pose.flags[eMATCH_JOINT_LEFT_FOOT];
            _state.lfoot_lock.tryLock( lfoot_value, lfoot_pos );
            _state.rfoot_lock.tryLock( rfoot_value, rfoot_pos );
        }
        

        const Vector3 lfoot_pos_final = lerp( _state.lfoot_lock.unlockAlpha(), lfoot_pos, _state.lfoot_lock._pos );
        const Vector3 rfoot_pos_final = lerp( _state.rfoot_lock.unlockAlpha(), rfoot_pos, _state.rfoot_lock._pos );
        //bxGfxDebugDraw::addSphere( Vector4( lfoot_pos_final, 0.05f ), 0xFFFF0FFF, 1 );
        //bxGfxDebugDraw::addSphere( Vector4( rfoot_pos_final, 0.05f ), 0xFFFF0FFF, 1 );

        _state.lfoot_lock.tryBlendout( deltaTime );
        _state.rfoot_lock.tryBlendout( deltaTime );

        const i16* parent_indices = TYPE_OFFSET_GET_POINTER( i16, _data.skel->offsetParentIndices );
        anim::ikNodeSolve( curr_local_joints, _left_foot_ik, _state.matrix_world, parent_indices, _state.lfoot_lock._pos, _state.lfoot_lock.unlockAlpha(), false );
        anim::ikNodeSolve( curr_local_joints, _right_foot_ik, _state.matrix_world, parent_indices, _state.rfoot_lock._pos, _state.rfoot_lock.unlockAlpha(), false );
    }


    if( curr_local_joints )
    {
        
        anim_ext::localJointsToWorldJoints( _state.joint_world, curr_local_joints, _data.skel, root );
        const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _data.skel->offsetParentIndices );
        utils::debugDrawJoints( _state.joint_world, parent_indices, num_joints, 0xffffffFF, 0.025f, 1.f );

        {
            //const bxAnim_Joint* bind_pose = TYPE_OFFSET_GET_POINTER( bxAnim_Joint, _data.skel->offsetBasePose );
            //bxAnimExt::localJointsToWorldJoints( _state.scratch_joints, bind_pose, _data.skel, bxAnim_Joint::identity() );
            //utils::debugDrawJoints( bind_pose, parent_indices, num_joints, 0xffffffFF, 0.025f, 1.f );
        }

    }
}

void FootLock::tryLock( float foot_value, const Vector3 anim_world_pos )
{
    if( !_lock_flag && foot_value > 0.9f && _unlock_alpha <= 0.f )
    {
        _lock_flag = 1;
        _pos = anim_world_pos;
        _unlock_alpha = 1.f;
    }
    else if( _lock_flag && foot_value < 0.5f )
    {
        _lock_flag = 0;
    }
}

void FootLock::tryBlendout( float deltaTime )
{
    if( !_lock_flag )
    {
        const float delta = deltaTime * 5.f; // 0.2s
        _unlock_alpha = maxOfPair( _unlock_alpha - delta, 0.f );
    }
}

void FootLock::unlock( float duration )
{
    _unlock_duration = duration;
    _lock_flag = 0;
}

bool Context::currentSpeed( f32* value )
{
    if( _state.pose_index == UINT32_MAX )
    {
        return false;
    }

    const Pose& pose = _data.poses[_state.pose_index];
    value[0] = length( pose.vel[0] ).getAsFloat();

    return true;
}

bool Context::currentPose( Matrix4* pose )
{
    if( _anim_player.empty() )
        return false;

    pose[0] = Matrix4( _state.joint_world[0].rotation, _state.joint_world[0].position );
    return true;
}

void Context::_DebugDrawPose( u32 poseIndex, u32 color, const Matrix4& base )
{
    if( poseIndex >= _data.poses.size() )
        return;

    const Pose& pose = _data.poses[poseIndex];

    const anim::Joint root_joint = anim::toAnimJoint_noScale( base );
    

    const ClipTrajectory& pose_trajectory = pose.trajectory; // _data.clip_trajectiories[pose.params.clip_index];

    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        bxGfxDebugDraw::addSphere( Vector4( pose_trajectory.pos[i], 0.03f ), 0x666666ff, 1 );
    }

    {
        bxGfxDebugDraw::addLine( root_joint.position, root_joint.position + pose.local_velocity, 0xFFFFFFFF, 1 );
    }

    //const bxAnim_Joint& j = _debug.joints[0];
    for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
    {
        const Vector3 p = fastTransform( root_joint.rotation, root_joint.position, pose.pos[i] ); // j.position;
        const Vector3 v = fastTransform( root_joint.rotation, root_joint.position, pose.vel[i] );
        //const Vector3 a = fastTransform( j.rotation, j.position, pose.params.acceleration );
        bxGfxDebugDraw::addSphere( Vector4( p, 0.01f ), 0x0000FFFF, 1 );
        bxGfxDebugDraw::addLine( p, v, 0x0000FFFF, 1 );
        //bxGfxDebugDraw::addLine( p, a, 0xFFFF00FF, 1 );
    }



    _debug.joints0.resize( _data.skel->numJoints );
    _debug.joints1.resize( _data.skel->numJoints );
    
    const anim::Clip* clip = _data.clips[pose.params.clip_index];
    const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _data.skel->offsetParentIndices );
    anim::evaluateClip( _debug.joints0.data(), clip, pose.params.clip_start_time );
    _debug.joints0[0].position = utils::removeRootMotion( _debug.joints0[0].position ); // mulPerElem( _debug.joints0[0].position, Vector3( 0.f, 1.f, 0.f ) );
    anim_ext::localJointsToWorldJoints( _debug.joints1.data(), _debug.joints0.data(), _data.skel, root_joint );
    utils::debugDrawJoints( _debug.joints1.data(), parent_indices, _data.skel->numJoints, 0x333333ff, 0.01f, 1.f );

}


}}///




namespace bx{
namespace motion_matching{

void DynamicState::tick( const bxInput& sysInput, const Input& input, float deltaTime )
{
    characterInputCollectData( &_input, sysInput, deltaTime, 0.01f );

    const Vector4 yplane = makePlane( Vector3::yAxis(), _position );
    const Vector3 anim_dir = normalizeSafe( projectVectorOnPlane( input.anim_pose.getCol2().getXYZ(), yplane ) );

    const Vector3 x_input_force = Vector3::xAxis() * _input.analogX;
    const Vector3 z_input_force = -Vector3::zAxis() * _input.analogY;
    //const Vector3 y_input_force = Vector3::yAxis() * ( ( _input.jump > 0.1f ) ? _input.jump*100.f : -9.8f );
    Vector3 input_force = x_input_force + z_input_force;// +y_input_force;

    const float converge_time01 = 0.0001f;
    const float lerp_alpha = 1.f - ::pow( converge_time01, deltaTime );

    const float max_input_speed = lerp( _input.R2, 0.6f, 1.f );

    const float input_speed = minOfPair( max_input_speed, length( input_force ).getAsFloat() );
    input_force = normalizeSafe( input_force ) * input_speed;
    _prev_speed01 = _speed01;
    _speed01 = lerp( lerp_alpha, _speed01, input_speed );

    _prev_direction = _direction;

    const float input_speed_sqr = input_speed * input_speed;
    if( input_speed_sqr > FLT_EPSILON )
    {
        const Vector3 input_dir = normalize( input_force );
        _direction = slerp( lerp_alpha, _direction, input_force );
        _direction = normalizeSafe( _direction );
    }

    const float delta_time_inv = ( deltaTime > 0.f ) ? 1.f / deltaTime : 0.f;
    const float spd = ( _speed01 < 0.1f ) ? 0 : _max_speed; // *_speed01;

    //_acceleration = ( _direction - _prev_direction ) * spd  * delta_time_inv;
    Vector3 prev_vel = _velocity;
    _velocity = _direction * spd;
    _acceleration = ( _velocity - prev_vel ) * delta_time_inv;
    _position += _velocity * deltaTime;

    //////////////////////////////////////////////////////////////////////////
    //mothahack
    if( _position.getY().getAsFloat() < 0.f )
    {
        _position.setY( 0.f );
    }
    //////////////////////////////////////////////////////////////////////////

    const float dt = _trajectory_integration_time / ( eNUM_TRAJECTORY_POINTS - 1 );
    float time = dt;
    _trajectory_pos[0] = _position;
    Vector3 trajectory_velocity = _velocity;
    Vector3 trajectory_acceleration = _acceleration;
    for( u32 i = 1; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        //trajectory_velocity += trajectory_acceleration * dt;
        _trajectory_pos[i] = _trajectory_pos[0] + trajectory_velocity * time + trajectory_acceleration*time*time*0.5f;

        time += dt;
    }
    if( input_speed_sqr <= FLT_EPSILON )
    {
        const Vector4 dir_plane = makePlane( _direction, _position );
        for( u32 i = 1; i < eNUM_TRAJECTORY_POINTS; ++i )
        {
            const floatInVec d = dot( dir_plane, Vector4( _trajectory_pos[i], oneVec ) );
            _trajectory_pos[i] = select( _trajectory_pos[i], projectPointOnPlane( _trajectory_pos[i], dir_plane ), d < zeroVec );
        }
    }

    //for( u32 i = 1; i < eNUM_TRAJECTORY_POINTS-1; ++i )
    //{
    //    bxGfxDebugDraw::addLine( _trajectory[i - 1], _trajectory[i], 0xFFFFFFFF, 1 );
    //}

    _last_delta_time = deltaTime;
    _prev_input_force = input_force;
}

Matrix4 DynamicState::computeBaseMatrix( bool includeTilt ) const
{
    const Vector3 acc = ( includeTilt ) ? _acceleration * _last_delta_time : Vector3( 0.f );
    Matrix3 rotation = utils::computeBaseMatrix3( _direction, acc, Vector3::yAxis() );
    return Matrix4( rotation, _position );
}

void DynamicState::debugDraw( u32 color )
{
    bxGfxDebugDraw::addLine( _position, _position + _direction, 0x0000FFFF, 1 );
    bxGfxDebugDraw::addLine( _position, _position + _direction * _speed01, color, 1 );

    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        bxGfxDebugDraw::addSphere( Vector4( _trajectory_pos[i], 0.03f ), 0x666666ff, 1 );
    }

    const Matrix4 base = computeBaseMatrix();
    const Matrix4 base_with_tilt = computeBaseMatrix( true );
    //bxGfxDebugDraw::addAxes( base );
    bxGfxDebugDraw::addAxes( base_with_tilt );
}

void motionMatchingCollectInput( Input* input, const DynamicState& dstate )
{
    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        input->trajectory[i] = dstate._trajectory_pos[i];

    input->velocity = dstate._velocity;
    input->acceleration = dstate._acceleration;
    //input->base_matrix = dstate.computeBaseMatrix( true );
    input->base_matrix_aligned = dstate.computeBaseMatrix( false );
    input->speed01 = dstate._speed01;
    //input->raw_input = dstate._prev_input_force;
    input->trajectory_integration_time = dstate._trajectory_integration_time;
}

}}////


namespace bx{
namespace anim{

inline boolInVec computeAxisAndAngle( Vector3* outAxis, floatInVec* outAngle, const Vector3& vA, const Vector3& vB, const floatInVec& kSqrLengthThreshold )
{
    /// axis between vA and vB
    Vector3 axis = cross( vA, vB );

    floatInVec axisSqlen = lengthSqr( axis );
    boolInVec axisValid = axisSqlen >= kSqrLengthThreshold;

    axisSqlen = sqrtf4( axisSqlen.get128() );
    axis /= axisSqlen;

    axisSqlen *= oneVec / ( lengthSqr( vA ) * lengthSqr( vB ) );
    axisSqlen = minf4( axisSqlen, oneVec );

    floatInVec angle( asinf( axisSqlen.getAsFloat() ) );

    // check for extreme angles
    angle = select( angle, piVec - angle, dot( vA, vB ) < zeroVec );

    outAxis[0] = axis;
    outAngle[0] = angle;

    return axisValid;
}
inline Quat ikComputeDeltaRot( const Vector3& p, const Vector3& e, const Vector3& g, float strength, const float* angleLimit, const Vector3* hingeAxis = nullptr )
{
    const floatInVec kSqrLengthThreshold = floatInVec( 1e-10f );

    Vector3 u = ( e - p );
    Vector3 v = ( g - p );
    if( hingeAxis )
    {
        const Vector4 plane = makePlane( *hingeAxis, Vector3( 0.f ) );
        u = projectVectorOnPlane( u, plane );
        v = projectVectorOnPlane( v, plane );
    }
    u = normalizeSafe( u );
    v = normalizeSafe( v );

    floatInVec angleInVec;
    Vector3 axis;
    const boolInVec valid = computeAxisAndAngle( &axis, &angleInVec, u, v, kSqrLengthThreshold );

    float angle = angleInVec.getAsFloat();
    if( angleLimit )
    {
        angle = clamp( angle, angleLimit[0], angleLimit[1] );
    }
    angle *= strength;
    //axis = ( hingeAxis ) ? *hingeAxis : axis;
    Quat drot = select( Quat::identity(), Quat::rotation( angle, axis ), valid );
    return drot;
}

void ikNodeInit( IKNode3* node, const Skel* skel, const char* jointNames[3] )
{
    node->idx_begin = getJointByName( skel, jointNames[0] );
    node->idx_middle = getJointByName( skel, jointNames[1] );
    node->idx_end = getJointByName( skel, jointNames[2] );
    SYS_ASSERT( node->idx_begin != -1 );
    SYS_ASSERT( node->idx_middle != -1 );
    SYS_ASSERT( node->idx_end != -1 );
    
    const Joint* bind_pose = TYPE_OFFSET_GET_POINTER( Joint, skel->offsetBasePose );

    const Vector3& begin_pos = bind_pose[node->idx_begin].position;
    const Vector3& mid_pos = bind_pose[node->idx_middle].position;
    const Vector3& end_pos = bind_pose[node->idx_end].position;
    
    node->len_begin_2_mid = length( mid_pos - begin_pos ).getAsFloat();
    node->len_mid_2_end = length( end_pos - mid_pos ).getAsFloat();
    node->chain_length = node->len_begin_2_mid + node->len_mid_2_end;
}

void ikNodeSolve( Joint* localJoints, const IKNode3& node, const Matrix4* animMatrices, const i16* parentIndices, const Vector3& goalPosition, float strength, bool debugDraw )
{
    const Matrix4& beginPose = animMatrices[node.idx_begin];
    const Matrix4& midPose = animMatrices[node.idx_middle];
    const Matrix4& endPose = animMatrices[node.idx_end];

    const Vector3 world_axis = beginPose.getUpper3x3() * node.local_axis;
    const Vector4 plane = makePlane( world_axis, beginPose.getTranslation() );

    const Vector3 beginPos = beginPose.getTranslation();
    const floatInVec midToBeginLen = floatInVec( node.len_begin_2_mid );// length( midPos - beginPos );
    const floatInVec endToMidLen = floatInVec( node.len_mid_2_end ); // length( midPos - endPos );

    Vector3 midPos = midPose.getTranslation();
    Vector3 endPos = endPose.getTranslation();
    const Vector3 goal = projectPointOnPlane( goalPosition, plane );
    for( int i = 0; i < node.num_iterations; ++i )
    {
        {
            //float limit[2] = { 0.f, PI /2 };
            //const Vector3 hingeAxis = midPose.getCol1().getXYZ();
            Quat drot = ikComputeDeltaRot( midPos, endPos, goal, 1.f, nullptr, nullptr );
            endPos = midPos + fastRotate( drot, endPos - midPos );
        }
        {
            //float limit[2] = { -PI / 2.f, PI / 4.f };
            Quat drot = ikComputeDeltaRot( beginPos, endPos, goal, 1.f, nullptr, nullptr );

            midPos = beginPos + normalize( fastRotate( drot, midPos - beginPos ) ) * midToBeginLen;
            endPos = midPos + normalize( endPos - midPos ) * endToMidLen;
        }

        if( lengthSqr( endPos - goal ).getAsFloat() < 0.001f )
                break;
    }

    {
        const Joint& joint = localJoints[node.idx_middle];
        const Matrix4 beginPoseInv = inverse( beginPose );
        const Vector3 posLocal0 = joint.position;
        const Vector3 posLocal1 = mulAsVec4( beginPoseInv, midPos );
        const Vector3 axis = -Vector3::xAxis();
        const Quat drot = ikComputeDeltaRot( Vector3( 0.f ), posLocal0, posLocal1, strength, nullptr, nullptr );
        localJoints[node.idx_begin].rotation *= drot;
    }

    /// recompute new midPose
    Matrix4 newMidPose;
    {
        Matrix4 parent = Matrix4::identity();
        if( parentIndices[node.idx_begin] != -1 )
            parent = animMatrices[parentIndices[node.idx_begin]];
        for( int i = node.idx_begin; i < node.idx_middle; ++i )
        {
            const Joint& localJoint = localJoints[i];
            parent *= appendScale( Matrix4( localJoint.rotation, localJoint.position ), localJoint.scale );
        }

        const Joint& midJoint = localJoints[node.idx_middle];
        newMidPose = parent * appendScale( Matrix4( midJoint.rotation, midJoint.position ), midJoint.scale );
    }

    {
        const Joint& joint = localJoints[node.idx_end];
        const Matrix4 newMidPoseInv = inverse( newMidPose );
        const Vector3 p0 = joint.position;
        const Vector3 p1 = mulAsVec4( newMidPoseInv, endPos );
        Quat drot = ikComputeDeltaRot( Vector3( 0.f ), p0, p1, strength, nullptr );
        localJoints[node.idx_middle].rotation *= drot;
    }

    if( debugDraw )
    {
        const float dbgSphRadius = 0.025f;
        bxGfxDebugDraw::addSphere( Vector4( beginPos, dbgSphRadius ), 0xFF00FF00, true );
        bxGfxDebugDraw::addSphere( Vector4( midPos, dbgSphRadius ), 0xFF00FF00, true );
        bxGfxDebugDraw::addSphere( Vector4( endPos, dbgSphRadius ), 0xFF00FF00, true );
        bxGfxDebugDraw::addSphere( Vector4( newMidPose.getTranslation(), dbgSphRadius * 2.6f ), 0x0000FFFF, true );
        
        bxGfxDebugDraw::addLine( beginPos, midPos, 0xFF00FF00, true );
        bxGfxDebugDraw::addLine( midPos, endPos, 0xFF00FF00, true );
        
        
        bxGfxDebugDraw::addSphere( Vector4( goal, dbgSphRadius ), 0xFF0000FF, true );
    }
}

}}///
