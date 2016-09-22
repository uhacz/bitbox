#include "motion_matching.h"
#include <anim/anim.h>
#include <resource_manager/resource_manager.h>
#include <algorithm>
#include <xfunctional>
#include <gfx/gfx_debug_draw.h>

#include <util/common.h>
#include <util/buffer_utils.h>
#include "util/signal_filter.h"

namespace bx{
namespace anim{

void CascadePlayer::prepare( const bxAnim_Skel* skel, bxAllocator* allcator /*= nullptr */ )
{
    _skel = skel;
    _ctx = bxAnim::contextInit( *skel );

}

void CascadePlayer::unprepare( bxAllocator* allocator /*= nullptr */ )
{
    bxAnim::contextDeinit( &_ctx );
    _skel = nullptr;
}

namespace
{
    static void makeLeaf( CascadePlayer::Node* node, const bxAnim_Clip* clip, float startTime, u64 userData )
    {
        *node = {};
        node->clip = clip;
        node->clip_eval_time = startTime;
        node->clip_user_data = userData;
    }
    static void makeBranch( CascadePlayer::Node* node, u32 nextNodeIndex, float blendDuration )
    {
        node->next = nextNodeIndex;
        node->blend_time = 0.f;
        node->blend_duration = blendDuration;
    }
}

bool CascadePlayer::play( const bxAnim_Clip* clip, float startTime, float blendDuration, u64 userData, bool replaceLastIfFull )
{
    u32 node_index = _AllocateNode();
    if( node_index == UINT32_MAX )
    {
        if( replaceLastIfFull )
        {
            node_index = _root_node_index;
            while( _nodes[node_index].next != UINT32_MAX )
                node_index = _nodes[node_index].next;
        }
        else
        {
            bxLogError( "Cannot allocate node" );
            return false;
        }
    }

    if( _root_node_index == UINT32_MAX )
    {
        _root_node_index = node_index;
        _nodes[node_index] = {};

        Node& node = _nodes[node_index];
        makeLeaf( &node, clip, startTime, userData );
    }
    else
    {
        u32 last_node = _root_node_index;
        if( _nodes[last_node].next != UINT32_MAX )
        {
            last_node = _nodes[last_node].next;
        }

        if( last_node == node_index ) // when replaceLastIfFull == true and there is no space for new nodes
        {
            Node& node = _nodes[node_index];
            makeLeaf( &node, clip, startTime, userData );
        }
        else
        {
            Node& prev_node = _nodes[last_node];
            makeBranch( &prev_node, node_index, blendDuration );
        
            Node& node = _nodes[node_index];
            makeLeaf( &node, clip, startTime, userData );
        }
    }
    return true;
}

void CascadePlayer::tick( float deltaTime )
{
    if( _root_node_index == UINT32_MAX ) 
    {
        // no animations to play
        return;
    }

    _Tick_processBlendTree();
    _Tick_updateTime( deltaTime );    
}

bool CascadePlayer::userData( u64* dst, u32 depth )
{
    u32 index = _root_node_index;
    while( index != UINT32_MAX )
    {
        const Node& node = _nodes[index];
        
        if( depth == 0 )
        {
            dst[0] = node.clip_user_data;
            return true;
        }
        depth -= 1;
        index = node.next;
    }

    return false;
}

void CascadePlayer::_Tick_processBlendTree()
{
    if( _nodes[_root_node_index].isLeaf() )
    {
        const Node& node = _nodes[_root_node_index];
        bxAnim_BlendLeaf leaf( node.clip, node.clip_eval_time );

        bxAnimExt::processBlendTree( _ctx,
                                     0 | bxAnim::eBLEND_TREE_LEAF,
                                     nullptr, 0,
                                     &leaf, 1,
                                     _skel );
    }
    else
    {
        bxAnim_BlendLeaf leaves[eMAX_NODES] = {};
        bxAnim_BlendBranch branches[eMAX_NODES] = {};

        u32 num_leaves = 0;
        u32 num_branches = 0;

        u32 node_index = _root_node_index;
        while( node_index != UINT32_MAX )
        {
            SYS_ASSERT( num_leaves < eMAX_NODES );
            SYS_ASSERT( num_branches < eMAX_NODES );

            const Node& node = _nodes[node_index];

            const u32 leaf_index = num_leaves++;
            bxAnim_BlendLeaf* leaf = &leaves[leaf_index];
            leaf[0] = bxAnim_BlendLeaf( node.clip, node.clip_eval_time );

            if( node.isLeaf() )
            {
                SYS_ASSERT( node.next == UINT32_MAX );
                SYS_ASSERT( num_branches > 0 );

                const u32 last_branch = num_branches - 1;
                bxAnim_BlendBranch* branch = &branches[last_branch];

                branch->right = leaf_index | bxAnim::eBLEND_TREE_LEAF;
            }
            else
            {
                bxAnim_BlendBranch* last_branch = nullptr;
                if( node_index != _root_node_index )
                {
                    const u32 last_branch_index = num_branches - 1;
                    last_branch = &branches[last_branch_index];
                    
                }

                const u32 branch_index = num_branches++;
                if( last_branch )
                {
                    last_branch->right = branch_index | bxAnim::eBLEND_TREE_BRANCH;
                }

                const float blend_alpha = minOfPair( 1.f, node.blend_time / node.blend_duration );
                bxAnim_BlendBranch* branch = &branches[branch_index];
                branch[0] = bxAnim_BlendBranch( leaf_index | bxAnim::eBLEND_TREE_LEAF, 0, blend_alpha );
            }

            node_index = node.next;
        }

        bxAnimExt::processBlendTree( _ctx,
                                     0 | bxAnim::eBLEND_TREE_BRANCH,
                                     branches, num_branches,
                                     leaves, num_leaves,
                                     _skel );


    }
}

namespace
{
    static inline void updateNodeClip( CascadePlayer::Node* node, float deltaTime )
    {
        node->clip_eval_time = ::fmodf( node->clip_eval_time + deltaTime, node->clip->duration );
    }
}

void CascadePlayer::_Tick_updateTime( float deltaTime )
{
    
    
    
    if( _root_node_index != UINT32_MAX && _nodes[_root_node_index].isLeaf() )
    {
        Node* node = &_nodes[_root_node_index];
        updateNodeClip( node, deltaTime );
    }
    else
    {
        const u32 node_index = _root_node_index;
        Node* node = &_nodes[node_index];
        
        const u32 next_node_index = node->next;
        Node* next_node = &_nodes[next_node_index];

        if( node->blend_time > node->blend_duration )
        {
            node[0] = *next_node;
            next_node[0] = {};
            updateNodeClip( node, deltaTime );
        }
        else
        {
            
            if( next_node->isLeaf() )
            {
                const bxAnim_Clip* clipA = node->clip;
                const bxAnim_Clip* clipB = next_node->clip;
                
                const float blend_alpha = minOfPair( 1.f, node->blend_time / node->blend_duration );
                const float clip_duration = lerp( blend_alpha, clipA->duration, clipB->duration );
                const float delta_phase = deltaTime / clip_duration;
                
                float phaseA = node->clip_eval_time / clipA->duration; //::fmodf( ( ) + delta_phase, 1.f );
                float phaseB = next_node->clip_eval_time / clipB->duration; //::fmodf( ( ) + delta_phase, 1.f );
                phaseA = ::fmodf( phaseA + delta_phase, 1.f );
                phaseB = ::fmodf( phaseB + delta_phase, 1.f );
                
                node->clip_eval_time = phaseA * clipA->duration;
                next_node->clip_eval_time = phaseB * clipB->duration;
            }
            else
            {
                updateNodeClip( node, deltaTime );
                //updateNodeClip( next_node, deltaTime );
                
                //u32 next_next_node_index = next_node->next;
                //while( next_next_node_index != UINT32_MAX )
                //{
                //    Node* next_next_node = &_nodes[next_next_node_index];
                //    updateNodeClip( next_next_node, deltaTime );

                //    next_next_node_index = next_next_node->next;
                //}
            }

            node->blend_time += deltaTime;
        }

        //while( node_index != UINT32_MAX )
        //{
        //    Node* node = &_nodes[node_index];

        //    node->clip_eval_time = ::fmodf( node->clip_eval_time + deltaTime, node->clip->duration );
        //    if( !node->isLeaf() )
        //    {
        //        if( node->blend_time > node->blend_duration )
        //        {
        //            Node* next_node = &_nodes[node->next];

        //            node[0] = *next_node;
        //            next_node[0] = {};

        //            //node->clip_eval_time = ::fmodf( node->clip_eval_time + deltaTime, node->clip->duration );
        //        }
        //        else
        //        {
        //            node->blend_time += deltaTime;
        //            //Node* next_node = &_nodes[node->next];
        //            //if( next_node->isLeaf() )
        //            //{
        //            //    const bxAnim_Clip* clipA = node->clip;
        //            //    const bxAnim_Clip* clipB = next_node->clip;
        //            //    
        //            //    const float blend_alpha = minOfPair( 1.f, node->blend_time / node->blend_duration );
        //            //    const float clip_duration = lerp( blend_alpha, clipA->duration, clipB->duration );
        //            //    const float delta_phase = deltaTime / clip_duration;
        //            //    
        //            //    float phaseA = node->clip_eval_time / clipA->duration; //::fmodf( ( ) + delta_phase, 1.f );
        //            //    float phaseB = next_node->clip_eval_time / clipB->duration; //::fmodf( ( ) + delta_phase, 1.f );
        //            //    phaseA = ::fmodf( phaseA + delta_phase, 1.f );
        //            //    phaseB = ::fmodf( phaseB + delta_phase, 1.f );
        //            //    
        //            //    node->clip_eval_time = phaseA * clipA->duration;
        //            //    next_node->clip_eval_time = phaseB * clipB->duration;
        //            //}
        //            //else
        //            //{
        //            //    node->clip_eval_time = ::fmodf( node->clip_eval_time + deltaTime, node->clip->duration );
        //            //}
        //        }
        //    }
        //    node_index = node->next;
        //}
    }
}

u32 CascadePlayer::_AllocateNode()
{
    u32 index = UINT32_MAX;
    for( u32 i = 0; i < eMAX_NODES; ++i )
    {
        if( _nodes[i].isEmpty() )
        {
            index = i;
            break;
        }
    }
    return index;
}


//////////////////////////////////////////////////////////////////////////

void SimplePlayer::prepare( const bxAnim_Skel* skel, bxAllocator* allcator /*= nullptr */ )
{
    _skel = skel;
    _ctx = bxAnim::contextInit( *skel );
}


void SimplePlayer::unprepare( bxAllocator* allocator /*= nullptr */ )
{
    bxAnim::contextDeinit( &_ctx );
    _skel = nullptr;
}

void SimplePlayer::play( const bxAnim_Clip* clip, float startTime, float blendTime, u64 userData )
{
    if( _num_clips == 2 )
        return;

    if( _num_clips == 0 )
    {
        Clip& c0 = _clips[0];
        c0.clip = clip;
        c0.eval_time = startTime;
        c0.user_data = userData;
        _num_clips = 1;
    }
    else
    {
        Clip& c1 = _clips[1];
        c1.clip = clip;
        c1.eval_time = startTime;
        c1.user_data = userData;
        _num_clips = 2;
    }

    _blend_time = 0.f;
    _blend_duration = blendTime;

}

void SimplePlayer::tick( float deltaTime )
{
    _Tick_processBlendTree();
    _Tick_updateTime( deltaTime );
}

void SimplePlayer::_Tick_processBlendTree()
{
    if( _num_clips == 0 )
    {
        return;
    }
    else if( _num_clips == 1 )
    {
        const Clip& c = _clips[0];

        bxAnim_BlendLeaf leaf( c.clip, c.eval_time );
        bxAnimExt::processBlendTree( _ctx,
                                     0 | bxAnim::eBLEND_TREE_LEAF,
                                     nullptr, 0,
                                     &leaf, 1,
                                     _skel );
    }
    else
    {
        const Clip& c0 = _clips[0];
        const Clip& c1 = _clips[1];

        bxAnim_BlendLeaf leaves[2] =
        {
            { c0.clip, c0.eval_time },
            { c1.clip, c1.eval_time },
        };

        const float blend_alpha = minOfPair( 1.f, _blend_time / _blend_duration );
        bxAnim_BlendBranch branch( 0 | bxAnim::eBLEND_TREE_LEAF, 1 | bxAnim::eBLEND_TREE_LEAF, blend_alpha );

        bxAnimExt::processBlendTree( _ctx,
                                     0 | bxAnim::eBLEND_TREE_BRANCH,
                                     &branch, 1,
                                     leaves, 2,
                                     _skel );
    }
}

void SimplePlayer::_Tick_updateTime( float deltaTime )
{
    if( _num_clips == 0 )
    {
 
    }
    else if( _num_clips == 1 )
    {
        _clips[0].updateTime( deltaTime );
    }
    else
    {
        Clip& c0 = _clips[0];
        Clip& c1 = _clips[1];
        //c0.updateTime( deltaTime );
        //c1.updateTime( deltaTime );

        const bxAnim_Clip* clipA = c0.clip;
        const bxAnim_Clip* clipB = c1.clip;

        const float blend_alpha = minOfPair( 1.f, _blend_time / _blend_duration );
        const float clip_duration = lerp( blend_alpha, clipA->duration, clipB->duration );
        const float delta_phase = deltaTime / clip_duration;

        float phaseA = c0.phase();
        float phaseB = c1.phase();
        phaseA = ::fmodf( phaseA + delta_phase, 1.f );
        phaseB = ::fmodf( phaseB + delta_phase, 1.f );

        c0.eval_time = phaseA * clipA->duration;
        c1.eval_time = phaseB * clipB->duration;

        if( _blend_time > _blend_duration )
        {
            _clips[0] = _clips[1];
            _clips[1] = {};
            _num_clips = 1;
        }
        else
        {
            _blend_time += deltaTime;
        }
    }
}

bool SimplePlayer::userData( u64* dst, u32 depth )
{
    if( _num_clips == 0 )
        return false;

    dst[0] = _clips[0].user_data;
    return true;
}

bool SimplePlayer::evalTime( f32* dst, u32 depth )
{
    if( _num_clips == 0 )
    {
        return false;
    }

    dst[0] = _clips[0].eval_time;
    return true;
}

}}////


