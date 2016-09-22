#pragma once

#include <util/array.h>
#include <util/curve.h>
#include <util/ring_buffer.h>
#include <anim/anim_joint_transform.h>
#include <anim/anim_struct.h>
#include <vector>
#include <string>
#include "game.h"
#include "util/queue.h"

namespace bx{
namespace anim{
    
struct CascadePlayer
{
    enum { 
        eMAX_NODES = 2,
    };

    struct Node
    {
        const bxAnim_Clip* clip = nullptr;
        
        /// for leaf
        u64 clip_user_data = 0;
        f32 clip_eval_time = 0.f;
        
        /// for branch
        u32 next = UINT32_MAX;
        f32 blend_time = 0.f;
        f32 blend_duration = 0.f;

        bool isLeaf() const { return next == UINT32_MAX; }
        bool isEmpty() const { return clip == nullptr; }
    };

    bxAnim_Context* _ctx = nullptr;
    const bxAnim_Skel* _skel = nullptr; /// weak reference
    Node _nodes[eMAX_NODES];
    u32 _root_node_index = UINT32_MAX;

    void prepare( const bxAnim_Skel* skel, bxAllocator* allcator = nullptr );
    void unprepare( bxAllocator* allocator = nullptr );

    bool play( const bxAnim_Clip* clip, float startTime, float blendTime, u64 userData, bool replaceLastIfFull );
    void tick( float deltaTime );

    bool empty() const { return _root_node_index == UINT32_MAX; }
    const bxAnim_Joint* localJoints() const { return bxAnim::poseFromStack( _ctx, 0 ); }
    bxAnim_Joint*       localJoints()       { return bxAnim::poseFromStack( _ctx, 0 ); }
    bool userData( u64* dst, u32 depth );

private:
    void _Tick_processBlendTree();
    void _Tick_updateTime( float deltaTime );

    u32 _AllocateNode();
};

struct SimplePlayer
{
    enum {
        eMAX_NODES = 1,
    };

    struct Clip
    {
        const bxAnim_Clip* clip = nullptr;
        u64 user_data = 0;
        f32 eval_time = 0.f;

        void updateTime( float deltaTime )
        {
            eval_time = ::fmodf( eval_time + deltaTime, clip->duration );
        }
        float phase() const { return eval_time / clip->duration; }
    };

    //struct PlayRequest
    //{
    //    Clip clip;
    //    f32 blend_duration = 0.f;
    //};
    
    bxAnim_Context* _ctx = nullptr;
    const bxAnim_Skel* _skel = nullptr; /// weak reference

    //PlayRequest _requests[eMAX_NODES];
    //queue_t< PlayRequest > _requests;
    Clip _clips[2];

    //RingArray< PlayRequest > _request_ring;

    f32 _blend_time = 0.f;
    f32 _blend_duration = 0.f;
    
    u32 _num_clips = 0;

    void prepare( const bxAnim_Skel* skel, bxAllocator* allcator = nullptr );
    void unprepare( bxAllocator* allocator = nullptr );

    void play( const bxAnim_Clip* clip, float startTime, float blendTime, u64 userData );
    void tick( float deltaTime );
private:
    void _Tick_processBlendTree();
    void _Tick_updateTime( float deltaTime );

public:
    bool empty() const { return _num_clips == 0; }
    const bxAnim_Joint* localJoints() const { return bxAnim::poseFromStack( _ctx, 0 ); }
    bxAnim_Joint*       localJoints()       { return bxAnim::poseFromStack( _ctx, 0 ); }
    bool userData( u64* dst, u32 depth );
    bool evalTime( f32* dst, u32 depth );


};

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

struct Pose
{
    Vector3 pos[_eMATCH_JOINT_COUNT_];
    Quat    rot[_eMATCH_JOINT_COUNT_];
    Vector3 vel[_eMATCH_JOINT_COUNT_];
    ClipTrajectory trajectory;
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
    //std::vector< ClipTrajectory > clip_trajectiories;
    std::vector< Pose > poses;
    std::vector< i16 > match_joints_indices;
    bx::Curve1D velocity_curve;
};

struct State
{
    //bxAnim_Context* anim_ctx = nullptr;
    //enum { eMAX_CLIPS = 4, };
    //struct ClipInfo
    //{
    //    u32 clip_index = UINT32_MAX;
    //    f32 eval_time = 0.f;
    //};
    //struct BlendInfo
    //{
    //    f32 duration = 0.f;
    //    f32 time = 0.f;
    //};

    //ClipInfo clip_info[eMAX_CLIPS];
    //BlendInfo blend_info[eMAX_CLIPS - 1];
    //u32 num_clips = 0;
    //
    f32 anim_delta_time_scaler = 1.f;
    u32 pose_index = UINT32_MAX;

    void* _memory_handle = nullptr;
    bxAnim_Joint* joint_world = nullptr;

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

    bxAllocator* _allocator = nullptr;

    //-------------------------------------------------------------------
    void load( const char* skelFile, const AnimClipInfo* animInfo, unsigned numAnims );
    void unload();

    void prepare( const ContextPrepareInfo& info );
    void prepare_evaluateClips();

    void unprepare();

    //-------------------------------------------------------------------
    void tick( const Input& input, float deltaTime );
    //void tick_animations( float deltaTime );
    //void tick_playClip( u32 clipIndex, float startTime, float blendTime );

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
    f32 _trajectory_integration_time{ 1.f };
    f32 _last_delta_time = 0.f;
    CharacterInput _input = {};

    Vector3 _trajectory[eNUM_TRAJECTORY_POINTS];

    //////////////////////////////////////////////////////////////////////////
    void tick( const bxInput& sysInput, const Input& input, float deltaTime );
    Matrix4 computeBaseMatrix( bool includeTilt = false ) const;
    void debugDraw( u32 color );
};
void motionMatchingCollectInput( Input* input, const DynamicState& dstate );

}}////

