#include "motion_fields.h"
#include <anim/anim.h>
#include <resource_manager/resource_manager.h>
#include <algorithm>
#include <xfunctional>
#include <gfx/gfx_debug_draw.h>

#include <util/common.h>
#include "util/buffer_utils.h"
#include <gfx/gui/imgui/imgui.h>

namespace bx{
namespace motion_fields
{
    static const float EVALUATION_FREQUENCY = 30.f;
    static const float EVALUATION_TIME_STEP = 1.f / EVALUATION_FREQUENCY;

    void _MotionStatePoseAllocate( MotionState::Pose* pose, unsigned numJoints, bxAllocator* allocator )
    {
        if( pose->joints )
        {
            BX_FREE0( allocator, pose->joints );
        }

        pose->joints = (bxAnim_Joint*)BX_MALLOC( allocator, numJoints * sizeof( *pose->joints ), 16 );

    }
    void _MotionStatePoseDeallocate( MotionState::Pose* pose, bxAllocator* allocator )
    {
        BX_FREE0( allocator, pose->joints );
    }

    void MotionState::allocate( unsigned numJoints, bxAllocator* allocator )
    {
        _MotionStatePoseAllocate( &_x, numJoints, allocator );
        _MotionStatePoseAllocate( &_v, numJoints, allocator );
        _MotionStatePoseAllocate( &_a, numJoints, allocator );

        _numJoints = numJoints;
    }
    void MotionState::deallocate( bxAllocator* allocator )
    {
        _MotionStatePoseDeallocate( &_a, allocator );
        _MotionStatePoseDeallocate( &_v, allocator );
        _MotionStatePoseDeallocate( &_x, allocator );
    }

    /// produces c = a - b
    void _ComputeDifference( bxAnim_Joint* c, const bxAnim_Joint* a, const bxAnim_Joint* b, u32 numJoints )
    {
        for( u32 i = 0; i < numJoints; ++i )
        {
            bxAnim_Joint& cJoint = c[i];
            const bxAnim_Joint& aJoint = a[i];
            const bxAnim_Joint& bJoint = b[i];

            cJoint.position = aJoint.position - bJoint.position;
            cJoint.rotation = aJoint.rotation * conj( bJoint.rotation );
            cJoint.scale = aJoint.scale - bJoint.scale;
        }
    }

    void MotionState::prepare( bxAnim_Clip* clip, float evalTime, float timeStep, const bxAnim_Skel* skel )
    {
        _clip = clip;
        _clip_eval_time = evalTime;

        _Evaluate( clip, evalTime, timeStep );
        _Transform( skel );
        _Difference();
        
        bxAnim::evaluateClip( jointsX(), clip, evalTime );
        jointsX()[0].position = mulPerElem( jointsX()[0].position, Vector3( 0.f, 1.f, 0.f ) );
        std::vector< bxAnim_Joint > scratch;
        scratch.resize( skel->numJoints );
        const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, skel->offsetParentIndices );
        bxAnim::localJointsToWorldJoints( scratch.data(), _x.joints, parent_indices, _numJoints, bxAnim_Joint::identity() );
        memcpy( jointsX(), scratch.data(), skel->numJoints * sizeof( bxAnim_Joint ) );
    }

    void MotionState::_Evaluate( bxAnim_Clip* clip, float evalTime, float timeStep )
    {
        const float duration = clip->duration;
        const float dt = ( duration - evalTime ) / float( eNUM_TRAJECTORY_POINTS - 1 );
        const Vector4 plane = makePlane( Vector3::yAxis(), Vector3( 0.f ) );
        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        {
            const float clipTime = evalTime + dt * i;
            bxAnim::evaluateClip( _x.joints, clip, clipTime );
            _trajectory[i] = projectVectorOnPlane( _x.joints[0].position, plane );
        }
        
        bxAnim::evaluateClip( _x.joints, clip, evalTime );
        bxAnim::evaluateClip( _v.joints, clip, evalTime + timeStep );
        bxAnim::evaluateClip( _a.joints, clip, evalTime + timeStep*2.f );
    }

    void MotionState::_Transform( const bxAnim_Skel* skel )
    {
        std::vector< bxAnim_Joint > scratch;
        scratch.resize( skel->numJoints );
        
        const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, skel->offsetParentIndices );
        
        bxAnim::localJointsToWorldJoints( scratch.data(), _x.joints, parent_indices, _numJoints, bxAnim_Joint::identity() );
        memcpy( _x.joints, scratch.data(), skel->numJoints * sizeof( bxAnim_Joint ) );

        bxAnim::localJointsToWorldJoints( scratch.data(), _v.joints, parent_indices, _numJoints, bxAnim_Joint::identity() );
        memcpy( _v.joints, scratch.data(), skel->numJoints * sizeof( bxAnim_Joint ) );