namespace bx{
namespace motion_matching{

namespace utils
{
    void debugDrawJoints( const bxAnim_Joint* joints, const u16* parentIndices, u32 numJoints, u32 color, float radius, float scale )
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

    void computeTrajectoryDerivatives( Vector3 v[eNUM_TRAJECTORY_POINTS - 1], Vector3 a[eNUM_TRAJECTORY_POINTS - 2], const Vector3 x[eNUM_TRAJECTORY_POINTS], float deltaTimeInv )
    {
        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS - 1; ++i )
            v[i] = ( x[i + 1] - x[i] ) * deltaTimeInv;
        for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS - 2; ++i )
            a[i] = ( v[i + 1] - v[i] ) * deltaTimeInv;
    }

    float computePositionCost( const Vector3* pos0, const Vector3* pos1, u32 numPos )
    {
        floatInVec result( 0 );
        for( u32 i = 0; i < numPos; ++i )
        {
            result += length( pos0[i] - pos1[i] );
        }

        return ( result.getAsFloat() );
    }
    float computeDirectionCost( const Vector3* dir0, const Vector3* dir1, u32 n )
    {
        floatInVec result( 0.f );
        for( u32 i = 0; i < n; ++i )
        {
            result += lengthSqr( normalizeSafe( dir0[i] ) - normalizeSafe( dir1[i] ) );
        }

        return ::sqrt( result.getAsFloat() );
    }

