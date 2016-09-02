#pragma once

#include <util/array.h>
#include <anim/anim_joint_transform.h>
#include <anim/anim_struct.h>
#include <vector>
#include "game.h"

namespace bx{
namespace motion_fields
{
    struct MotionState
    {
        struct Pose
        {
            bxAnim_Joint* joints = nullptr;
        };

        Pose _x; // pose
        Pose _v; // velocity
        Pose _v1; //
        u32 _numJoints = 0;

        bxAnim_Clip* _clip = nullptr;
        f32 _clip_eval_time = 0.f;

        bxAnim_Joint* jointsX() { return _x.joints; }
        bxAnim_Joint* jointsV() { return _v.joints; }
        bxAnim_Joint* jointsV1() { return _v1.joints; }

        const bxAnim_Joint* jointsX () const { return _x.joints; }
        const bxAnim_Joint* jointsV () const { return _v.joints; }
        const bxAnim_Joint* jointsV1() const { return _v1.joints; }
        
        u32 numJoints() const { return _numJoints; }

        void allocate( unsigned numJoints, bxAllocator* allocator );
        void deallocate( bxAllocator* allocator );

        void prepare( bxAnim_Clip* clip, float evalTime, float timeStep, const bxAnim_Skel* skel );
        void _Evaluate( bxAnim_Clip* clip, float evalTime, float timeStep );
        void _Transform( const bxAnim_Skel* skel );
        void _Difference();
        void _RemoveRootMotion();

    };

    enum { eNUM_NEIGHBOURS = 15, };
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

    struct DynamicState
    {
        Vector3 _position{ 0.f };
        Vector3 _direction{ 0.f, 0.f, 1.f };
        f32 _speed{ 0.f };

        CharacterInput _input = {};

        void tick( const bxInput& sysInput, float deltaTime );
        void debugDraw( u32 color );

    };

}}///