        bxAnim::localJointsToWorldJoints( scratch.data(), _a.joints, parent_indices, _numJoints, bxAnim_Joint::identity() );
        memcpy( _a.joints, scratch.data(), skel->numJoints * sizeof( bxAnim_Joint ) );
    }

    void MotionState::_Difference()
    {
        _ComputeDifference( _a.joints, _a.joints, _v.joints, _numJoints );
        _ComputeDifference( _v.joints, _v.joints, _x.joints, _numJoints );
    }

    void Data::load( const char* skelFile, const char** animFiles, unsigned numAnimFiles )
    {
        if( !_allocator )
        {
            _allocator = bxDefaultAllocator();
        }

        bxResourceManager* resource_manager = resourceManagerGet();

        _skel = bxAnimExt::loadSkelFromFile( resource_manager, skelFile );
        SYS_ASSERT( _skel != nullptr );

        _clips.reserve( numAnimFiles );
        for( unsigned i = 0; i < numAnimFiles; ++i )
        {
            bxAnim_Clip* clip = bxAnimExt::loadAnimFromFile( resource_manager, animFiles[i] );
            if( !clip )
            {
                continue;
            }

            _clips.push_back( clip );
        }
    }

    void Data::unload()
    {
        bxResourceManager* resource_manager = resourceManagerGet();
        while( !_clips.empty() )
        {
            bxAnim_Clip* c = _clips.back();
            bxAnimExt::unloadAnimFromFile( resource_manager, &c );
            _clips.pop_back();
        }

        bxAnimExt::unloadSkelFromFile( resource_manager, &_skel );
        
    }

    void Data::prepare()
    {
        _EvaluateClips( EVALUATION_TIME_STEP );
        _ComputeNeighbourhood();
    }

    void Data::unprepare()
    {
        while( !_states.empty() )
        {
            MotionState& ms = _states.back();
            ms.deallocate( _allocator );
            _states.pop_back();
        }
    }

    void _DebugDrawJoints( const bxAnim_Joint* joints, const u16* parentIndices, u32 numJoints, u32 color, float radius, float scale )
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

    void Data::debugDrawState( unsigned index, u32 color, bool drawNeighbours )
    {
        if( index >= _states.size() )
            return;

        const MotionState& ms = _states[index];

        const bxAnim_Joint* joints = ms.jointsX();
        const bxAnim_Joint* jointsV = ms.jointsV();
        const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _skel->offsetParentIndices );

        _debug_joints.resize( _skel->numJoints );
        //const bxAnim_Joint root_joint = bxAnim_Joint::identity();
        //bxAnim::localJointsToWorldJoints( _debug_joints.data(), joints, parent_indices, _skel->numJoints, root_joint );
        const float sph_radius = 0.05f;
        _DebugDrawJoints( joints, parent_indices, _skel->numJoints, color, sph_radius, 1.f );

        bxGfxDebugDraw::addLine( joints->position, joints->position + jointsV->position * 30.f, 0xFFFF00FF, 1 );

        if( drawNeighbours )
        {
            const NeighbourIndices& ni = _neighbours[index];
            const NeighbourValue& weights = _neighbour_weight[index];
            for( u32 i = 0; i < eNUM_NEIGHBOURS; ++i )
            {
                const MotionState& neighbour_state = _states[ni.i[i]];
                const bxAnim_Joint* neighbour_joints = neighbour_state.jointsX();
                const bxAnim_Joint* neighbour_jointsV = neighbour_state.jointsV();
                //bxAnim::localJointsToWorldJoints( _debug_joints.data(), neighbour_joints, parent_indices, _skel->numJoints, root_joint );
                //_DebugDrawJoints( _debug_joints.data(), parent_indices, _skel->numJoints, color, sph_radius * weights.v[i], 1.f );
                _DebugDrawJoints( neighbour_joints, parent_indices, _skel->numJoints, 0x66666666, sph_radius * weights.v[i], 1.f );



                bxGfxDebugDraw::addLine( neighbour_joints->position, neighbour_joints->position + neighbour_jointsV->position * 30.f, 0xFFFF00FF, 1 );
            }
        }

        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        {
            bxGfxDebugDraw::addSphere( Vector4( ms._trajectory[i], 0.03f ), 0x000066ff, 1 );
        }

    }

    float _ComputeGoalFactor( const MotionState& ms, const Vector3 desiredDirection, float desiredSpeed, float sampleFrequency )
    {
        const Vector3 ms_vel = ms.jointsV()->position * sampleFrequency;
        const Vector3 ms_dir = fastRotate( ms.jointsX()->rotation, Vector3::zAxis() );

        const float dir_factor = 1.f - dot( ms_dir, desiredDirection ).getAsFloat();
        const float spd_factor = length( ms_vel ).getAsFloat() - desiredSpeed;
        const float goal_factor = ::sqrt( ( dir_factor *dir_factor ) * ( spd_factor * spd_factor ) );

        return goal_factor;
    }

    u32 Data::findState( const Vector3 desiredDirection, float desiredSpeed )
    {
        const float sample_frequency = EVALUATION_FREQUENCY;
        u32 best_index = UINT32_MAX;
        float best_factor = FLT_MAX;

        for( size_t i = 0; i < _states.size(); ++i )
        {
            const MotionState& ms = _states[i];
            const float goal_factor = _ComputeGoalFactor( ms, desiredDirection, desiredSpeed, sample_frequency );
            if( best_factor > goal_factor )
            {
                best_index = (u32)i;
                best_factor = goal_factor;
            }
        }

        return best_index;
    }

    u32 Data::findNextState( u32 currentStateIndex, const Vector3 desiredDirection, float desiredSpeed )
    {
        const float sample_frequency = EVALUATION_FREQUENCY;
        u32 best_index = UINT32_MAX;
        float best_factor = FLT_MAX;

        const NeighbourIndices& indices = _neighbours[currentStateIndex];

        for( u32 i = 0; i < eNUM_NEIGHBOURS; ++i )
        {
            const u32 neighbour_index = indices.i[i];
            const MotionState& ms = _states[neighbour_index];
            const float goal_factor = _ComputeGoalFactor( ms, desiredDirection, desiredSpeed, sample_frequency );
            if( best_factor > goal_factor )
            {
                best_index = neighbour_index;
                best_factor = goal_factor;
            }
        }

        return best_index;
    }

    u32 Data::getMostSimilarState( u32 currentStateIndex )
    {
        const NeighbourIndices& indices = _neighbours[currentStateIndex];
        return indices.i[0];
    }

    void Data::_EvaluateClips( float timeStep )
    {

        for( u32 iclip = 0; iclip < (u32)_clips.size(); ++iclip )
        {
            bxAnim_Clip* clip = _clips[iclip];
            float time = 0.f;
            while( time <= clip->duration )
            {
                MotionState ms = {};

                ms.allocate( clip->numJoints, _allocator );
                ms.prepare( clip, time, timeStep, _skel );

                _states.push_back( ms );

                time += timeStep;
            }
        }
    }

    float _ComputeSimilarity( const MotionState& msa, const MotionState& msb, const float* betha, const Vector3 u )
    {
        const float vr = lengthSqr( msa._v.joints[0].position - msb._v.joints[0].position ).getAsFloat() * betha[0];
        const float q0 = lengthSqr( rotate( msa._v.joints[0].rotation, u ) - rotate( msb._v.joints[0].rotation, u ) ).getAsFloat() * betha[0];

        float pi = 0.f;
        float qpi = 0.f;
        for( u32 i = 1; i < msa._numJoints; ++i )
        {
            const Vector3 xa = rotate( msa._x.joints[i].rotation, u );
            const Vector3 xb = rotate( msb._x.joints[i].rotation, u );
            pi += lengthSqr( xa - xb ).getAsFloat() * betha[i];

            const Quat qp_a = msa._v.joints[i].rotation * msa._x.joints[i].rotation;
            const Quat qp_b = msb._v.joints[i].rotation * msb._x.joints[i].rotation;
            const Vector3 va = rotate( qp_a, u );
            const Vector3 vb = rotate( qp_b, u );

            qpi += lengthSqr( va - vb ).getAsFloat() * betha[i];
        }

        const float result = ::sqrt( vr + q0 + pi + qpi );
        return result;
    }


    struct Similarity
    {
        float value = 0.f;
        u32 index = 0;

        bool operator < ( Similarity a ) const { return value < a.value; }
        bool operator >( Similarity a ) const { return value > a.value; }
    };

    void Data::_ComputeNeighbourhood()
    {
        std::vector<float> bone_lengths;
        bone_lengths.reserve( _skel->numJoints );
        
        const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _skel->offsetParentIndices );
        const bxAnim_Joint* base_pose = TYPE_OFFSET_GET_POINTER( bxAnim_Joint, _skel->offsetBasePose );

        for( u32 i = 0; i < _skel->numJoints; ++i )
        {
            const u16 parent_idx = parent_indices[i];
            if( parent_idx == UINT16_MAX )
            {
                bone_lengths.push_back( 0.5f );
                continue;
            }

            const bxAnim_Joint& parent = base_pose[parent_idx];
            const bxAnim_Joint& child = base_pose[i];
            const float blength = length( parent.position - child.position ).getAsFloat();
            bone_lengths.push_back( blength );
        }

        const u32 num_states = (u32)_states.size();
        std::vector< Similarity > similarity;
        similarity.resize( num_states );

        const Vector3 u( 0.1f, 0.6f, 0.25f );

        for( u32 i = 0; i < num_states; ++i )
        {
            const MotionState& base_ms = _states[i];
            similarity.clear();
            for( u32 j = 0; j < num_states; ++j )
            {
                if( i == j )
                    continue;

                const MotionState& second_ms = _states[j];
                Similarity sim = {};
                sim.value = _ComputeSimilarity( base_ms, second_ms, bone_lengths.data(), u );
                sim.index = j;
                similarity.push_back( sim );

            }
            std::sort( similarity.begin(), similarity.end(), std::less< Similarity >() );
            
            float sim_sum = 0.f;
            for( u32 j = 0; j < eNUM_NEIGHBOURS; ++j )
            {
                const float v = similarity[j].value;
                sim_sum += 1.f / ( v * v );
            }
            
            NeighbourIndices ni = {};
            NeighbourValue ns = {};
            NeighbourValue nw = {};
            for( u32 j = 0; j < eNUM_NEIGHBOURS; ++j )
            {
                ni.i[j] = similarity[j].index;

                const float s_value = similarity[j].value;
                ns.v[j] = s_value;
                nw.v[j] = 1.f / ( sim_sum * s_value*s_value );
            }

            _neighbours.push_back( ni );
            _neighbour_similarity.push_back( ns );
            _neighbour_weight.push_back( nw );
        }
    }

    //////////////////////////////////////////////////////////////////////////
    void DynamicState::tick( const bxInput& sysInput, float deltaTime )
    {
        characterInputCollectData( &_input, sysInput, deltaTime, 0.1f );
        
        const Vector3 x_input_force = Vector3::xAxis() * _input.analogX;
        const Vector3 z_input_force = -Vector3::zAxis() * _input.analogY;
        //const Vector3 y_input_force = Vector3::yAxis() * ( ( _input.jump > 0.1f ) ? _input.jump*10.f : -1.8f );
        Vector3 input_force = x_input_force + z_input_force; // +y_input_force;

        const float converge_time01 = 0.01f;
        const float lerp_alpha = 1.f - ::pow( converge_time01, deltaTime );

        const float input_speed = minOfPair( 1.f, length( input_force ).getAsFloat() );
        input_force = normalizeSafe( input_force ) * input_speed;
        _prev_speed01 = _speed01;
        _speed01 = lerp( lerp_alpha, _speed01, input_speed );
        //
        _prev_direction = _direction;
        if( input_speed > FLT_EPSILON )
        {
            const Vector3 input_dir = normalize( input_force );
            _direction = slerp( lerp_alpha, _direction, input_force );
            _direction = normalizeSafe( _direction );
        }


        const float spd = ( _speed01 < 0.01f ) ? 0: _max_speed; // *_speed01;
        _acceleration = ( _direction - _prev_direction ) * spd;
        _velocity = _direction * spd;
        _position += _velocity * deltaTime;
        

        //////////////////////////////////////////////////////////////////////////
        //mothahack
        if( _position.getY().getAsFloat() < 0.f )
        {
            _position.setY( 0.f );
        }
        //////////////////////////////////////////////////////////////////////////
        //const float prev_spd = _max_speed * _prev_speed01;
        //const float spd = _max_speed * _speed01;

        
        //Vector3 acceleration = ( _direction - _prev_direction ) * spd;
        //_acceleration = ( input_force - _prev_input_force );
        //_velocity = _direction * _speed01;
        const float delta_time_inv = ( deltaTime > 0.f ) ? 1.f / deltaTime : 0.f;
        const float dt = _trajectory_integration_time / ( eNUM_TRAJECTORY_POINTS - 1 );
        float time = dt;
        _trajectory[0] = _position;
        for( u32 i = 1; i < eNUM_TRAJECTORY_POINTS; ++i )
        {
            /// s = s0 + v0 * t + a*t2*0.5f
            //_trajectory[i] = _position + (_direction * _max_speed ) * time + acceleration * ( time*time );
            //_trajectory[i] = _position + input_force*time + acceleration*time*time*0.5f;
            _trajectory[i] = _position + _velocity * time + _acceleration*delta_time_inv*time*time * 0.5f;
            //_trajectory[i] = _position + normalizeSafe( input_force ) * time; // +input_force*( time*time ) * 0.5f;
            time += dt;
        }

        const float dt_inv = ( deltaTime > 0 ) ? 1.f / deltaTime : 0.f;
        bxGfxDebugDraw::addLine( _trajectory[eNUM_TRAJECTORY_POINTS - 1], _trajectory[eNUM_TRAJECTORY_POINTS - 1] + _acceleration * dt_inv, 0xFFFFFFFF, 1 );

        //_position += (_direction * _max_speed ) * deltaTime;
        //_prev_input_force = input_force;

        //_acceleration = input_force / _mass;
        //_velocity += _acceleration * deltaTime;

        //const float spd = length( _velocity ).getAsFloat();
        //const float velScaler = ( _max_speed > 0.f ) ? _max_speed / spd : 0.f;
        //_velocity *= ::pow( 1.f - 0.99f, deltaTime );
        //_velocity *= ( spd > _max_speed ) ? velScaler : 1.f;
        //_position += _velocity * deltaTime;

        //float trajectory_time = 0.f;
        //const float trajectory_dt = 1.f / (eNUM_TRAJECTORY_POINTS-1);
        //for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        //{
        //    _trajectory[i] = _position + _velocity * trajectory_time + _acceleration * ( trajectory_time*trajectory_time )*0.5f;
        //    trajectory_time += trajectory_dt;
        //}

        //_speed01 = linearstep( 0.f, _max_speed, length( _velocity ).getAsFloat() );
        //if( _speed01 > 0.f )
        //{
        //    _direction = normalize( _velocity );
        //}
    }
    void DynamicState::computeLocalTrajectory( Vector3 localTrajectory[eNUM_TRAJECTORY_POINTS], const Matrix4& base )
    {
        const Matrix4 base_inv = inverse( base );
        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        {
            localTrajectory[i] = mulAsVec4( base_inv, _trajectory[i] );
        }
    }

    Matrix4 DynamicState::_ComputeBaseMatrix() const
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









    //////////////////////////////////////////////////////////////////////////
    void MotionMatching::poseAllocate( Pose* pose, u32 numJoints, bxAllocator* allocator )
    {
        u32 mem_size = 0;
        mem_size += numJoints * ( sizeof( *pose->joints ) ); // +sizeof( *pose->joints_v ) + sizeof( *pose->joints_a ) );
        mem_size += eNUM_TRAJECTORY_POINTS * sizeof( *pose->trajectory );

        void* mem = BX_MALLOC( allocator, mem_size, 16 );
        memset( mem, 0x00, mem_size );

        bxBufferChunker chunker( mem, mem_size );
        pose->joints = chunker.add< bxAnim_Joint >( numJoints );
        //pose->joints_v = chunker.add< bxAnim_Joint >( numJoints );
        //pose->joints_a = chunker.add< bxAnim_Joint >( numJoints );
        pose->trajectory = chunker.add< Vector3 >( eNUM_TRAJECTORY_POINTS );
        chunker.check();
    }
    void MotionMatching::poseFree( Pose* pose, bxAllocator* allocator )
    {
        BX_FREE( allocator, pose->joints );
        pose[0] = {};
    }

    void MotionMatching::posePrepare( Pose* pose, const bxAnim_Skel* skel, const bxAnim_Clip* clip, u32 frameNo, const AnimClipInfo& clipInfo )
    {
        bxAnim_Joint* joints = pose->joints;
        //bxAnim_Joint* joints_v = pose->joints_v;
        //bxAnim_Joint* joints_a = pose->joints_a;
        const Vector4 plane = makePlane( Vector3::yAxis(), Vector3( 0.f ) );
        
        bxAnim::evaluateClip( joints, clip, frameNo, 0.f );
        const Vector3 root_displacement_xz = projectVectorOnPlane( joints[0].position, plane );

        bxAnim_Joint trajectory_joint;
        const float dt = ( clip->duration ) / (float)( eNUM_TRAJECTORY_POINTS - 1 );
        const float start_time = (float)frameNo / clip->sampleFrequency;
        
        //if( clipInfo.is_loop == 0 )
        //{
        //    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        //    {
        //        const float trajectory_time = ::fmodf( start_time + dt * i, clip->duration );
        //        bxAnim::evaluateClip( &trajectory_joint, clip, trajectory_time, 0, 0 );

        //        trajectory_joint.position -= root_displacement_xz;
        //        pose->trajectory[i] = trajectory_joint.position;
        //    }
        //}
        //else
        {
            //bool clip_end_reached = false;
            //Vector3 displacement{ 0.f };

            for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
            {
                const float trajectory_time = start_time + dt * i;
                if( trajectory_time <= clip->duration )
                {
                    bxAnim::evaluateClip( &trajectory_joint, clip, trajectory_time, 0, 0 );
                    trajectory_joint.position -= root_displacement_xz;
                    pose->trajectory[i] = trajectory_joint.position;
                }
                else
                {
                    bxAnim::evaluateClip( &trajectory_joint, clip, clip->numFrames - 1, 0.f, 0, 0 );
                    trajectory_joint.position -= root_displacement_xz;
                    
                    const float trajectory_time1 = ::fmodf( trajectory_time + dt, clip->duration );
                    bxAnim_Joint trajectory_joint0;
                    bxAnim::evaluateClip( &trajectory_joint0, clip, 0, 0.f, 0, 0 );
                    
                    bxAnim_Joint trajectory_joint1;
                    bxAnim::evaluateClip( &trajectory_joint1, clip, trajectory_time1, 0, 0 );
                    const Vector3 dpos = trajectory_joint1.position - trajectory_joint0.position;

                    //if( !clip_end_reached )
                    //{
                    //    bxAnim::evaluateClip( &trajectory_joint0, clip, clip->numFrames-2, 0.f, 0, 0 );
                    //    trajectory_joint0.position -= root_displacement_xz;
                    //    displacement = trajectory_joint.position - trajectory_joint0.position;
                    //    clip_end_reached = true;
                    //}

                    pose->trajectory[i] = trajectory_joint.position + dpos;
                }
            }
        }


        //const float duration = clip->duration;
        //const float dt = duration / float( eNUM_TRAJECTORY_POINTS - 1 );
        //
        //Vector3 prev_point( 0.f );
        //float diff = ::abs( clip->duration - evalTime );
        //if( diff < dt - FLT_EPSILON )
        //{
        //    bxAnim::evaluateClip( joints, clip, evalTime - dt );
        //    Vector3 p0 = joints[0].position - root_displacement_xz;
        //    //p0 = projectVectorOnPlane( p0, plane );
        //    
        //    bxAnim::evaluateClip( joints, clip, evalTime );
        //    Vector3 p1 = joints[0].position - root_displacement_xz;
        //    //p1 = projectVectorOnPlane( p1, plane );

        //    Vector3 displ = p1 - p0;
        //    
        //    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        //    {
        //        pose->trajectory[i] = p1 + displ * (float)i;
        //    }
        //}
        //else
        //{
        //    u32 last = 0;
        //    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        //    {
        //        const float clipTime = evalTime + dt * i;
        //        if( clipTime <= clip->duration )
        //        {
        //            bxAnim::evaluateClip( joints, clip, clipTime );
        //            pose->trajectory[i] = joints[0].position - root_displacement_xz;
        //            //pose->trajectory[i] = projectVectorOnPlane( pose->trajectory[i], plane );
        //            prev_point = pose->trajectory[i];
        //        }
        //        else
        //        {
        //            SYS_ASSERT( i > 1 );
        //            Vector3 p0 = pose->trajectory[i - 2];
        //            Vector3 p1 = pose->trajectory[i - 1];
        //            pose->trajectory[i] = p1 + ( p1 - p0 );
        //        }
        //    }
        //}

        //bxAnim::evaluateClip( joints, clip, evalTime );
        //joints[0].position = joints[0].position - root_displacement_xz;

        //std::vector< bxAnim_Joint > vector_joints_v;
        //std::vector< bxAnim_Joint > vector_joints_a;
        //vector_joints_v.resize( skel->numJoints );
        //vector_joints_a.resize( skel->numJoints );
        //bxAnim_Joint* joints_v = vector_joints_v.data();
        //bxAnim_Joint* joints_a = vector_joints_a.data();

        //bxAnim::evaluateClip( joints_scratch.data(), clip, evalTime + EVALUATION_TIME_STEP );
        //_ComputeDifference( joints_v, joints_scratch.data(), joints, skel->numJoints );

        
        bxAnim_Joint j0, j1;
        Vector3 v0, v1;

        const u32 lastFrame = clip->numFrames - 1;
        if( frameNo == 0 )
        {
            bxAnim::evaluateClip( &j0, clip, lastFrame-1, 0.f, 0, 0 );
            bxAnim::evaluateClip( &j1, clip, lastFrame  , 0.f, 0, 0 );

            v0 = ( j1.position - j0.position ) * clip->sampleFrequency;

            bxAnim::evaluateClip( &j0, clip, 0, 0.f, 0, 0 );
            bxAnim::evaluateClip( &j1, clip, 1, 0.f, 0, 0 );

            v1 = ( j1.position - j0.position ) * clip->sampleFrequency;
        }
        else if( frameNo == lastFrame )
        {
            bxAnim::evaluateClip( &j0, clip, 0, 0.f, 0, 0 );
            bxAnim::evaluateClip( &j1, clip, 1, 0.f, 0, 0 );

            v0 = ( j1.position - j0.position ) * clip->sampleFrequency;

            bxAnim::evaluateClip( &j0, clip, frameNo - 1, 0.f, 0, 0 );
            bxAnim::evaluateClip( &j1, clip, frameNo, 0.f, 0, 0 );
            
            v1 = ( j1.position - j0.position ) * clip->sampleFrequency;
        }
        else
        {
            bxAnim::evaluateClip( &j0, clip, frameNo - 1, 0.f, 0, 0 );
            bxAnim::evaluateClip( &j1, clip, frameNo, 0.f, 0, 0 );

            v0 = ( j1.position - j0.position ) * clip->sampleFrequency;

            bxAnim::evaluateClip( &j0, clip, frameNo, 0.f, 0, 0 );
            bxAnim::evaluateClip( &j1, clip, frameNo + 1, 0.f, 0, 0 );
            
            v1 = ( j1.position - j0.position ) * clip->sampleFrequency;
        }
        //bxAnim::evaluateClip( joints_v, clip, evalTime + EVALUATION_TIME_STEP );
        //bxAnim::evaluateClip( joints_a, clip, evalTime + EVALUATION_TIME_STEP * 2.f );
        //
        //
        //_ComputeDifference( joints_a, joints_a, joints_v, skel->numJoints );
        //_ComputeDifference( joints_v, joints_v, joints, skel->numJoints );
        //_ComputeDifference( joints_a, joints_a, joints_v, skel->numJoints );

        /// remove root motion
        joints[0].position = mulPerElem( joints[0].position, Vector3( 0.f, 1.f, 0.f ) );

        PoseParams& params = pose->params;
        params.velocity = v1;
        params.acceleration = v1 - v0;
        
        //joints_scratch[0].position = mulPerElem( joints_scratch[0].position, Vector3( 0.f, 1.f, 0.f ) );
    }

    void MotionMatching::stateAllocate( State* state, u32 numJoints, bxAllocator* allocator )
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

    void MotionMatching::stateFree( State* state, bxAllocator* allocator )
    {
        BX_FREE( allocator, state->_memory_handle );
        state[0] = {};
    }