    float computeVelocityCost( const Vector3* vel0, const Vector3* vel1, u32 numPos, float deltaTime )
    {
        floatInVec result( 0 );
        const floatInVec dt( deltaTime );
        for( u32 i = 0; i < numPos; ++i )
        {
            result += length( vel0[i] - vel1[i] ) * dt;
        }

        return ( result.getAsFloat() );
    }
    inline float computeVelocityCost( const Vector3 vA, const Vector3 vB )
    {
        floatInVec cost = length( vA - vB );
        //cost += oneVec - dot( normalizeSafe( vA ), normalizeSafe( vB ) );
        return cost.getAsFloat();
    }
    void rebuildTrajectory( Vector3* dstPoints, const Vector3* srcPoints, Curve3D& tmpCurve )
    {
        curve::clear( tmpCurve );
        float lengths[eNUM_TRAJECTORY_POINTS - 1] = {};
        float trajectory_length = 0.f;
        for( u32 j = 1; j < eNUM_TRAJECTORY_POINTS; ++j )
        {
            lengths[j - 1] = length( srcPoints[j] - srcPoints[j - 1] ).getAsFloat();
            trajectory_length += lengths[j - 1];
        }

        const float trajectory_length_inv = ( trajectory_length > FLT_EPSILON ) ? 1.f / trajectory_length : 0.f;
        float current_length = 0.f;
        for( u32 j = 0; j < eNUM_TRAJECTORY_POINTS; ++j )
        {
            if( j > 0 )
            {
                current_length += lengths[j-1];
            }
            float t = current_length * trajectory_length_inv;
            curve::push_back( tmpCurve, srcPoints[j], t );
        }
        for( u32 j = 0; j < eNUM_TRAJECTORY_POINTS; ++j )
        {
            dstPoints[j] = curve::evaluate_catmullrom( tmpCurve, (float)j / (float)( eNUM_TRAJECTORY_POINTS - 1 ) );
        }
    }

    Matrix3 computeBaseMatrix3( const Vector3 direction, const Vector3 acc, const Vector3 upReference )
    {
        if( lengthSqr( direction ).getAsFloat() > FLT_EPSILON )
        {
            Vector3 y = upReference;
            Vector3 x = normalize( cross( y, direction ) );
            Vector3 z = normalize( cross( x, y ) );
            Matrix3 rotation = Matrix3( x, y, z );

            floatInVec d = dot( acc, x );
            Matrix3 tilt_rotation = Matrix3::rotationZ( -d );
            rotation *= tilt_rotation;

            d = dot( acc, z );
            tilt_rotation = Matrix3::rotationX( d );
            rotation *= tilt_rotation;

            return rotation;
        }
        return Matrix3::identity();
    }


    inline float triangleArea( const Vector3 a, const Vector3 b, const Vector3 c ) 
    {
        return length( cross( a - b, a - c ) ).getAsFloat() * 0.5f;
    }
    inline float signedTriangleVolume( const Vector3 a, const Vector3 b, const Vector3 c )
    {
        return dot( a, cross( b, c ) ).getAsFloat() / 6.f;
    }

