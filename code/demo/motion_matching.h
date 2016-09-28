#pragma once

#include <util/array.h>
#include <util/curve.h>
#include <util/ring_buffer.h>
#include <anim/anim_joint_transform.h>
#include <anim/anim_struct.h>
#include <anim/anim_player.h>

#include <vector>
#include <string>

#include "game.h"

namespace bx{
namespace anim{

struct IKNode3
{
    i16 idx_begin = -1;
    i16 idx_middle = -1;
    i16 idx_end = -1;
    i16 idx_padding__ = -1;

    float len_begin_2_mid = 0.f;
    float len_mid_2_end = 0.f;
    float chain_length = 0.f;
};

void ikNodeInit( IKNode3* node, const bxAnim_Skel* skel, const char* jointNames[3] );
void ikNodeSolve( bxAnim_Joint* localJoints, const IKNode3& node, const Matrix4* animMatrices, const i16* parentIndices, const Vector3& goalPosition, float strength, bool debugDraw = false );

}}///

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

struct ClipTrajectory
{
    Vector3 pos[eNUM_TRAJECTORY_POINTS];
    Vector3 vel[eNUM_TRAJECTORY_POINTS - 1];
    Vector3 acc[eNUM_TRAJECTORY_POINTS - 2];
};

struct BIT_ALIGNMENT_16 Pose
{
    Vector3 pos[_eMATCH_JOINT_COUNT_];
    Vector3 vel[_eMATCH_JOINT_COUNT_];
    Vector3 local_velocity{ 0.f };
    ClipTrajectory trajectory;
    u32 flags[_eMATCH_JOINT_COUNT_];
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
struct AnimFootPlaceInfo
{
    Curve1D foot_curve;
};

struct PosePrepareInfo
{
    const bxAnim_Skel* skel = nullptr;
    const bxAnim_Clip* clip = nullptr;

    const i16* joint_indices = nullptr; // [_eMATCH_JOINT_COUNT_];
    u32 frameNo = 0;
    bool is_loop = false;
};

struct Data
{
    const bxAnim_Skel* skel = nullptr;
    std::vector< bxAnim_Clip* > clips;
    std::vector< AnimClipInfo > clip_infos;
    std::vector< AnimFootPlaceInfo > clip_foot_place_info;
    //std::vector< ClipTrajectory > clip_trajectiories;
    std::vector< Pose > poses;
    std::vector< i16 > match_joints_indices;
    bx::Curve1D velocity_curve;
};

struct State
{
    f32 anim_delta_time_scaler = 1.f;
    u32 pose_index = UINT32_MAX;

    void* _memory_handle = nullptr;
    bxAnim_Joint* joint_world = nullptr;
    bxAnim_Joint* scratch_joints = nullptr;
    Matrix4* matrix_world = nullptr;

    Curve3D input_trajectory_curve;
    Curve3D candidate_trajectory_curve;

    State();
};

struct Input
{
    Vector3 trajectory[eNUM_TRAJECTORY_POINTS];
    
    Matrix4 base_matrix;
    Matrix4 base_matrix_aligned;
    
    Vector3 velocity;
    Vector3 acceleration;
    Vector3 raw_input;

    f32 speed01;
    f32 trajectory_integration_time;
};
struct Output
{
    bxAnim_Clip* clip = nullptr;
    f32 start_time = 0.f;
    f32 blend_time = 0.f;
};

struct Debug
{
    std::vector< bxAnim_Joint > joints0;
    std::vector< bxAnim_Joint > joints1;
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
void computeClipTrajectory( ClipTrajectory* ct, const bxAnim_Clip* clip, float trajectoryStartTime, float trajectoryDuration );

struct FootPlace
{
    Vector3 _world_pos{ 0.f };
    u32 _flag = 0;
    void place( const Vector3 candidateWorldPos, u32 onGroundFlag, float speed, float glueRC, float deltaTime );
    bool isOnGround() const { return _flag != 0; }
};
    
struct ContextPrepareInfo
{
    const char* matching_joint_names[_eMATCH_JOINT_COUNT_];
};

struct Context
{
    Data _data;
    State _state;
    Debug _debug;
    anim::SimplePlayer _anim_player;
    
    anim::IKNode3 _left_foot_ik = {};
    anim::IKNode3 _right_foot_ik = {};
    f32 _left_foot_ik_strength = 0.f;
    f32 _right_foot_ik_strength = 0.f;

    FootPlace _left_foot_place = {};
    FootPlace _right_foot_place = {};

    bxAllocator* _allocator = nullptr;

    //-------------------------------------------------------------------
    void load( const char* skelFile, const AnimClipInfo* animInfo, unsigned numAnims );
    void unload();

    void prepare( const ContextPrepareInfo& info );
    void prepare_evaluateClips();

    void unprepare();

    //-------------------------------------------------------------------
    void tick( const Input& input, float deltaTime );
    void tick_updateFootPlace( FootPlace* fp, EMatchJoint joint, const Matrix4 baseMatrix, float glueRC, float deltaTime );

    bool currentSpeed( f32* value );
    bool currentPose( Matrix4* pose );
    //-------------------------------------------------------------------
    void _DebugDrawPose( u32 poseIndex, u32 color, const Matrix4& base );
};
        
}}///


namespace bx{
namespace motion_matching{

struct DynamicState
{
    struct Input
    {
        Matrix4 anim_pose = Matrix4::identity();
        float anim_velocity = -1.f;
        u32 data_valid = 0;
    };

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
    f32 _trajectory_integration_time{ 1.0f };
    f32 _last_delta_time = 0.f;
    CharacterInput _input = {};

    Vector3 _trajectory_pos[eNUM_TRAJECTORY_POINTS];

    //////////////////////////////////////////////////////////////////////////
    void tick( const bxInput& sysInput, const Input& input, float deltaTime );
    Matrix4 computeBaseMatrix( bool includeTilt = false ) const;
    void debugDraw( u32 color );
};
void motionMatchingCollectInput( Input* input, const DynamicState& dstate );
}}////



