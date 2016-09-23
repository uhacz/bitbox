#pragma once

#include <util/type.h>

struct bxAllocator;

struct bxAnim_Joint;
struct bxAnim_Skel;
struct bxAnim_Clip;
struct bxAnim_Context;

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
    Node _nodes[eMAX_NODES];
    u32 _root_node_index = UINT32_MAX;

    void prepare( const bxAnim_Skel* skel, bxAllocator* allcator = nullptr );
    void unprepare( bxAllocator* allocator = nullptr );

    bool play( const bxAnim_Clip* clip, float startTime, float blendTime, u64 userData, bool replaceLastIfFull );
    void tick( float deltaTime );

    bool empty() const { return _root_node_index == UINT32_MAX; }
    const bxAnim_Joint* localJoints() const;
    bxAnim_Joint*       localJoints();
    bool userData( u64* dst, u32 depth );

private:
    void _Tick_processBlendTree();
    void _Tick_updateTime( float deltaTime );

    u32 _AllocateNode();
};

//////////////////////////////////////////////////////////////////////////
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
    };

    bxAnim_Context* _ctx = nullptr;
    bxAnim_Joint* _prev_joints = nullptr;
    Clip _clips[2];
    f32 _blend_time = 0.f;
    f32 _blend_duration = 0.f;
    u32 _num_clips = 0;

    void prepare( const bxAnim_Skel* skel, bxAllocator* allcator = nullptr );
    void unprepare( bxAllocator* allocator = nullptr );

    void play( const bxAnim_Clip* clip, float startTime, float blendTime, u64 userData );
    void tick( float deltaTime );
private:
    static void _ClipUpdateTime( Clip* clip, float deltaTime );
    static float _ClipPhase( const Clip& clip );
    void _Tick_processBlendTree();
    void _Tick_updateTime( float deltaTime );
    

public:
    bool empty() const { return _num_clips == 0; }
    const bxAnim_Joint* localJoints() const;
    bxAnim_Joint*       localJoints();

    const bxAnim_Joint* prevLocalJoints() const;
    bxAnim_Joint*       prevLocalJoints();

    bool userData( u64* dst, u32 depth );
    bool evalTime( f32* dst, u32 depth );
};

}}///