    float computeTrajectoryCost( const Vector3 pointsA[eNUM_TRAJECTORY_POINTS], const Vector3 pointsB[eNUM_TRAJECTORY_POINTS] )
    {
        //float lenA = 0.f;
        //float lenB = 0.f;

        //for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS - 1; ++i )
        //    lenA += length( pointsA[i] - pointsA[i + 1] ).getAsFloat();

        //for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS - 1; ++i )
        //    lenB += length( pointsB[i] - pointsB[i + 1] ).getAsFloat();

        //Vector3 axis = Vector3::zAxis();

        //float dirA = 0.f;
        //float dirB = 0.f;

        //for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        //    dirA += 1.f - dot( normalizeSafe( pointsA[i] ), axis ).getAsFloat();

        //for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        //    dirB += 1.f - dot( normalizeSafe( pointsB[i] ), axis ).getAsFloat();


        //float cost = ::abs( lenA - lenB ); // +::abs( dirA - dirB );
        //float tri_x_a = 0.f, tri_x_b = 0.f;
        //float tri_y_a = 0.f, tri_y_b = 0.f;
        //float tri_z_a = 0.f, tri_z_b = 0.f;

        //const Vector3 axisX = Vector3::xAxis();
        //const Vector3 axisY = Vector3::yAxis();
        //const Vector3 axisZ = Vector3::zAxis(); 

        //float area_x = 0.f;
        //float area_y = 0.f;
        //float area_z = 0.f;

        //for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        //{
        //    area_x += triangleArea( axisX, pointsA[i], pointsB[i] );
        //    area_y += triangleArea( axisY, pointsA[i], pointsB[i] );
        //    area_z += triangleArea( axisZ, pointsA[i], pointsB[i] );
        //    //tri_x_a += triangleArea( axisX, pointsA[i], pointsA[i + 1] );
        //    //tri_y_a += triangleArea( axisY, pointsA[i], pointsA[i + 1] );
        //    //tri_z_a += triangleArea( axisZ, pointsA[i], pointsA[i + 1] );
        //    //
        //    //tri_x_b += triangleArea( axisX, pointsB[i], pointsB[i + 1] );
        //    //tri_y_b += triangleArea( axisY, pointsB[i], pointsB[i + 1] );
        //    //tri_z_b += triangleArea( axisZ, pointsB[i], pointsB[i + 1] );
        //}
        
        float cost = computePositionCost( pointsA+1, pointsB+1, eNUM_TRAJECTORY_POINTS-1 );
        //cost += area_x + area_y + area_z;
        //cost += ::abs( tri_x_a - tri_x_b );
        //cost += ::abs( tri_y_a - tri_y_b );
        //cost += ::abs( tri_z_a - tri_z_b );
        return cost;


        //float cost = 0.f;
        //for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS-1; ++i )
        //{
        //    const Vector3 a = pointsA[i];
        //    const Vector3 b = pointsB[i];
        //    const Vector3 c = pointsB[i+1];
        //    const Vector3 d = pointsA[i+1];

        //    cost += triangleArea( a, b, c );
        //    cost += triangleArea( a, c, d );
        //}
        //return cost;
    }

}///

State::State()
{
    //memset( clip_index, 0xff, sizeof( clip_index ) );
    //memset( clip_eval_time, 0x00, sizeof( clip_eval_time ) );
    //memset( anim_blend, 0x00, sizeof( anim_blend ) );
}

void poseAllocate( Pose* pose, u32 numJoints, bxAllocator* allocator )
{
    /// I've left this function for future use
}

void poseFree( Pose* pose, bxAllocator* allocator )
{
    pose[0] = {};
}

void posePrepare( Pose* pose, const PosePrepareInfo& info )
{
    const bxAnim_Clip* clip = info.clip;
    const bxAnim_Skel* skel = info.skel;
    const u32 frameNo = info.frameNo;
    const Vector4 plane = makePlane( Vector3::yAxis(), Vector3( 0.f ) );

    std::vector< bxAnim_Joint > frame_joints_0;
    std::vector< bxAnim_Joint > frame_joints_1;
    std::vector< bxAnim_Joint > tmp_joints;
    frame_joints_0.resize( skel->numJoints );
    frame_joints_1.resize( skel->numJoints );
    tmp_joints.resize( skel->numJoints );

    bxAnim_Joint j0[_eMATCH_JOINT_COUNT_], j1[_eMATCH_JOINT_COUNT_];
    const u32 lastFrame = info.clip->numFrames - 1;
    if( frameNo == lastFrame )
    {
        bxAnim::evaluateClip( frame_joints_0.data(), clip, frameNo - 1, 0.f );
        bxAnim::evaluateClip( frame_joints_1.data(), clip, frameNo, 0.f );
    }
    else
    {
        bxAnim::evaluateClip( frame_joints_0.data(), clip, frameNo, 0.f );
        bxAnim::evaluateClip( frame_joints_1.data(), clip, frameNo + 1, 0.f );
    }

    {
        bxAnim_Joint root_joint = bxAnim_Joint::identity();

        bxAnimExt::localJointsToWorldJoints( tmp_joints.data(), frame_joints_0.data(), skel, root_joint );
        for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
            j0[i] = tmp_joints[info.joint_indices[i]];

        bxAnimExt::localJointsToWorldJoints( tmp_joints.data(), frame_joints_1.data(), skel, root_joint );
        for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
            j1[i] = tmp_joints[info.joint_indices[i]];
    }

    bxAnim_Joint current_joint;
    bxAnim::evaluateClip( &current_joint, clip, frameNo, 0.f, 0, 0 );
    const Vector3 current_displacement_xz = projectVectorOnPlane( current_joint.position, plane );
    for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
    {
        const Vector3 v0 = j0[i].position - current_displacement_xz;
        const Vector3 v1 = j1[i].position - current_displacement_xz;

        if( frameNo == lastFrame )
        {
            pose->pos[i] = v1;
            pose->rot[i] = j1[i].rotation;
        }
        else
        {
            pose->pos[i] = v0;
            pose->rot[i] = j0[i].rotation;
        }

        
        pose->vel[i] = ( v1 - v0 ) * clip->sampleFrequency;
    }
        
    const float frameTime = 0.f; // ( info.is_loop ) ? (float)frameNo / clip->sampleFrequency : 0.f;
    const float duration = clip->duration; // ( info.is_loop ) ? 1.f : clip->duration;
    computeClipTrajectory( &pose->trajectory, clip, frameTime, duration );
}
//-------------------------------------------------------------------
void stateAllocate( State* state, u32 numJoints, bxAllocator* allocator )
{
    u32 mem_size = 0;
    mem_size = numJoints * ( /*sizeof( *state->joint_local ) +*/ sizeof( *state->joint_world ) );

    state->_memory_handle = BX_MALLOC( allocator, mem_size, 16 );
    memset( state->_memory_handle, 0x00, mem_size );

    bxBufferChunker chunker( state->_memory_handle, mem_size );
    //state->joint_local = chunker.add< bxAnim_Joint >( numJoints );
    state->joint_world = chunker.add< bxAnim_Joint >( numJoints );
    chunker.check();
}

void stateFree( State* state, bxAllocator* allocator )
{
    BX_FREE( allocator, state->_memory_handle );
    state[0] = {};
}

