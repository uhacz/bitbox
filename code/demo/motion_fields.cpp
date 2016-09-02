#include "motion_fields.h"
#include <anim/anim.h>
#include <resource_manager/resource_manager.h>
#include <algorithm>
#include <xfunctional>
#include <gfx/gfx_debug_draw.h>

#include <util/common.h>

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
        _MotionStatePoseAllocate( &_v1, numJoints, allocator );

        _numJoints = numJoints;
    }
    void MotionState::deallocate( bxAllocator* allocator )
    {
        _MotionStatePoseDeallocate( &_v1, allocator );
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
        bxAnim::evaluateClip( _x.joints, clip, evalTime );
        bxAnim::evaluateClip( _v.joints, clip, evalTime + timeStep );
        bxAnim::evaluateClip( _v1.joints, clip, evalTime + timeStep*2.f );
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

        bxAnim::localJointsToWorldJoints( scratch.data(), _v1.joints, parent_indices, _numJoints, bxAnim_Joint::identity() );
        memcpy( _v1.joints, scratch.data(), skel->numJoints * sizeof( bxAnim_Joint ) );
    }

    void MotionState::_Difference()
    {
        _ComputeDifference( _v1.joints, _v1.joints, _v.joints, _numJoints );
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
        _speed = lerp( lerp_alpha, _speed, input_speed );
        
        if( input_speed > FLT_EPSILON )
        {
            const Vector3 input_dir = normalize( input_force );
            _direction = slerp( lerp_alpha, _direction, input_force );
            _direction = normalize( _direction );
        }
    }

    void DynamicState::debugDraw( u32 color )
    {
        bxGfxDebugDraw::addLine( _position, _position + _direction, 0x0000FFFF, 1 );
        bxGfxDebugDraw::addLine( _position, _position + _direction * _speed, color, 1 );

    }

}}////
