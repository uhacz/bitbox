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
        enum EMatchJoint : i16
        { 
            eMATCH_JOINT_HIPS = 0,
            eMATCH_JOINT_LEFT_FOOT,
            eMATCH_JOINT_RIGHT_FOOT,
            _eMATCH_JOINT_COUNT_,
        };

        struct PoseParams
        {
            //Vector3 velocity{ 0.f };
            //Vector3 acceleration{ 0.f };
            
            u32 clip_index{ UINT32_MAX };
            f32 clip_start_time{ 0.f };
            u32 __padding[2];
        };
        struct Pose
        {
            //bxAnim_Joint* joints{ nullptr };
            Vector3 pos[_eMATCH_JOINT_COUNT_];
            Vector3 vel[_eMATCH_JOINT_COUNT_];
            
            //Vector3 pos_hips{ 0.f };
            //Vector3 pos_left_foot{ 0.f };
            //Vector3 pos_right_foot{ 0.f };
            //
            //Vector3 vel_hips{ 0.f };
            //Vector3 vel_left_foot{ 0.f };
            //Vector3 vel_right_foot{ 0.f };

            Vector3 trajectory[eNUM_TRAJECTORY_POINTS];

            //bxAnim_Joint* joints_v{ nullptr };
            //bxAnim_Joint* joints_a{ nullptr };
            //Vector3* trajectory{ nullptr }; // eNUM_TRAJECTORY_POINTS length
            PoseParams params{};
        };
        
        struct AnimClipInfo
        {
            u32 is_loop{ 0 };
            
            AnimClipInfo( u32 isLoop )
                : is_loop( isLoop )
            {}
        };

        struct PosePrepareInfo
        {
            const bxAnim_Skel* skel = nullptr;
            const bxAnim_Clip* clip = nullptr;
            
            const i16* joint_indices = nullptr; // [_eMATCH_JOINT_COUNT_];
            u32 frameNo = 0;

            Vector3* trajectory_root_pos = nullptr; // array [ numFrames ]
            Vector3* trajectory_root_vel = nullptr; // array [ numFrames ]

        };

        struct Data
        {
            const bxAnim_Skel* skel = nullptr;
            std::vector< f32 > bone_lengths;
            std::vector< std::string> clip_names;
            std::vector< bxAnim_Clip* > clips;
            std::vector< AnimClipInfo > clip_infos;
            std::vector< Pose > poses;

            std::vector< i16 > match_joints_indices;

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
            u32 clip_evaluated = 0;
            
            u32 pose_index = UINT32_MAX;
            f32 last_match_time_s = 0.1f;

            void* _memory_handle = nullptr;
            bxAnim_Joint* joint_world = nullptr;

            Curve3D _trajectory_curve0;
            Curve3D _trajectory_curve1;
            
            State()
            {
                memset( clip_index, 0xff, sizeof( clip_index ) );
                memset( clip_eval_time, 0x00, sizeof( clip_eval_time ) );
            }
        }_state;
        
        struct Input
        {
            Vector3 trajectory[eNUM_TRAJECTORY_POINTS];
            Matrix4 base_matrix;
            Vector3 velocity;
            Vector3 acceleration;
            f32 speed01 = 0.f;
        };

        struct Debug
        {
            std::vector< bxAnim_Joint > joints;
            std::vector< u32 > pose_indices;
        } _debug;

        bxAllocator* _allocator = nullptr;

        //-------------------------------------------------------------------
        static void poseAllocate( Pose* pose, u32 numJoints, bxAllocator* allocator );
        static void poseFree( Pose* pose, bxAllocator* allocator );
        //static void posePrepare( Pose* pose, const bxAnim_Skel* skel, const bxAnim_Clip* clip, u32 frameNo, const AnimClipInfo& clipInfo );
        static void posePrepare( Pose* pose, const PosePrepareInfo& info );
        //-------------------------------------------------------------------
        static void stateAllocate( State* state, u32 numJoints, bxAllocator* allocator );
        static void stateFree( State* state, bxAllocator* allocator );
        
        //-------------------------------------------------------------------
        void load( const char* skelFile, const char** animFiles, const AnimClipInfo* animInfo, unsigned numAnimFiles );
        void unload();

        void prepare();
        void _EvaluateClips( float timeStep );
        void _ComputeBoneLengths();
        bool currentSpeed( f32* value );

        void unprepare();

        void tick( const Input& input, float deltaTime );
        void _TickAnimations( float deltaTime );
        //-------------------------------------------------------------------
        

        //-------------------------------------------------------------------
        void _DebugDrawPose( u32 poseIndex, u32 color, const Matrix4& base );
        void _DebugDrawState( u32 color );
    };
    //////////////////////////////////////////////////////////////////////////

    struct DynamicState
    {
        Vector3 _acceleration{ 0.f };
        Vector3 _velocity{ 0.f };
        Vector3 _position{ 0.f };

        Vector3 _prev_input_force{ 0.f };
        Vector3 _prev_direction{ 0.f, 0.f, 1.f };
        Vector3 _direction{ 0.f, 0.f, 1.f };
        f32 _mass{ 0.01f };
        f32 _speed01{ 0.f };
        f32 _prev_speed01{ 0.f };
        f32 _max_speed{ 3.f };
        f32 _trajectory_integration_time{ 1.f };
        CharacterInput _input = {};

        
        
        Vector3 _trajectory[eNUM_TRAJECTORY_POINTS];
        
        //////////////////////////////////////////////////////////////////////////
        void tick( const bxInput& sysInput, float deltaTime );
        void computeLocalTrajectory( Vector3 localTrajectory[eNUM_TRAJECTORY_POINTS], const Matrix4& base );
        Matrix4 _ComputeBaseMatrix() const;

        void debugDraw( u32 color );
    };
    void motionMatchingCollectInput( MotionMatching::Input* input, const DynamicState& dstate );


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