void computeClipTrajectory( ClipTrajectory* ct, const bxAnim_Clip* clip, float trajectoryStartTime, float trajectoryDuration )
{
    const Vector4 plane = makePlane( Vector3::yAxis(), Vector3( 0.f ) );
    bxAnim_Joint root_joint;
    bxAnim::evaluateClip( &root_joint, clip, 0, 0.f, 0, 0 );
    const Vector3 root_displacement_xz = projectVectorOnPlane( root_joint.position, plane );
    const Vector3 root_displacement_y = mulPerElem( root_joint.position, Vector3( 0.f, 1.f, 0.f ) );

    bxAnim_Joint trajectory_joint;
    const float duration = trajectoryDuration;
    const float dt = ( duration ) / (float)( eNUM_TRAJECTORY_POINTS - 1 );
    const float start_time = trajectoryStartTime;

    Vector3 extrapolation_dpos( 0.f );
    bool end_clip_reached = false;
    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        float trajectory_time = start_time + dt * i;
        if( trajectory_time <= ( clip->duration + FLT_EPSILON ) )
        {
            trajectory_time = minOfPair( clip->duration, trajectory_time );
            bxAnim::evaluateClip( &trajectory_joint, clip, trajectory_time, 0, 0 );
            trajectory_joint.position -= root_displacement_xz;
            trajectory_joint.position -= root_displacement_y;
            ct->pos[i] = trajectory_joint.position;
        }
        else
        {
            trajectory_time = ::fmodf( trajectory_time, 1.f );

            bxAnim_Joint last_joint, prev_last_joint;
            bxAnim::evaluateClip( &last_joint, clip, clip->numFrames - 1, 0.f, 0, 0 );
            bxAnim::evaluateClip( &prev_last_joint, clip, clip->numFrames - 2, 0.f, 0, 0 );
            bxAnim::evaluateClip( &trajectory_joint, clip, trajectory_time, 0, 0 );

            Vector3 v = last_joint.position - prev_last_joint.position;

            if( i == 0 )
            {
                ct->pos[i] = last_joint.position;
            }
            else
            {
                const floatInVec displacement = length( trajectory_joint.position );

                ct->pos[i] = ct->pos[i - 1] + normalizeSafe( v ) * displacement;
            }
        }
    }
    utils::computeTrajectoryDerivatives( ct->vel, ct->acc, ct->pos, 1.f / dt );
}



//-------------------------------------------------------------------

void Context::load( const char* skelFile, const AnimClipInfo* animInfo, unsigned numAnims )
{
    if( !_allocator )
    {
        _allocator = bxDefaultAllocator();
    }

    bxResourceManager* resource_manager = resourceManagerGet();

    _data.skel = bxAnimExt::loadSkelFromFile( resource_manager, skelFile );
    SYS_ASSERT( _data.skel != nullptr );

    _data.clips.reserve( numAnims );
    _data.clip_infos.reserve( numAnims );
    for( unsigned i = 0; i < numAnims; ++i )
    {
        bxAnim_Clip* clip = bxAnimExt::loadAnimFromFile( resource_manager, animInfo[i].name.c_str() );
        if( !clip )
        {
            continue;
        }

        _data.clip_infos.push_back( animInfo[i] );
        _data.clips.push_back( clip );
    }
}

void Context::unload()
{
    bxResourceManager* resource_manager = resourceManagerGet();
    while( !_data.clips.empty() )
    {
        bxAnim_Clip* c = _data.clips.back();
        bxAnimExt::unloadAnimFromFile( resource_manager, &c );
        _data.clips.pop_back();
    }
        
    bxAnim_Skel* skel = (bxAnim_Skel*)_data.skel;
    bxAnimExt::unloadSkelFromFile( resource_manager, &skel );
    _data.skel = nullptr;
}

void Context::prepare( const ContextPrepareInfo& info )
{
    const u32 num_match_joints_names = _eMATCH_JOINT_COUNT_;
    _data.match_joints_indices.resize( num_match_joints_names, -1 );

    for( u32 i = 0; i < num_match_joints_names; ++i )
    {
        _data.match_joints_indices[i] = bxAnim::getJointByName( _data.skel, info.matching_joint_names[i] );
    }

    prepare_evaluateClips();
        
    stateAllocate( &_state, _data.skel->numJoints, _allocator );
    _anim_player.prepare( _data.skel, _allocator );

    curve::allocate( _state.input_trajectory_curve, eNUM_TRAJECTORY_POINTS );
    curve::allocate( _state.candidate_trajectory_curve, eNUM_TRAJECTORY_POINTS );
}

void Context::prepare_evaluateClips()
{
    const bxAnim_Skel* skel = _data.skel;
    std::vector< float > velocity;

    //_data.clip_trajectiories.resize( _data.clips.size() );

    for( size_t i = 0; i < _data.clips.size(); ++i )
    {
        const bxAnim_Clip* clip = _data.clips[i];
        u32 frame_no = 0;

        float velocity_sum = 0.f;
        u32 velocity_count = 0;

        const float frame_dt = 1.f / clip->sampleFrequency;
        while( frame_no < clip->numFrames )
        {
            Pose pose;
            PosePrepareInfo pose_prepare_info = {};
            pose_prepare_info.clip = clip;
            pose_prepare_info.skel = skel;
            pose_prepare_info.frameNo = frame_no;
            pose_prepare_info.joint_indices = _data.match_joints_indices.data();
            pose_prepare_info.is_loop = _data.clip_infos[i].is_loop != 0;

            poseAllocate( &pose, _data.skel->numJoints, _allocator );
            posePrepare( &pose, pose_prepare_info );

            pose.params.clip_index = (u32)i;
            pose.params.clip_start_time = frame_no * frame_dt;
            _data.poses.push_back( pose );

            const float pose_speed = length( pose.vel[0] ).getAsFloat();
            velocity_sum += pose_speed;

            velocity.push_back( pose_speed );
            ++velocity_count;
            ++frame_no;
        }
        
        //ClipTrajectory& clipTrajectory = _data.clip_trajectiories[i];
        //computeClipTrajectory( &clipTrajectory, clip, 1.f );
    }

        
    std::sort( velocity.begin(), velocity.end(), std::less<float>() );
    curve::allocate( _data.velocity_curve, (u32)velocity.size() );
    for( u32 i = 0; i < velocity.size(); ++i )
    {
        const float t = (float)i / float( velocity.size() - 1 );
        curve::push_back( _data.velocity_curve, velocity[i], t );
    }
}

void Context::unprepare()
{
    curve::deallocate( _state.candidate_trajectory_curve );
    curve::deallocate( _state.input_trajectory_curve );

    _anim_player.unprepare( _allocator );

    stateFree( &_state, _allocator );

    while( !_data.poses.empty() )
    {
        Pose& pose = _data.poses.back();
        poseFree( &pose, _allocator );
        _data.poses.pop_back();
    }
}

