#pragma once

#include <util/array.h>
#include <anim/anim_joint_transform.h>
#include <anim/anim_struct.h>
#include <vector>

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

        void allocate( unsigned numJoints, bxAllocator* allocator );
        void deallocate( bxAllocator* allocator );

        void prepare( bxAnim_Clip* clip, float evalTime, float timeStep, const bxAnim_Skel* skel );
        void _Evaluate( bxAnim_Clip* clip, float evalTime, float timeStep );
        void _Transform( const bxAnim_Skel* skel );
        void _Difference();

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

        void _EvaluateClips( float timeStep );
        void _ComputeNeighbourhood();

    };

}}///