//-----------------------------------------------------------------------
    void MotionMatching::load( const char* skelFile, const char** animFiles, const AnimClipInfo* animInfo, unsigned numAnimFiles )
    {
        if( !_allocator )
        {
            _allocator = bxDefaultAllocator();
        }

        bxResourceManager* resource_manager = resourceManagerGet();

        _data.skel = bxAnimExt::loadSkelFromFile( resource_manager, skelFile );
        SYS_ASSERT( _data.skel != nullptr );

        _data.clips.reserve( numAnimFiles );
        for( unsigned i = 0; i < numAnimFiles; ++i )
        {
            bxAnim_Clip* clip = bxAnimExt::loadAnimFromFile( resource_manager, animFiles[i] );
            if( !clip )
            {
                continue;
            }

            _data.clip_names.push_back( animFiles[i] );
            _data.clip_infos.push_back( animInfo[i] );
            _data.clips.push_back( clip );
        }
    }

    void MotionMatching::unload()
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

    void MotionMatching::prepare()
    {
        _EvaluateClips( EVALUATION_TIME_STEP );
        _ComputeBoneLengths();

        stateAllocate( &_state, _data.skel->numJoints, _allocator );
        _state.anim_ctx = bxAnim::contextInit( *_data.skel );
    }
    void MotionMatching::_EvaluateClips( float timeStep )
    {
        const bxAnim_Skel* skel = _data.skel;

        std::vector< float > velocity;

        for( size_t i = 0; i < _data.clips.size(); ++i )
        {
            const bxAnim_Clip* clip = _data.clips[i];
            //float time = 0.f;
            u32 frame_no = 0;

            float velocity_sum = 0.f;
            u32 velocity_count = 0;

            //while( time < clip->duration )
            const float frame_dt = 1.f / clip->sampleFrequency;

            while( frame_no < clip->numFrames )
            {
                Pose pose;
                poseAllocate( &pose, _data.skel->numJoints, _allocator );
                posePrepare( &pose, skel, clip, frame_no, _data.clip_infos[i] );

                pose.params.clip_index = (u32)i;
                pose.params.clip_start_time = frame_no * frame_dt;
                _data.poses.push_back( pose );

                //const Vector3 pose_velocity = pose.joints_v[0].position * EVALUATION_FREQUENCY;
                //const float pose_speed = length( pose_velocity ).getAsFloat();
                //velocity.push_back( pose_speed );
                velocity_sum += length( pose.params.velocity ).getAsFloat();
                ++velocity_count;

                //time += timeStep;
                ++frame_no;
            }
        
            float velocity_avg = velocity_sum / (float)velocity_count;
            velocity.push_back( velocity_avg );
        }

        float velocity_vel_len = 0.f;
        std::sort( velocity.begin(), velocity.end(), std::less<float>() );
        curve::allocate( _data.velocity_curve, (u32)velocity.size() );
        for( u32 i = 0; i < velocity.size(); ++i )
        {
            const float t = (float)i / float( velocity.size() - 1 );
            curve::push_back( _data.velocity_curve, velocity[i], t );
        }

    }
    void MotionMatching::_ComputeBoneLengths()
    {
        const bxAnim_Skel* skel = _data.skel;
        _data.bone_lengths.reserve( skel->numJoints );

        const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, skel->offsetParentIndices );
        const bxAnim_Joint* base_pose = TYPE_OFFSET_GET_POINTER( bxAnim_Joint, skel->offsetBasePose );

        for( u32 i = 0; i < skel->numJoints; ++i )
        {
            const u16 parent_idx = parent_indices[i];
            if( parent_idx == UINT16_MAX )
            {
                _data.bone_lengths.push_back( 0.5f );
                continue;
            }

            const bxAnim_Joint& parent = base_pose[parent_idx];
            const bxAnim_Joint& child = base_pose[i];
            const float blength = length( parent.position - child.position ).getAsFloat();
            _data.bone_lengths.push_back( blength );
        }
    }


    bool MotionMatching::currentSpeed( f32* value )
    {
        if( _state.num_clips == 0 )
        {
            return false;
        }

        if( _state.num_clips == 1 )
        {
            const Pose& pose = _data.poses[_state.clip_index[0]];
            value[0] = length( pose.params.velocity ).getAsFloat();
        }
        else
        {
            const Pose& poseA = _data.poses[_state.clip_index[0]];
            const Pose& poseB = _data.poses[_state.clip_index[1]];
            const float blend_alpha = _state._blend_time / _state._blend_duration;
            const float velA = length( poseA.params.velocity ).getAsFloat();
            const float velB = length( poseB.params.velocity ).getAsFloat();
            value[0] = lerp( blend_alpha, velA, velB );
        }

        return true;
    }

    void MotionMatching::unprepare()
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


    float _ComputePoseCost( const bxAnim_Joint* aJoints, const bxAnim_Joint* bJoints, const float* betha, u32 numJoints )
    {
        //const float vr = lengthSqr( aJoints[0].position - bJoints[0].position ).getAsFloat() * betha[0];
        //const float q0 = lengthSqr( rotate( aJoints[0].rotation, u ) - rotate( bJoints[0].rotation, u ) ).getAsFloat() * betha[0];
        //
        //float pi = 0.f;
        ////float qpi = 0.f;
        //for( u32 i = 1; i < numJoints; ++i )
        //{
        //    const Vector3 xa = rotate( aJoints[i].rotation, u );
        //    const Vector3 xb = rotate( bJoints[i].rotation, u );
        //    pi += lengthSqr( xa - xb ).getAsFloat() * betha[i];
        //
        //    //const Quat qp_a = msa._v.joints[i].rotation * msa._x.joints[i].rotation;
        //    //const Quat qp_b = msb._v.joints[i].rotation * msb._x.joints[i].rotation;
        //    //const Vector3 va = rotate( qp_a, u );
        //    //const Vector3 vb = rotate( qp_b, u );
        //
        //    //qpi += lengthSqr( va - vb ).getAsFloat() * betha[i];
        //}
        float result = 0.f;

        for( u32 i = 0; i < numJoints; ++i )
        {
            const Vector3 a = aJoints[i].position;
            const Vector3 b = bJoints[i].position;
            result += length( a - b ).getAsFloat();

            const Quat qa = aJoints[i].rotation;
            const Quat qb = bJoints[i].rotation;
            const float d = 1.f - dot( qa, qb ).getAsFloat();
            result += d;
        }

        //const float result = ::sqrt( vr + q0 + pi );
        return ( result );
    }
    float _ComputeTrajectoryCost( const Vector3* trajectoryA, const Vector3* trajectoryB, u32 n )
    {
        float cost = 0;
        for( u32 i = 0; i < n; ++i )
        {
            const Vector3 a = trajectoryA[i];
            const Vector3 b = trajectoryB[i];

            cost += lengthSqr( a - b ).getAsFloat();
        }

        cost = ::sqrt( cost );

        for( u32 i = 1; i < n; ++i )
        {
            const Vector3 a0 = trajectoryA[i-1];
            const Vector3 a1 = trajectoryA[i];
            
            const Vector3 b0 = trajectoryB[i-1];
            const Vector3 b1 = trajectoryB[i];

            const Vector3 aV = normalizeSafe( a1 - a0 );
            const Vector3 bV = normalizeSafe( b1 - b0 );

            cost += 1.f - dot( aV, bV ).getAsFloat();
        }
        return cost;
    }
    inline float _ComputeVelocityCost( const Vector3 vA, const Vector3 vB )
    {
        floatInVec cost = length( vA - vB );
        cost += oneVec - dot( normalizeSafe( vA ), normalizeSafe( vB ) );
        return cost.getAsFloat();
    }


    void MotionMatching::tick( const Input& input, float deltaTime )
    {
        //bxAnim_Clip* curr_clip = nullptr;
        //if( _state.clip_index != UINT32_MAX )
        //{
        //    curr_clip = _data.clips[_state.clip_index];
        //}

        const Matrix4 to_local_space = inverse( input.base_matrix );
        Vector3 local_trajectory[eNUM_TRAJECTORY_POINTS];
        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        {
            local_trajectory[i] = mulAsVec4( to_local_space, input.trajectory[i] );
        }
        const Vector3 local_velocity = to_local_space.getUpper3x3() * input.velocity;
        const Vector3 local_acceleration = to_local_space.getUpper3x3() * input.acceleration;


        const u32 num_joints = _data.skel->numJoints;

        bxAnim_Joint* curr_local_joints = nullptr;

        if( _state.num_clips )
        {
            _TickAnimations( deltaTime );
            curr_local_joints = bxAnim::poseFromStack( _state.anim_ctx, 0 );
            curr_local_joints[0].position = mulPerElem( curr_local_joints[0].position, Vector3( 0.f, 1.f, 0.f ) );
        }

        if( curr_local_joints && _state.num_clips == 1)
        {
            bxAnim_Joint* tmp_joints = _state.joint_world;
            float change_cost = FLT_MAX;
            u32 change_index = UINT32_MAX;

            const bool is_current_anim_looped = _data.clip_infos[_state.clip_index[0]].is_loop == 1;

            for( size_t i = 0; i < _data.poses.size(); ++i )
            {
                const Pose pose = _data.poses[i];

                const AnimClipInfo& clip_info = _data.clip_infos[pose.params.clip_index];
                const bool is_pose_clip_looped = clip_info.is_loop == 1;
                //_ComputeDifference( tmp_joints, curr_local_joints, pose.joints, num_joints );
                const float pose_cost = _ComputePoseCost( curr_local_joints, pose.joints, _data.bone_lengths.data(), num_joints );
                const float trajectory_cost = _ComputeTrajectoryCost( local_trajectory, pose.trajectory, eNUM_TRAJECTORY_POINTS );
                const Vector3 pose_velocity = pose.params.velocity;
                const Vector3 pose_acceleration = pose.params.acceleration;

                const float velocity_cost = _ComputeVelocityCost( local_velocity, pose_velocity );
                const float acceleration_cost = length( local_acceleration - pose_acceleration ).getAsFloat();

                float basis_cost = 0.f;
                {
                    const Matrix3 world_m3 = input.base_matrix.getUpper3x3();
                    Matrix3 m3 = world_m3 * Matrix3( pose.joints[0].rotation );
                    basis_cost += 1.f - dot( m3.getCol0(), world_m3.getCol0() ).getAsFloat();
                    basis_cost += 1.f - dot( m3.getCol1(), world_m3.getCol1() ).getAsFloat();
                    basis_cost += 1.f - dot( m3.getCol2(), world_m3.getCol2() ).getAsFloat();
                }

                const float loop_cost = ( !is_current_anim_looped && !is_pose_clip_looped ) ? 10.f : 1.f;

                const float current_cost = pose_cost + basis_cost;
                const float future_cost = ( trajectory_cost + velocity_cost + acceleration_cost );
                const float cost = (current_cost + future_cost) * loop_cost;
                //const float cost = trajectory_cost;

                if( change_cost > cost )
                {
                    change_cost = cost;
                    change_index = (u32)i;
                }
            }

            const Pose& winner_pose = _data.poses[change_index];

            const bool winner_is_at_the_same_location = _state.clip_index[0] == winner_pose.params.clip_index; // &&
                //::abs( _state.clip_eval_time - winner_pose.clip_start_time ) < 0.2f;

            if( !winner_is_at_the_same_location )
            {
                //if( _state.num_clips == 1 )
                {
                    _state.clip_index[1] = winner_pose.params.clip_index;
                    _state.clip_eval_time[1] = winner_pose.params.clip_start_time;
                    _state.num_clips = 2;
                    _state._blend_duration = 0.2f;
                    _state._blend_time = 0.f;
                }
                //else
                //{
                //    
                //}
            }


            for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
            {
                bxGfxDebugDraw::addSphere( Vector4( mulAsVec4( input.base_matrix, winner_pose.trajectory[i] ), 0.03f ), 0xffffffff, 1 );
            }
        }
        else if( _state.num_clips == 0 )
        {
            float change_cost = FLT_MAX;
            u32 change_index = UINT32_MAX;
            for( size_t i = 0; i < _data.poses.size(); ++i )
            {
                const Pose pose = _data.poses[i];
                const float cost = _ComputeTrajectoryCost( input.trajectory, pose.trajectory, eNUM_TRAJECTORY_POINTS );;
                if( change_cost > cost )
                {
                    change_cost = cost;
                    change_index = (u32)i;
                }
            }

            const Pose& winner_pose = _data.poses[change_index];
            _state.clip_index[0] = winner_pose.params.clip_index;
            _state.clip_eval_time[0] = winner_pose.params.clip_start_time;
            _state.num_clips = 1;


            
        }
        
        if( curr_local_joints )
        {
            bxAnim_Joint root = toAnimJoint_noScale( input.base_matrix );
            bxAnimExt::localJointsToWorldJoints( _state.joint_world, curr_local_joints, _data.skel, root );
            const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _data.skel->offsetParentIndices );
            _DebugDrawJoints( _state.joint_world, parent_indices, num_joints, 0xffffffFF, 0.025f, 1.f );
        }
    }
    
    void MotionMatching::_TickAnimations( float deltaTime )
    {
        if( _state.num_clips == 1 )
        {
            bxAnim_Clip* clip = _data.clips[_state.clip_index[0]];
            float eval_time = _state.clip_eval_time[0];
            bxAnim_BlendLeaf leaf( clip, eval_time );
            bxAnimExt::processBlendTree( _state.anim_ctx, 0 | bxAnim::eBLEND_TREE_LEAF, nullptr, 0, &leaf, 1, _data.skel );
        }
        else
        {
            bxAnim_Clip* clipA = _data.clips[_state.clip_index[0]];
            float eval_timeA = _state.clip_eval_time[0];

            bxAnim_Clip* clipB = _data.clips[_state.clip_index[1]];
            float eval_timeB = _state.clip_eval_time[1];

            bxAnim_BlendLeaf leaves[2] =
            {
                bxAnim_BlendLeaf( clipA, eval_timeA ),
                bxAnim_BlendLeaf( clipB, eval_timeB ),
            };

            const float blend_alpha = minOfPair( 1.f, _state._blend_time / _state._blend_duration );

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
            }
        }

        /// TODO: change eval_time update while blending (coherent phase for animations)
        for( u32 i = 0; i < _state.num_clips; ++i )
        {
            bxAnim_Clip* clip = _data.clips[_state.clip_index[i]];
            float eval_time = _state.clip_eval_time[i] + deltaTime;
            _state.clip_eval_time[i] = ::fmodf( eval_time, clip->duration );
        }
    }

    //-----------------------------------------------------------------------
    void MotionMatching::_DebugDrawPose( u32 poseIndex, u32 color, const Matrix4& base )
    {
        if( poseIndex >= _data.poses.size() )
            return;

        const Pose pose = _data.poses[poseIndex];

        const bxAnim_Joint root_joint = toAnimJoint_noScale( base );
        const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _data.skel->offsetParentIndices );

        _debug.joints.resize( _data.skel->numJoints );
        bxAnimExt::localJointsToWorldJoints( _debug.joints.data(), pose.joints, _data.skel, root_joint );

        _DebugDrawJoints( _debug.joints.data(), parent_indices, _data.skel->numJoints, color, 0.04f, 1.f );

        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        {
            bxGfxDebugDraw::addSphere( Vector4( pose.trajectory[i], 0.03f ), 0x666666ff, 1 );
        }

        const bxAnim_Joint& j = _debug.joints[0];
        const Vector3 p = j.position;
        const Vector3 v = fastTransform( j.rotation, j.position, pose.params.velocity );
        const Vector3 a = fastTransform( j.rotation, j.position, pose.params.acceleration );
        bxGfxDebugDraw::addLine( p, v, 0x0000FFFF, 1 );
        bxGfxDebugDraw::addLine( p, a, 0xFFFF00FF, 1 );
    }

    //////////////////////////////////////////////////////////////////////////
    void motionMatchingCollectInput( MotionMatching::Input* input, const DynamicState& dstate )
    {
        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
            input->trajectory[i] = dstate._trajectory[i];

        input->velocity = dstate._velocity;
        input->acceleration = dstate._acceleration;
        input->base_matrix = dstate._ComputeBaseMatrix();
    }

    void DynamicState::debugDraw( u32 color )
    {
        bxGfxDebugDraw::addLine( _position, _position + _direction, 0x0000FFFF, 1 );
        bxGfxDebugDraw::addLine( _position, _position + _direction * _speed01, color, 1 );

        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        {
            bxGfxDebugDraw::addSphere( Vector4( _trajectory[i], 0.03f ), 0x666666ff, 1 );
        }

        const Matrix4 base = _ComputeBaseMatrix();
        bxGfxDebugDraw::addAxes( base );
    }

    //////////////////////////////////////////////////////////////////////////
    void AnimState::prepare( bxAnim_Skel* skel )
    {
        _skel = skel;

        const u32 numJoints = skel->numJoints;

        u32 mem_size = 0;
        //mem_size += numJoints * sizeof( *_anim.local );
        mem_size += numJoints * sizeof( *_world_joints );
        void* mem = BX_MALLOC( bxDefaultAllocator(), mem_size, 16 );
        memset( mem, 0x00, mem_size );

        bxBufferChunker chunker( mem, mem_size );
        //_anim.local = chunker.add< bxAnim_Joint >( numJoints );
        _world_joints = chunker.add< bxAnim_Joint >( numJoints );
        chunker.check();

        _ctx = bxAnim::contextInit( *skel );
    }

    void AnimState::unprepare()
    {
        bxAnim::contextDeinit( &_ctx );
        BX_FREE( bxDefaultAllocator(), _world_joints );
        *this = {};
    }

    void AnimState::tick( float deltaTime )
    {
        if( _num_clips == 1 )
        {
            bxAnimExt::processBlendTree( _ctx,
                                       0 | bxAnim::eBLEND_TREE_LEAF,
                                       nullptr, 0,
                                       _blend_leaf, 1,
                                       _skel );
        }
        else
        {
            const float blend_alpha = minOfPair( 1.f, _blend_time / _blend_duration );

            _blend_branch = bxAnim_BlendBranch(
                    0 | bxAnim::eBLEND_TREE_LEAF,
                    1 | bxAnim::eBLEND_TREE_LEAF,
                    blend_alpha
                    );

            bxAnimExt::processBlendTree( _ctx,
                                       0 | bxAnim::eBLEND_TREE_BRANCH,
                                       &_blend_branch, 1,
                                       _blend_leaf, 2,
                                       _skel );

            if( _blend_time > _blend_duration )
            {
                _blend_leaf[0] = _blend_leaf[1];
                _blend_leaf[1] = {};
                _num_clips = 1;
            }
            else
            {
                _blend_time += deltaTime;
            }
        }
        // evaluate clips
        {
            for( u32 i = 0; i < _num_clips; ++i )
            {
                bxAnim_Clip* clip = (bxAnim_Clip*)_blend_leaf[i].anim;
                _blend_leaf[i].evalTime = ::fmodf( _blend_leaf[i].evalTime + deltaTime, clip->duration );
            }
        }

        {
            bxAnim_Joint* local_joints = bxAnim::poseFromStack( _ctx, 0 );

            bxAnim_Joint root_joint = bxAnim_Joint::identity();
            local_joints[0].position = mulPerElem( local_joints[0].position, Vector3( 0.f, 1.f, 0.f ) );
            bxAnimExt::localJointsToWorldJoints( _world_joints, local_joints, _skel, root_joint );

            {
                const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _skel->offsetParentIndices );
                _DebugDrawJoints( _world_joints, parent_indices, _skel->numJoints, 0xFF0000FF, 0.05f, 1.f );
            }
        }
    }

    void AnimState::playClip( bxAnim_Clip* clip, float startTime, float blendTime )
    {
        if( _num_clips == 0 )
        {
            _blend_leaf[0] = bxAnim_BlendLeaf( clip, startTime );
            _num_clips = 1;
        }
        else
        {
            _blend_leaf[1] = bxAnim_BlendLeaf( clip, startTime );
            _num_clips = 2;
            _blend_duration = blendTime;
            _blend_time = 0.f;
        }
    }



}}////