void Context::tick( const Input& input, float deltaTime )
{
    const float delta_time_inv = ( deltaTime > FLT_EPSILON ) ? 1.f / deltaTime : 0.f;
    const float desired_anim_speed = bx::curve::evaluate_catmullrom( _data.velocity_curve, input.speed01 );
    const float desired_anim_speed_inv = ( desired_anim_speed > FLT_EPSILON ) ? 1.f / desired_anim_speed : 0.f;


    const Matrix4 to_local_space = inverse( input.base_matrix_aligned );
    Vector3 local_trajectory[eNUM_TRAJECTORY_POINTS];
    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        local_trajectory[i] = mulAsVec4( to_local_space, input.trajectory[i] );
    }
    const Vector3 local_velocity = to_local_space.getUpper3x3() * input.velocity;
    const Vector3 local_acceleration = to_local_space.getUpper3x3() * input.acceleration;
    const Vector3 desired_anim_velocity = normalizeSafe( local_velocity ) * desired_anim_speed;

    //utils::rebuildTrajectory( local_trajectory, local_trajectory, _state.input_trajectory_curve );

    Vector3 local_trajectory_v[eNUM_TRAJECTORY_POINTS - 1];
    Vector3 local_trajectory_a[eNUM_TRAJECTORY_POINTS - 2];
    utils::computeTrajectoryDerivatives( local_trajectory_v, local_trajectory_a, local_trajectory, delta_time_inv );

    const Matrix3 desired_future_rotation = utils::computeBaseMatrix3( normalizeSafe( local_trajectory_v[2] ), local_trajectory_a[1] * deltaTime*deltaTime, Vector3::yAxis() );
    bxGfxDebugDraw::addAxes( input.base_matrix_aligned * Matrix4( desired_future_rotation, local_trajectory[3] ) );
    const u32 num_joints = _data.skel->numJoints;

    bxAnim_Joint* curr_local_joints = nullptr;

    Vector3 prev_matching_pos[_eMATCH_JOINT_COUNT_];
    Vector3 curr_matching_pos[_eMATCH_JOINT_COUNT_];
    Vector3 curr_matching_vel[_eMATCH_JOINT_COUNT_];

    memset( prev_matching_pos, 0x00, sizeof( prev_matching_pos ) );
    memset( curr_matching_pos, 0x00, sizeof( curr_matching_pos ) );
    memset( curr_matching_vel, 0x00, sizeof( curr_matching_vel ) );
    
    //if( _state.num_clips )
    if( !_anim_player.empty() )
    {
        const Vector4 yplane = makePlane( Vector3::yAxis(), Vector3( 0.f ) );
        
        bxAnim_Joint* tmp_joints = _state.joint_world;
        bxAnim_Joint* prev_local_joints = _anim_player.localJoints(); // bxAnim::poseFromStack( _state.anim_ctx, 0 );
        prev_local_joints[0].position = mulPerElem( prev_local_joints[0].position, Vector3( 0.f, 1.f, 0.f ) );
        
        bxAnimExt::localJointsToWorldJoints( tmp_joints, prev_local_joints, _data.skel, bxAnim_Joint::identity() );
        Vector3 root_displacement_xz = projectVectorOnPlane( tmp_joints[0].position, yplane );

        for( u32 i = 0; i < _data.match_joints_indices.size(); ++i )
        {
            i16 index = _data.match_joints_indices[i];
            prev_matching_pos[i] = tmp_joints[index].position - root_displacement_xz;
        }

        //float anim_speed = 0.f;
        //float anim_scaler = 1.f;
        //if( acceleration_value > 0.1f )
        //{
        //    if( currentSpeed( &anim_speed ) )
        //    {
        //        if( anim_speed > FLT_EPSILON )
        //        {
        //            anim_scaler = desired_anim_speed / anim_speed;
        //        }
        //    }
        //}
        //_state.anim_delta_time_scaler = signalFilter_lowPass( anim_scaler, _state.anim_delta_time_scaler, 0.1f, deltaTime );

        const float anim_delta_time = deltaTime; // *_state.anim_delta_time_scaler;
        _anim_player.tick( anim_delta_time );
        //tick_animations( anim_delta_time );
        curr_local_joints = _anim_player.localJoints(); // ::poseFromStack( _state.anim_ctx, 0 );
        curr_local_joints[0].position = mulPerElem( curr_local_joints[0].position, Vector3( 0.f, 1.f, 0.f ) );
        bxAnimExt::localJointsToWorldJoints( tmp_joints, curr_local_joints, _data.skel, bxAnim_Joint::identity() );
        root_displacement_xz = projectVectorOnPlane( tmp_joints[0].position, yplane );

        for( u32 i = 0; i < _data.match_joints_indices.size(); ++i )
        {
            i16 index = _data.match_joints_indices[i];
            const Vector3 pos = tmp_joints[index].position - root_displacement_xz;
            curr_matching_pos[i] = pos;
            curr_matching_vel[i] = ( pos - prev_matching_pos[i] ) * delta_time_inv;

            bxGfxDebugDraw::addSphere( Vector4( curr_matching_pos[i], 0.01f ), 0xFF0000FF, 1 );
            bxGfxDebugDraw::addLine( pos, pos + curr_matching_vel[i], 0x0000FFFF, 1 );

        }
    }

        
    _debug.pose_indices.clear();

    if( curr_local_joints )
    {
        bxAnim_Joint* tmp_joints = _state.joint_world;
        float change_cost = FLT_MAX;
        u32 change_index = UINT32_MAX;

        //const bool is_current_anim_looped = _data.clip_infos[_state.clip_index[0]].is_loop == 1;

        //u64 root_clip_index = UINT64_MAX;
        //bool root_stil_playing = _anim_player.userData( &root_clip_index, 0 );

        //bxAnim_Clip* current_clip = _data.clips[_state.clip_index[0]];
        //const float current_phase = ::fmodf( ( _state.clip_eval_time[0] ) / current_clip->duration, 1.f );
        //const float current_delta_phase = deltaTime / current_clip->duration;

        for( size_t i = 0; i < _data.poses.size(); ++i )
        {
            const Pose& pose = _data.poses[i];
            if( i == _state.pose_index )
                continue;

            const AnimClipInfo& clip_info = _data.clip_infos[pose.params.clip_index];
            const bxAnim_Clip* clip = _data.clips[pose.params.clip_index];
            const bool is_pose_clip_looped = clip_info.is_loop == 1;

            const ClipTrajectory& pose_trajectory = pose.trajectory; // .clip_trajectiories[pose.params.clip_index];
            
            //Vector3 pose_trajectory_pos[eNUM_TRAJECTORY_POINTS];
            //utils::rebuildTrajectory( pose_trajectory_pos, pose_trajectory.pos, _state.candidate_trajectory_curve );

            const float pose_position_cost = utils::computePositionCost( pose.pos, curr_matching_pos, _eMATCH_JOINT_COUNT_ );
            const float pose_velocity_cost = utils::computeVelocityCost( pose.vel+1, curr_matching_vel+1, _eMATCH_JOINT_COUNT_-1, deltaTime );
            
            const float velocity_cost = utils::computeVelocityCost( desired_anim_velocity, pose.vel[0] );// *desired_anim_speed_inv;
            //const float trajectory_position_cost = utils::computePositionCost( local_trajectory, pose_trajectory_pos, eNUM_TRAJECTORY_POINTS );
            const float trajectory_position_cost = utils::computeTrajectoryCost( local_trajectory, pose_trajectory.pos );
            const float trajectory_velocity_cost = utils::computeVelocityCost( local_trajectory_v, pose_trajectory.vel, eNUM_TRAJECTORY_POINTS - 1, deltaTime );
            const float trajectory_acceleration_cost = utils::computeVelocityCost( local_trajectory_a, pose_trajectory.acc, eNUM_TRAJECTORY_POINTS - 2, deltaTime*deltaTime );

            float cost = 0.f;
            cost += pose_position_cost;
            cost += pose_velocity_cost;
            
            cost += velocity_cost;
            cost += trajectory_position_cost;
            cost += trajectory_velocity_cost;
            cost += trajectory_acceleration_cost;
            checkFloat( cost );

            if( change_cost > cost )
            {
                change_cost = cost;
                change_index = (u32)i;
                _debug.pose_indices.push_back( change_index );
            }
        }

        const Pose& winner_pose = _data.poses[change_index];

        u64 current_clip_index = UINT64_MAX;
        bool valid = _anim_player.userData( &current_clip_index, 0 );
        SYS_ASSERT( valid );
        
        float curret_clip_time = 0.f;
        valid = _anim_player.evalTime( &curret_clip_time, 0 );
        SYS_ASSERT( valid );

        const bool winner_is_at_the_same_location = ( (u32)current_clip_index == winner_pose.params.clip_index ) && ::abs( curret_clip_time - winner_pose.params.clip_start_time ) < 0.2f;

        _state.pose_index = change_index;

        if( !winner_is_at_the_same_location )
        {
            bxAnim_Clip* clip = _data.clips[winner_pose.params.clip_index];
            _anim_player.play( clip, winner_pose.params.clip_start_time, 0.25f, winner_pose.params.clip_index );
            _state.pose_index = change_index;
        }


        //{
        //    const ClipTrajectory& pose_trajectory = winner_pose.trajectory;
        //    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        //    {
        //        bxGfxDebugDraw::addSphere( Vector4( mulAsVec4( input.base_matrix, pose_trajectory.pos[i] ), 0.03f ), 0xffffffff, 1 );
        //    }

        //    for( u32 j = 0; j < _eMATCH_JOINT_COUNT_; ++j )
        //    {
        //        const Vector3 pos = winner_pose.pos[j];
        //        const Vector3 vel = winner_pose.vel[j];
        //        bxGfxDebugDraw::addSphere( Vector4( pos, 0.02f ), 0x00FF00FF, 1 );
        //        bxGfxDebugDraw::addLine( pos, pos + vel, 0x00FFFFFF, 1 );
        //    }
        //}
    }
    else if( _anim_player.empty() )
    {
        const u32 change_index = 0;
        const Pose& winner_pose = _data.poses[change_index];
        bxAnim_Clip* clip = _data.clips[winner_pose.params.clip_index];
        _anim_player.play( clip, winner_pose.params.clip_start_time, 0.25f, winner_pose.params.clip_index );
        _anim_player.tick( deltaTime );
        _state.pose_index = change_index;
    }

    if( curr_local_joints )
    {
        bxAnim_Joint root = toAnimJoint_noScale( input.base_matrix_aligned );
        //curr_local_joints[0].position = mulPerElem( curr_local_joints[0].position, Vector3( 0.f, 1.f, 0.f ) );
        bxAnimExt::localJointsToWorldJoints( _state.joint_world, curr_local_joints, _data.skel, root );
        const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _data.skel->offsetParentIndices );
        utils::debugDrawJoints( _state.joint_world, parent_indices, num_joints, 0xffffffFF, 0.025f, 1.f );

        //bxGfxDebugDraw::addLine( _state.joint_world[0].position, _state.joint_world[0].position + input.raw_input, 0xFF00FF00, 1 );
        //bxGfxDebugDraw::addAxes( Matrix4( _state.joint_world[0].rotation, _state.joint_world[0].position ) );

        //if( _state.pose_index != UINT32_MAX )
        //{
        //    _DebugDrawPose( _state.pose_index, 0xFF0000FF, input.base_matrix_aligned );
        //}

        //for( u32 index : _debug.pose_indices )
        //{
        //    u32 color = ( index == _state.pose_index ) ? 0xFF0000FF : 0x222222FF;
        //    _DebugDrawPose( index, color, input.base_matrix_aligned );
        //}

    }
}

