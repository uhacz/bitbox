#include "motion_fields.h"
#include <anim/anim.h>
#include <resource_manager/resource_manager.h>
#include <algorithm>
#include <xfunctional>
#include <gfx/gfx_debug_draw.h>

#include <util/common.h>
#include "util/buffer_utils.h"

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
        characterInputCollectData( &_input, sysInput, deltaTime );

        const Vector3 x_input_force = Vector3::xAxis() * _input.analogX;
        const Vector3 z_input_force = -Vector3::zAxis() * _input.analogY;
        const Vector3 input_force = x_input_force + z_input_force;

        const float converge_time01 = 0.01f;
        const float lerp_alpha = 1.f - ::pow( converge_time01, deltaTime );

        const float input_speed = length( normalizeSafe( input_force ) ).getAsFloat();
        _prev_speed01 = _speed01;
        _speed01 = lerp( lerp_alpha, _speed01, input_speed );
        
        _prev_direction = _direction;
        if( input_speed > FLT_EPSILON )
        {
            const Vector3 input_dir = normalize( input_force );
            _direction = slerp( lerp_alpha, _direction, input_force );
            _direction = normalizeSafe( _direction );
        }

        const float prev_spd = _max_speed * _prev_speed01;
        const float spd = _max_speed * _speed01;

        Vector3 acceleration = _direction*spd - _prev_direction*prev_spd;
        _velocity = _direction * _speed01;

        float time = 0.f;
        const float dt = 5.f / (eNUM_TRAJECTORY_POINTS-1);
        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        {
            /// s = s0 + v0 * t + a*t2*0.5f
            _trajectory[i] = _position + _velocity * time + acceleration*( time*time ) * 0.5f;
            time += dt;
        }


    }

    void DynamicState::debugDraw( u32 color )
    {
        bxGfxDebugDraw::addLine( _position, _position + _direction, 0x0000FFFF, 1 );
        bxGfxDebugDraw::addLine( _position, _position + _direction * _speed01, color, 1 );

        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        {
            bxGfxDebugDraw::addSphere( Vector4( _trajectory[i], 0.03f ), 0x666666ff, 1 );
        }
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
