#pragma once

#include <util/array.h>
#include <util/curve.h>
#include <anim/anim_joint_transform.h>
#include <anim/anim_struct.h>
#include <vector>
#include <string>
#include "game.h"

namespace bx{
namespace motion_matching{

enum { eNUM_TRAJECTORY_POINTS = 4, };
enum EMatchJoint : i16
{
    eMATCH_JOINT_HIPS = 0,
    eMATCH_JOINT_LEFT_FOOT,
    eMATCH_JOINT_RIGHT_FOOT,
    _eMATCH_JOINT_COUNT_,
};

struct PoseParams
{
    u32 clip_index{ UINT32_MAX };
    f32 clip_start_time{ 0.f };
    u32 __padding[2];
};
struct Pose
{
    Vector3 pos[_eMATCH_JOINT_COUNT_];
    Vector3 vel[_eMATCH_JOINT_COUNT_];
    PoseParams params{};
};
    
    
struct AnimClipInfo
{
    std::string name;
    u32 is_loop{ 0 };

    AnimClipInfo( const char* n, u32 isLoop )
        : name( n ), is_loop( isLoop )
    {}
};
struct ClipTrajectory
{
    Vector3 pos[eNUM_TRAJECTORY_POINTS];
    Vector3 vel[eNUM_TRAJECTORY_POINTS - 1];
    Vector3 acc[eNUM_TRAJECTORY_POINTS - 2];
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
    std::vector< bxAnim_Clip* > clips;
    std::vector< AnimClipInfo > clip_infos;
    std::vector< ClipTrajectory > clip_trajectiories;
    std::vector< Pose > poses;
    std::vector< i16 > match_joints_indices;
    bx::Curve1D velocity_curve;
};

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

    void* _memory_handle = nullptr;
    bxAnim_Joint* joint_world = nullptr;

    State();
};

struct Input
{
    Vector3 trajectory[eNUM_TRAJECTORY_POINTS];
    Matrix4 base_matrix;
    Vector3 velocity;
    Vector3 acceleration;
    f32 speed01;
};

struct Debug
{
    std::vector< bxAnim_Joint > joints;
    std::vector< u32 > pose_indices;
};

//-------------------------------------------------------------------
void poseAllocate( Pose* pose, u32 numJoints, bxAllocator* allocator );
void poseFree( Pose* pose, bxAllocator* allocator );
void posePrepare( Pose* pose, const PosePrepareInfo& info );
//-------------------------------------------------------------------
void stateAllocate( State* state, u32 numJoints, bxAllocator* allocator );
void stateFree( State* state, bxAllocator* allocator );
//-------------------------------------------------------------------
void computeClipTrajectory( ClipTrajectory* ct, const bxAnim_Clip* clip, float trajectoryDuration );
    
    
struct ContextPrepareInfo
{
    const char* matching_joint_names[_eMATCH_JOINT_COUNT_];
};

struct Context
{
    Data _data;
    State _state;
    Debug _debug;

    bxAllocator* _allocator = nullptr;

    //-------------------------------------------------------------------
    void load( const char* skelFile, const AnimClipInfo* animInfo, unsigned numAnims );
    void unload();

    void prepare( const ContextPrepareInfo& info );
    void prepare_evaluateClips();

    void unprepare();

    //-------------------------------------------------------------------
    void tick( const Input& input, float deltaTime );
    void tick_animations( float deltaTime );
    void tick_playClip( u32 clipIndex, float startTime, float blendTime );

    bool currentSpeed( f32* value );

    //-------------------------------------------------------------------
    void _DebugDrawPose( u32 poseIndex, u32 color, const Matrix4& base );
};
        
}}///

namespace bx{
namespace motion_matching{

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
    Matrix4 computeBaseMatrix() const;
    void debugDraw( u32 color );
};
void motionMatchingCollectInput( Input* input, const DynamicState& dstate );

}}////