bool Context::currentSpeed( f32* value )
{
    if( _state.pose_index == UINT32_MAX )
    {
        return false;
    }

    const Pose& pose = _data.poses[_state.pose_index];
    value[0] = length( pose.vel[0] ).getAsFloat();

    return true;
}

bool Context::currentPose( Matrix4* pose )
{
    if( _anim_player.empty() )
        return false;

    pose[0] = Matrix4( _state.joint_world[0].rotation, _state.joint_world[0].position );
    return true;
}

void Context::_DebugDrawPose( u32 poseIndex, u32 color, const Matrix4& base )
{
    if( poseIndex >= _data.poses.size() )
        return;

    const Pose& pose = _data.poses[poseIndex];

    const bxAnim_Joint root_joint = toAnimJoint_noScale( base );
    

    const ClipTrajectory& pose_trajectory = pose.trajectory;

    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        bxGfxDebugDraw::addSphere( Vector4( pose_trajectory.pos[i], 0.03f ), 0x666666ff, 1 );
    }

    //const bxAnim_Joint& j = _debug.joints[0];
    for( u32 i = 0; i < _eMATCH_JOINT_COUNT_; ++i )
    {
        const Vector3 p = fastTransform( root_joint.rotation, root_joint.position, pose.pos[i] ); // j.position;
        const Vector3 v = fastTransform( root_joint.rotation, root_joint.position, pose.vel[i] );
        //const Vector3 a = fastTransform( j.rotation, j.position, pose.params.acceleration );
        bxGfxDebugDraw::addSphere( Vector4( p, 0.01f ), 0x0000FFFF, 1 );
        bxGfxDebugDraw::addLine( p, v, 0x0000FFFF, 1 );
        //bxGfxDebugDraw::addLine( p, a, 0xFFFF00FF, 1 );
    }

    _debug.joints0.resize( _data.skel->numJoints );
    _debug.joints1.resize( _data.skel->numJoints );
    
    const bxAnim_Clip* clip = _data.clips[pose.params.clip_index];
    const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, _data.skel->offsetParentIndices );
    bxAnim::evaluateClip( _debug.joints0.data(), clip, pose.params.clip_start_time );
    _debug.joints0[0].position = mulPerElem( _debug.joints0[0].position, Vector3( 0.f, 1.f, 0.f ) );
    bxAnimExt::localJointsToWorldJoints( _debug.joints1.data(), _debug.joints0.data(), _data.skel, root_joint );
    utils::debugDrawJoints( _debug.joints1.data(), parent_indices, _data.skel->numJoints, 0x333333ff, 0.01f, 1.f );

}
}}///




