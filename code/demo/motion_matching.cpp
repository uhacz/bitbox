#include "motion_matching.h"
#include <anim/anim.h>
#include <resource_manager/resource_manager.h>
#include <algorithm>
#include <xfunctional>
#include <gfx/gfx_debug_draw.h>

#include <util/common.h>
#include <util/buffer_utils.h>

namespace bx{
namespace motion_matching{

namespace utils
{
    void debugDrawJoints( const bxAnim_Joint* joints, const u16* parentIndices, u32 numJoints, u32 color, float radius, float scale )
    {
        for( u32 i = 0; i < numJoints; ++i )
        {
            Vector4 sph( joints[i].position * scale, radius );
            bxGfxDebugDraw::addSphere( sph, color, 1 );

            const u16 parent_idx = parentIndices[i];
            if( parent_idx != 0xFFFF )
            {
                const Vector3 parent_pos = joints[parent_idx].position;
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
            result += lengthSqr( pos0[i] - pos1[i] );
        }

        return ::sqrt( result.getAsFloat() );
    }
    float computeDirectionCost( const Vector3* dir0, const Vector3* dir1, u32 n )
    {
        floatInVec result( 0.f );
        for( u32 i = 0; i < n; ++i )
        {
            result += oneVec - dot( normalizeSafe( dir0[i] ), normalizeSafe( dir1[i] ) );
        }

        return result.getAsFloat();
    }

    float computeVelocityCost( const Vector3* vel0, const Vector3* vel1, u32 numPos )
    {
        floatInVec result( 0 );
        for( u32 i = 0; i < numPos; ++i )
        {
            result += lengthSqr( vel0[i] - vel1[i] );
        }

        return ::sqrt( result.getAsFloat() );
    }
    inline float computeVelocityCost( const Vector3 vA, const Vector3 vB )
    {
        floatInVec cost = length( vA - vB );
        //cost += oneVec - dot( normalizeSafe( vA ), normalizeSafe( vB ) );
        return cost.getAsFloat();
    }
}///

State::State()
{
    memset( clip_index, 0xff, sizeof( clip_index ) );
    memset( clip_eval_time, 0x00, sizeof( clip_eval_time ) );
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
    const bxAnim_Clip* clip = info.clip;
    const bxAnim_Skel* skel = info.skel;
    const u32 frameNo = info.frameNo;
    const Vector4 plane = makePlane( Vector3::yAxis(), Vector3( 0.f ) );

    std::vector< bxAnim_Joint > frame_joints_0;
    std::vector< bxAnim_Joint > frame_joints_1;
    std::vector< bxAnim_Joint > tmp_joints;
    frame_joints_0.resize( skel->numJoints );
    frame_joints_1.resize( skel->numJoints );
    tmp_joints.resize( skel->numJoints );

    bxAnim_Joint j0[_eMATCH_JOINT_COUNT_], j1[_eMATCH_JOINT_COUNT_];
    const u32 lastFrame = info.clip->numFrames - 1;
    if( frameNo == lastFrame )
    {
        bxAnim::evaluateClip( frame_joints_0.data(), clip, frameNo - 1, 0.f );
        bxAnim::evaluateClip( frame_joints_1.data(), clip, frameNo, 0.f );
    }
    else
    {
        bxAnim::evaluateClip( frame_joints_0.data(), clip, frameNo, 0.f );
        bxAnim::evaluateClip( frame_joints_1.data(), clip, frameNo + 1, 0.f );
    }

    {
        bxAnim_Joint root_joint = bxAnim_Joint::identity();

        bxAnimExt::localJointsToWorldJoints( tmp_joints.data(), frame_joints_0.data(), skel, root_joint );
        for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
            j0[i] = tmp_joints[info.joint_indices[i]];

        bxAnimExt::localJointsToWorldJoints( tmp_joints.data(), frame_joints_1.data(), skel, root_joint );
        for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
            j1[i] = tmp_joints[info.joint_indices[i]];
    }

    bxAnim_Joint current_joint;
    bxAnim::evaluateClip( &current_joint, clip, frameNo, 0.f, 0, 0 );
    const Vector3 current_displacement_xz = projectVectorOnPlane( current_joint.position, plane );
    for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
    {
        const Vector3 v0 = j0[i].position - current_displacement_xz;
        const Vector3 v1 = j1[i].position - current_displacement_xz;

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
        
        
}
//-------------------------------------------------------------------
void stateAllocate( State* state, u32 numJoints, bxAllocator* allocator )
{
    u32 mem_size = 0;
    mem_size = numJoints * ( /*sizeof( *state->joint_local ) +*/ sizeof( *state->joint_world ) );

    state->_memory_handle = BX_MALLOC( allocator, mem_size, 16 );
    memset( state->_memory_handle, 0x00, mem_size );

    bxBufferChunker chunker( state->_memory_handle, mem_size );
    //state->joint_local = chunker.add< bxAnim_Joint >( numJoints );
    state->joint_world = chunker.add< bxAnim_Joint >( numJoints );
    chunker.check();
}

void stateFree( State* state, bxAllocator* allocator )
{
    BX_FREE( allocator, state->_memory_handle );
    state[0] = {};
}

void computeClipTrajectory( ClipTrajectory* ct, const bxAnim_Clip* clip, float trajectoryDuration )
{
    const Vector4 plane = makePlane( Vector3::yAxis(), Vector3( 0.f ) );
    bxAnim_Joint root_joint;
    bxAnim::evaluateClip( &root_joint, clip, 0, 0.f, 0, 0 );
    const Vector3 root_displacement_xz = projectVectorOnPlane( root_joint.position, plane );
    const Vector3 root_displacement_y = mulPerElem( root_joint.position, Vector3( 0.f, 1.f, 0.f ) );

    bxAnim_Joint trajectory_joint;
    const float duration = trajectoryDuration;
    const float dt = ( duration ) / (float)( eNUM_TRAJECTORY_POINTS - 1 );
    const float start_time = 0.f;

    Vector3 extrapolation_dpos( 0.f );
    bool end_clip_reached = false;
    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        float trajectory_time = start_time + dt * i;
        if( trajectory_time <= clip->duration )
        {
            bxAnim::evaluateClip( &trajectory_joint, clip, trajectory_time, 0, 0 );
            trajectory_joint.position -= root_displacement_xz;
            trajectory_joint.position -= root_displacement_y;
            ct->pos[i] = trajectory_joint.position;
        }
        else
        {
            bxAnim_Joint last_joint, prev_last_joint;
            bxAnim::evaluateClip( &last_joint, clip, clip->numFrames - 1, 0.f, 0, 0 );
            bxAnim::evaluateClip( &prev_last_joint, clip, clip->numFrames - 2, 0.f, 0, 0 );
            bxAnim::evaluateClip( &trajectory_joint, clip, trajectory_time, 0, 0 );

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

    bxResourceManager* resource_manager = resourceManagerGet();

    _data.skel = bxAnimExt::loadSkelFromFile( resource_manager, skelFile );
    SYS_ASSERT( _data.skel != nullptr );

    _data.clips.reserve( numAnims );
    _data.clip_infos.reserve( numAnims );
    for( unsigned i = 0; i < numAnims; ++i )
    {
        bxAnim_Clip* clip = bxAnimExt::loadAnimFromFile( resource_manager, animInfo[i].name.c_str() );
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
    bxResourceManager* resource_manager = resourceManagerGet();
    while( !_data.clips.empty() )
    {
        bxAnim_Clip* c = _data.clips.back();
        bxAnimExt::unloadAnimFromFile( resource_manager, &c );
        _data.clips.pop_back();
    }
        
    bxAnim_Skel* skel = (bxAnim_Skel*)_data.skel;
    bxAnimExt::unloadSkelFromFile( resource_manager, &skel );
    _data.skel = nullptr;
}

void Context::prepare( const ContextPrepareInfo& info )
{
    const u32 num_match_joints_names = _eMATCH_JOINT_COUNT_;
    _data.match_joints_indices.resize( num_match_joints_names, -1 );

    for( u32 i = 0; i < num_match_joints_names; ++i )
    {
        _data.match_joints_indices[i] = bxAnim::getJointByName( _data.skel, info.matching_joint_names[i] );
    }

    prepare_evaluateClips();
        
    stateAllocate( &_state, _data.skel->numJoints, _allocator );
    _state.anim_ctx = bxAnim::contextInit( *_data.skel );
}

void Context::prepare_evaluateClips()
{
    const bxAnim_Skel* skel = _data.skel;
    std::vector< float > velocity;

    _data.clip_trajectiories.resize( _data.clips.size() );

    for( size_t i = 0; i < _data.clips.size(); ++i )
    {
        const bxAnim_Clip* clip = _data.clips[i];
        u32 frame_no = 0;

        float velocity_sum = 0.f;
        u32 velocity_count = 0;

        const float frame_dt = 1.f / clip->sampleFrequency;
        while( frame_no < clip->numFrames )
        {
            Pose pose;
            PosePrepareInfo pose_prepare_info = {};
            pose_prepare_info.clip = clip;
            pose_prepare_info.skel = skel;
            pose_prepare_info.frameNo = frame_no;
            pose_prepare_info.joint_indices = _data.match_joints_indices.data();

            poseAllocate( &pose, _data.skel->numJoints, _allocator );
            posePrepare( &pose, pose_prepare_info );

            pose.params.clip_index = (u32)i;
            pose.params.clip_start_time = frame_no * frame_dt;
            _data.poses.push_back( pose );

            const float pose_speed = length( pose.vel[0] ).getAsFloat();
            velocity_sum += pose_speed;

            velocity.push_back( pose_speed );
            ++velocity_count;
            ++frame_no;
        }
        
        ClipTrajectory& clipTrajectory = _data.clip_trajectiories[i];
        computeClipTrajectory( &clipTrajectory, clip, 1.f );
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
    bxAnim::contextDeinit( &_state.anim_ctx );

    stateFree( &_state, _allocator );

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
    const float desired_anim_speed = bx::curve::evaluate_catmullrom( _data.velocity_curve, input.speed01 );

    const Matrix4 to_local_space = inverse( input.base_matrix );
    Vector3 local_trajectory[eNUM_TRAJECTORY_POINTS];
    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        local_trajectory[i] = mulAsVec4( to_local_space, input.trajectory[i] );
    }
    const Vector3 local_velocity = to_local_space.getUpper3x3() * input.velocity;
    const Vector3 local_acceleration = to_local_space.getUpper3x3() * input.acceleration;
    const Vector3 desired_anim_velocity = normalizeSafe( local_velocity ) * desired_anim_speed;

    Vector3 local_trajectory_v[eNUM_TRAJECTORY_POINTS - 1];
    Vector3 local_trajectory_a[eNUM_TRAJECTORY_POINTS - 2];
    utils::computeTrajectoryDerivatives( local_trajectory_v, local_trajectory_a, local_trajectory, delta_time_inv );

    const u32 num_joints = _data.skel->numJoints;

    bxAnim_Joint* curr_local_joints = nullptr;

    Vector3 prev_matching_pos[_eMATCH_JOINT_COUNT_];
    Vector3 curr_matching_pos[_eMATCH_JOINT_COUNT_];
    Vector3 curr_matching_vel[_eMATCH_JOINT_COUNT_];

    memset( prev_matching_pos, 0x00, sizeof( prev_matching_pos ) );
    memset( curr_matching_pos, 0x00, sizeof( curr_matching_pos ) );
    memset( curr_matching_vel, 0x00, sizeof( curr_matching_vel ) );

    if( _state.num_clips )
    {
        bxAnim_Joint* tmp_joints = _state.joint_world;

        if( _state.clip_evaluated )
        {
            bxAnim_Joint* prev_local_joints = bxAnim::poseFromStack( _state.anim_ctx, 0 );
            bxAnimExt::localJointsToWorldJoints( tmp_joints, prev_local_joints, _data.skel, bxAnim_Joint::identity() );
            for( u32 i = 0; i < _data.match_joints_indices.size(); ++i )
            {
                i16 index = _data.match_joints_indices[i];
                prev_matching_pos[i] = tmp_joints[index].position;
            }
        }

        tick_animations( deltaTime );
        curr_local_joints = bxAnim::poseFromStack( _state.anim_ctx, 0 );
        curr_local_joints[0].position = mulPerElem( curr_local_joints[0].position, Vector3( 0.f, 1.f, 0.f ) );

        bxAnimExt::localJointsToWorldJoints( tmp_joints, curr_local_joints, _data.skel, bxAnim_Joint::identity() );

        for( u32 i = 0; i < _data.match_joints_indices.size(); ++i )
        {
            i16 index = _data.match_joints_indices[i];
            curr_matching_pos[i] = curr_local_joints[index].position;
            curr_matching_vel[i] = ( curr_local_joints[index].position - prev_matching_pos[i] ) * delta_time_inv;

        }

        _state.clip_evaluated = 1;
    }

    const float acceleration_value = length( input.acceleration ).getAsFloat();
        
    _debug.pose_indices.clear();

    if( curr_local_joints && _state.num_clips == 1 )
    {
        bxAnim_Joint* tmp_joints = _state.joint_world;
        float change_cost = FLT_MAX;
        u32 change_index = UINT32_MAX;

        const bool is_current_anim_looped = _data.clip_infos[_state.clip_index[0]].is_loop == 1;

        for( size_t i = 0; i < _data.poses.size(); ++i )
        {
            const Pose& pose = _data.poses[i];

            const AnimClipInfo& clip_info = _data.clip_infos[pose.params.clip_index];
            const bxAnim_Clip* clip = _data.clips[pose.params.clip_index];
            const bool is_pose_clip_looped = clip_info.is_loop == 1;

            const ClipTrajectory& pose_trajectory = _data.clip_trajectiories[pose.params.clip_index];

            const float trajectory_position_cost = utils::computePositionCost( local_trajectory, pose_trajectory.pos, eNUM_TRAJECTORY_POINTS );
            float trajectory_direction_cost = utils::computeDirectionCost( local_trajectory_v, pose_trajectory.vel, eNUM_TRAJECTORY_POINTS - 1 );
            trajectory_direction_cost += utils::computeDirectionCost( local_trajectory_a, pose_trajectory.acc, eNUM_TRAJECTORY_POINTS - 2 );

            const float pose_position_cost = utils::computePositionCost( pose.pos, curr_matching_pos, _eMATCH_JOINT_COUNT_ );
            const float pose_velocity_cost = utils::computeVelocityCost( pose.vel, curr_matching_vel, _eMATCH_JOINT_COUNT_ );
            const float velocity_cost = utils::computeVelocityCost( desired_anim_velocity, pose.vel[0] );

            const float trajectory_velocity_cost = utils::computeVelocityCost( local_trajectory_v, pose_trajectory.vel, eNUM_TRAJECTORY_POINTS - 1 );
            const float trajectory_acceleration_cost = utils::computeVelocityCost( local_trajectory_a, pose_trajectory.acc, eNUM_TRAJECTORY_POINTS - 2 );

            const float loop_cost = ( !is_current_anim_looped && _state.clip_index[0] == pose.params.clip_index ) ? 10.f : 0.f;

            float cost = 0.f;
            cost += pose_position_cost;
            cost += pose_velocity_cost;
            cost += velocity_cost;

            cost += trajectory_position_cost;
            cost += trajectory_direction_cost;
            cost += trajectory_velocity_cost;
            cost += trajectory_acceleration_cost;
            cost += loop_cost;

            if( change_cost > cost )
            {
                change_cost = cost;
                change_index = (u32)i;
                _debug.pose_indices.push_back( change_index );
            }
        }

        const Pose& winner_pose = _data.poses[change_index];

        const bool winner_is_at_the_same_location = ( _state.clip_index[0] == winner_pose.params.clip_index ); // && ::abs( _state.clip_eval_time[0] - winner_pose.params.clip_start_time ) < 0.2f;

        _state.pose_index = change_index;

        if( !winner_is_at_the_same_location )
        {
            tick_playClip( winner_pose.params.clip_index, winner_pose.params.clip_start_time, 0.25f );
            _state.pose_index = change_index;
        }


        {
            const ClipTrajectory& pose_trajectory = _data.clip_trajectiories[winner_pose.params.clip_index];
            for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
            {
                bxGfxDebugDraw::addSphere( Vector4( mulAsVec4( input.base_matrix, pose_trajectory.pos[i] ), 0.03f ), 0xffffffff, 1 );
            }
        }
    }
    else if( _state.num_clips == 0 )
    {
        const u32 change_index = 0;
        const Pose& winner_pose = _data.poses[change_index];
        tick_playClip( winner_pose.params.clip_index, winner_pose.params.clip_start_time, 0.25f );
        _state.pose_index = change_index;
    }

    if( curr_local_joints )
    {
        bxAnim_Joint root = toAnimJoint_noScale( input.base_matrix );
        bxAnimExt::localJointsToWorldJoints( _state.joint_world, curr_local_joints, _data.skel, root );
        const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _data.skel->offsetParentIndices );
        utils::debugDrawJoints( _state.joint_world, parent_indices, num_joints, 0xffffffFF, 0.025f, 1.f );
    }
}

void Context::tick_animations( float deltaTime )
{
    if( _state.num_clips == 1 )
    {
        bxAnim_Clip* clip = _data.clips[_state.clip_index[0]];
        float eval_time = _state.clip_eval_time[0];
        bxAnim_BlendLeaf leaf( clip, eval_time );
        bxAnimExt::processBlendTree( _state.anim_ctx, 0 | bxAnim::eBLEND_TREE_LEAF, nullptr, 0, &leaf, 1, _data.skel );

        eval_time = _state.clip_eval_time[0] + deltaTime;
        _state.clip_eval_time[0] = ::fmodf( eval_time, clip->duration );
    }
    else
    {
        const float blend_alpha = minOfPair( 1.f, _state._blend_time / _state._blend_duration );
        bxAnim_Clip* clipA = _data.clips[_state.clip_index[0]];
        const float eval_timeA = _state.clip_eval_time[0];

        bxAnim_Clip* clipB = _data.clips[_state.clip_index[1]];
        const float eval_timeB = _state.clip_eval_time[1];

        bxAnim_BlendLeaf leaves[2] =
        {
            bxAnim_BlendLeaf( clipA, eval_timeA ),
            bxAnim_BlendLeaf( clipB, eval_timeB ),
        };


        bxAnim_BlendBranch branch(
            0 | bxAnim::eBLEND_TREE_LEAF,
            1 | bxAnim::eBLEND_TREE_LEAF,
            blend_alpha
            );
        bxAnimExt::processBlendTree( _state.anim_ctx, 0 | bxAnim::eBLEND_TREE_BRANCH,
                                        &branch, 1, leaves, 2,
                                        _data.skel );

        if( _state._blend_time > _state._blend_duration )
        {
            _state.clip_index[0] = _state.clip_index[1];
            _state.clip_eval_time[0] = _state.clip_eval_time[1];
            _state.num_clips = 1;
        }
        else
        {
            _state._blend_time += deltaTime;
            const float clip_duration = lerp( blend_alpha, clipA->duration, clipB->duration );
            const float delta_phase = deltaTime / clip_duration;

            float phaseA = eval_timeA / clipA->duration; //::fmodf( ( ) + delta_phase, 1.f );
            float phaseB = eval_timeB / clipB->duration; //::fmodf( ( ) + delta_phase, 1.f );
            phaseA = ::fmodf( phaseA + delta_phase, 1.f );
            phaseB = ::fmodf( phaseB + delta_phase, 1.f );

            _state.clip_eval_time[0] = phaseA * clipA->duration;
            _state.clip_eval_time[1] = phaseB * clipB->duration;
        }
    }
}

void Context::tick_playClip( u32 clipIndex, float startTime, float blendTime )
{
    if( _state.num_clips == 0 )
    {
        _state.clip_index[0] = clipIndex;
        _state.clip_eval_time[0] = startTime;
        _state.num_clips = 1;
    }
    else
    {
        _state.clip_index[1] = clipIndex;
        _state.clip_eval_time[1] = startTime;
        _state.num_clips = 2;
        _state._blend_duration = blendTime;
        _state._blend_time = 0.f;
    }
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

void Context::_DebugDrawPose( u32 poseIndex, u32 color, const Matrix4& base )
{
    if( poseIndex >= _data.poses.size() )
        return;

    const Pose& pose = _data.poses[poseIndex];

    const bxAnim_Joint root_joint = toAnimJoint_noScale( base );
    //const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _data.skel->offsetParentIndices );

    const ClipTrajectory& pose_trajectory = _data.clip_trajectiories[pose.params.clip_index];

    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        bxGfxDebugDraw::addSphere( Vector4( pose_trajectory.pos[i], 0.03f ), 0x666666ff, 1 );
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
}
}}///


namespace bx{
namespace motion_matching{

void DynamicState::tick( const bxInput& sysInput, float deltaTime )
{
    characterInputCollectData( &_input, sysInput, deltaTime, 0.02f );

    const Vector3 x_input_force = Vector3::xAxis() * _input.analogX;
    const Vector3 z_input_force = -Vector3::zAxis() * _input.analogY;
    //const Vector3 y_input_force = Vector3::yAxis() * ( ( _input.jump > 0.1f ) ? _input.jump*100.f : -9.8f );
    Vector3 input_force = x_input_force + z_input_force;// +y_input_force;

    const float converge_time01 = 0.0001f;
    const float lerp_alpha = 1.f - ::pow( converge_time01, deltaTime );

    const float max_input_speed = lerp( _input.R2, 0.5f, 1.f );

    const float input_speed = minOfPair( max_input_speed, length( input_force ).getAsFloat() );
    input_force = normalizeSafe( input_force ) * input_speed;
    _prev_speed01 = _speed01;
    _speed01 = lerp( lerp_alpha, _speed01, input_speed );
    //_speed01 *= _speed01;
    //
    _prev_direction = _direction;
    if( input_speed * input_speed > FLT_EPSILON )
    {
        const Vector3 input_dir = normalize( input_force );
        //_direction = input_dir;
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
    _trajectory[0] = _position;
    Vector3 trajectory_velocity = _velocity;
    Vector3 trajectory_acceleration = _acceleration;
    //const Vector4 plane = makePlane( _direction, _position );
    //const Vector3 trajectory_acc = projectVectorOnPlane( _acceleration, plane );
    for( u32 i = 1; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        trajectory_velocity += trajectory_acceleration * dt;
        _trajectory[i] = _trajectory[i - 1] + trajectory_velocity * dt;

        bxGfxDebugDraw::addLine( _trajectory[i - 1], _trajectory[i], 0xFFFFFFFF, 1 );
        //bxGfxDebugDraw::addLine( _trajectory[i], _trajectory[i] + trajectory_velocity, 0x00FFFFFF, 1 );
        //bxGfxDebugDraw::addLine( _trajectory[i], _trajectory[i] + trajectory_acceleration, 0xFF0000FF, 1 );
    }

    //for( u32 i = 1; i < eNUM_TRAJECTORY_POINTS; ++i )
    //{
    //    /// s = s0 + v0 * t + a*t2*0.5f
    //    _trajectory[i] = _position + _velocity * time + _acceleration*time * 0.5f;
    //    //checkFloat( _trajectory[i].getX().getAsFloat() );
    //    time += dt;
    //}

}

Matrix4 DynamicState::computeBaseMatrix() const
{
    if( lengthSqr( _direction ).getAsFloat() > FLT_EPSILON )
    {
        Vector3 y = Vector3::yAxis();
        Vector3 x = normalize( cross( y, _direction ) );
        Vector3 z = normalize( cross( x, y ) );
        Matrix3 rotation = Matrix3( x, y, z );

        //const floatInVec d = dot( _acceleration, x );
        //Matrix3 tilt_rotation = Matrix3::rotationZ( -d );
        //rotation *= tilt_rotation;

        return Matrix4( rotation, _position );
    }
    return Matrix4::identity();
}

void DynamicState::debugDraw( u32 color )
{
    bxGfxDebugDraw::addLine( _position, _position + _direction, 0x0000FFFF, 1 );
    bxGfxDebugDraw::addLine( _position, _position + _direction * _speed01, color, 1 );

    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        bxGfxDebugDraw::addSphere( Vector4( _trajectory[i], 0.03f ), 0x666666ff, 1 );
    }

    const Matrix4 base = computeBaseMatrix();
    bxGfxDebugDraw::addAxes( base );
}

void motionMatchingCollectInput( Input* input, const DynamicState& dstate )
{
    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        input->trajectory[i] = dstate._trajectory[i];

    input->velocity = dstate._velocity;
    input->acceleration = dstate._acceleration;
    input->base_matrix = dstate.computeBaseMatrix();
    input->speed01 = dstate._speed01;
}

}}////

