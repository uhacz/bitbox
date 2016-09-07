#pragma once

#include <util/array.h>
#include <anim/anim_joint_transform.h>
#include <anim/anim_struct.h>
#include <vector>
#include "game.h"
#include "util/curve.h"

namespace bx{
namespace motion_fields
{
    enum { eNUM_NEIGHBOURS = 15, };
    enum { eNUM_TRAJECTORY_POINTS = 4, };

    struct MotionState
    {
        struct Pose
        {
            bxAnim_Joint* joints = nullptr;
        };

        Pose _x; // pose
        Pose _v; // velocity
        Pose _a; //
        u32 _numJoints = 0;

        Vector3 _trajectory[eNUM_TRAJECTORY_POINTS];

        bxAnim_Clip* _clip = nullptr;
        f32 _clip_eval_time = 0.f;

        bxAnim_Joint* jointsX() { return _x.joints; }
        bxAnim_Joint* jointsV() { return _v.joints; }
        bxAnim_Joint* jointsV1() { return _a.joints; }

        const bxAnim_Joint* jointsX () const { return _x.joints; }
        const bxAnim_Joint* jointsV () const { return _v.joints; }
        const bxAnim_Joint* jointsV1() const { return _a.joints; }
        
        u32 numJoints() const { return _numJoints; }

        void allocate( unsigned numJoints, bxAllocator* allocator );
        void deallocate( bxAllocator* allocator );

        void prepare( bxAnim_Clip* clip, float evalTime, float timeStep, const bxAnim_Skel* skel );
        void _Evaluate( bxAnim_Clip* clip, float evalTime, float timeStep );
        void _Transform( const bxAnim_Skel* skel );
        void _Difference();
        void _RemoveRootMotion();
    };

    
    struct NeighbourIndices
    {
        u32 i[eNUM_NEIGHBOURS];
    };
    struct NeighbourValue
    {
        f32 v[eNUM_NEIGHBOURS];
    };

    struct Data
    {
        bxAnim_Skel* _skel;
        std::vector< bxAnim_Clip* > _clips;
        std::vector< MotionState > _states;
        std::vector< NeighbourIndices > _neighbours;
        std::vector< NeighbourValue > _neighbour_similarity;
        std::vector< NeighbourValue > _neighbour_weight;
        std::vector< bxAnim_Joint > _debug_joints;

        bxAllocator* _allocator = nullptr;
        
        void load( const char* skelFile, const char** animFiles, unsigned numAnimFiles );
        void unload();

        void prepare();
        void unprepare();

        void debugDrawState( unsigned index, u32 color, bool drawNeighbours );
        u32 findState( const Vector3 desiredDirection, float desiredSpeed );
        u32 findNextState( u32 currentStateIndex, const Vector3 desiredDirection, float desiredSpeed );
        u32 getMostSimilarState( u32 currentStateIndex );

        void _EvaluateClips( float timeStep );
        void _ComputeNeighbourhood();
    };


    //////////////////////////////////////////////////////////////////////////
    struct MotionMatching
    {
        struct Pose
        {
            bxAnim_Joint* joints{ nullptr };
            bxAnim_Joint* joints_v{ nullptr };
            Vector3* trajectory{ nullptr }; // eNUM_TRAJECTORY_POINTS length
            //const bxAnim_Clip* clip{ nullptr };

            u32 clip_index{ UINT32_MAX };
            f32 clip_start_time{ 0.f };
        };
        
        struct Data
        {
            const bxAnim_Skel* skel = nullptr;
            std::vector< f32 > bone_lengths;
            std::vector< bxAnim_Clip* > clips;
            std::vector< Pose > poses;

            bx::Curve1D velocity_curve;

        } _data;

        struct State
        {
            bxAnim_Context* anim_ctx = nullptr;
            
            f32 _blend_duration = 0.f;
            f32 _blend_time = 0.f;

            enum { eMAX_CLIPS = 2, };
            u32 clip_index[eMAX_CLIPS]; // = { UINT32_MAX };
            f32 clip_eval_time[eMAX_CLIPS]; // = { 0.f };
            u32 num_clips = 0;
            
            void* _memory_handle = nullptr;
            bxAnim_Joint* joint_world = nullptr;

            State()
            {
                memset( clip_index, 0xff, sizeof( clip_index ) );
                memset( clip_eval_time, 0x00, sizeof( clip_eval_time ) );
            }
        }_state;
        
        struct Input
        {
            Vector3 trajectory[eNUM_TRAJECTORY_POINTS]; // local space
            Matrix4 base_matrix;
            Vector3 velocity; // local space

        };

        struct Debug
        {
            std::vector< bxAnim_Joint > joints;
        } _debug;

        bxAllocator* _allocator = nullptr;

        //-------------------------------------------------------------------
        static void poseAllocate( Pose* pose, u32 numJoints, bxAllocator* allocator );
        static void poseFree( Pose* pose, bxAllocator* allocator );
        static void posePrepare( Pose* pose, const bxAnim_Skel* skel, const bxAnim_Clip* clip, float evalTime );
        //-------------------------------------------------------------------
        static void stateAllocate( State* state, u32 numJoints, bxAllocator* allocator );
        static void stateFree( State* state, bxAllocator* allocator );

        //-------------------------------------------------------------------
        void load( const char* skelFile, const char** animFiles, unsigned numAnimFiles );
        void unload();

        void prepare();
        void _EvaluateClips( float timeStep );
        void _ComputeBoneLengths();

        void unprepare();

        void tick( const Input& input, float deltaTime );
        //-------------------------------------------------------------------
        

        //-------------------------------------------------------------------
        void _DebugDrawPose( u32 poseIndex, u32 color, const Matrix4& base );
        void _DebugDrawState( u32 color );
    };
    //////////////////////////////////////////////////////////////////////////

    struct DynamicState
    {
        Vector3 _velocity{ 0.f };
        Vector3 _position{ 0.f };

        Vector3 _prev_direction{ 0.f, 0.f, 1.f };
        Vector3 _direction{ 0.f, 0.f, 1.f };
        f32 _speed01{ 0.f };
        f32 _prev_speed01{ 0.f };
        f32 _max_speed{ 3.f };
        CharacterInput _input = {};

        
        Vector3 _trajectory[eNUM_TRAJECTORY_POINTS];
        
        //////////////////////////////////////////////////////////////////////////
        void tick( const bxInput& sysInput, float deltaTime );
        void computeLocalTrajectory( Vector3 localTrajectory[eNUM_TRAJECTORY_POINTS], const Matrix4& base );
        Matrix4 _ComputeBaseMatrix();

        void debugDraw( u32 color );
    };

    struct AnimState
    {
        bxAnim_Context* _ctx = nullptr;
        bxAnim_Skel* _skel = nullptr; // weak reference to Data::_skel
        u32 _num_clips = 0;
        f32 _eval_time = 0.f;

        bxAnim_BlendBranch _blend_branch {};
        bxAnim_BlendLeaf _blend_leaf[2] = {};
        f32 _blend_duration = 0.f;
        f32 _blend_time = 0.f;

        bxAnim_Joint* _world_joints = nullptr;

        //////////////////////////////////////////////////////////////////////////
        void prepare( bxAnim_Skel* skel );
        void unprepare();
        
        void tick( float deltaTime );
        void playClip( bxAnim_Clip* clip, float startTime, float blendTime );
    };


}}///