namespace bx{
namespace motion_matching{

void DynamicState::tick( const bxInput& sysInput, const Input& input, float deltaTime )
{
    characterInputCollectData( &_input, sysInput, deltaTime, 0.02f );

    const Vector4 yplane = makePlane( Vector3::yAxis(), _position );
    const Vector3 anim_dir = normalizeSafe( projectVectorOnPlane( input.anim_pose.getCol2().getXYZ(), yplane ) );

    const Vector3 x_input_force = Vector3::xAxis() * _input.analogX;
    const Vector3 z_input_force = -Vector3::zAxis() * _input.analogY;
    //const Vector3 y_input_force = Vector3::yAxis() * ( ( _input.jump > 0.1f ) ? _input.jump*100.f : -9.8f );
    Vector3 input_force = x_input_force + z_input_force;// +y_input_force;

    const float converge_time01 = 0.0001f;
    const float lerp_alpha = 1.f - ::pow( converge_time01, deltaTime );

    const float max_input_speed = lerp( _input.R2, 0.6f, 1.f );

    const float input_speed = minOfPair( max_input_speed, length( input_force ).getAsFloat() );
    input_force = normalizeSafe( input_force ) * input_speed;
    _prev_speed01 = _speed01;
    _speed01 = lerp( lerp_alpha, _speed01, input_speed );
    //_speed01 *= _speed01;
    //
    _prev_direction = _direction;
    //if( input.data_valid )
    //{
    //    _direction = anim_dir;
    //    _direction = normalizeSafe( _direction );
    //}

    //if( input.data_valid )
    //{
    //    const float speed_converge_time01 = 0.01f;
    //    const float speed_lerp_alpha = 1.f - ::pow( speed_converge_time01, deltaTime );

    //    _max_speed = lerp( speed_lerp_alpha, _max_speed, input.anim_velocity );
    //}


    if( input_speed * input_speed > FLT_EPSILON )
    {
        const Vector3 input_dir = normalize( input_force );
        //_direction = input_dir;
        _direction = slerp( lerp_alpha, _direction, input_force );
        _direction = normalizeSafe( _direction );
    }

    

    const float delta_time_inv = ( deltaTime > 0.f ) ? 1.f / deltaTime : 0.f;
    const float spd = ( _speed01 < 0.1f ) ? 0 : _max_speed; // *_speed01;

    //_acceleration = ( _direction - _prev_direction ) * spd  * delta_time_inv;
    Vector3 prev_vel = _velocity;
    _velocity = _direction * spd;
    _acceleration = ( _velocity - prev_vel ) * delta_time_inv;
    _position += _velocity * deltaTime;

    //////////////////////////////////////////////////////////////////////////
    //mothahack
    if( _position.getY().getAsFloat() < 0.f )
    {
        _position.setY( 0.f );
    }
    //////////////////////////////////////////////////////////////////////////

    const float dt = _trajectory_integration_time / ( eNUM_TRAJECTORY_POINTS - 1 );
    float time = dt;
    _trajectory[0] = _position;
    Vector3 trajectory_velocity = _velocity;
    Vector3 trajectory_acceleration = _acceleration;
    for( u32 i = 1; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        trajectory_velocity += trajectory_acceleration * dt;
        _trajectory[i] = _trajectory[i - 1] + trajectory_velocity * dt;
        bxGfxDebugDraw::addLine( _trajectory[i - 1], _trajectory[i], 0xFFFFFFFF, 1 );
        //bxGfxDebugDraw::addLine( _trajectory[i], _trajectory[i] + trajectory_velocity, 0x00FFFFFF, 1 );
        //bxGfxDebugDraw::addLine( _trajectory[i], _trajectory[i] + trajectory_acceleration, 0xFF0000FF, 1 );
    }

    _last_delta_time = deltaTime;
    _prev_input_force = input_force;
}

Matrix4 DynamicState::computeBaseMatrix( bool includeTilt ) const
{
    const Vector3 acc = ( includeTilt ) ? _acceleration * _last_delta_time : Vector3( 0.f );
    Matrix3 rotation = utils::computeBaseMatrix3( _direction, acc, Vector3::yAxis() );
    return Matrix4( rotation, _position );
}

void DynamicState::debugDraw( u32 color )
{
    bxGfxDebugDraw::addLine( _position, _position + _direction, 0x0000FFFF, 1 );
    bxGfxDebugDraw::addLine( _position, _position + _direction * _speed01, color, 1 );

    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
    {
        bxGfxDebugDraw::addSphere( Vector4( _trajectory[i], 0.03f ), 0x666666ff, 1 );
    }

    const Matrix4 base = computeBaseMatrix();
    const Matrix4 base_with_tilt = computeBaseMatrix( true );
    //bxGfxDebugDraw::addAxes( base );
    bxGfxDebugDraw::addAxes( base_with_tilt );
}

void motionMatchingCollectInput( Input* input, const DynamicState& dstate )
{
    for( u32 i = 0; i < eNUM_TRAJECTORY_POINTS; ++i )
        input->trajectory[i] = dstate._trajectory[i];

    input->velocity = dstate._velocity;
    input->acceleration = dstate._acceleration;
    input->base_matrix = dstate.computeBaseMatrix( true );
    input->base_matrix_aligned = dstate.computeBaseMatrix( false );
    input->speed01 = dstate._speed01;
    input->raw_input = dstate._prev_input_force;
}

}}////

