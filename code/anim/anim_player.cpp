#include "anim_player.h"
#include "anim.h"

#include <util/memory.h>
#include <util/common.h>

namespace bx{
namespace anim{

void CascadePlayer::prepare( const bxAnim_Skel* skel, bxAllocator* allcator /*= nullptr */ )
{
    _ctx = bxAnim::contextInit( *skel );

}

void CascadePlayer::unprepare( bxAllocator* allocator /*= nullptr */ )
{
    bxAnim::contextDeinit( &_ctx );
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

const bxAnim_Joint* CascadePlayer::localJoints() const
{
    return bxAnim::poseFromStack( _ctx, 0 );
}

bxAnim_Joint* CascadePlayer::localJoints()
{
    return bxAnim::poseFromStack( _ctx, 0 );
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
                                     &leaf, 1
                                     );
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
                                     leaves, num_leaves
                                    );


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
    _ctx = bxAnim::contextInit( *skel );
    _prev_joints = (bxAnim_Joint*)BX_MALLOC( allcator, skel->numJoints * sizeof( bxAnim_Joint ), 16 );
    for( u32 i = 0; i < skel->numJoints; ++i )
    {
        _prev_joints[i] = bxAnim_Joint::identity();
    }
}


void SimplePlayer::unprepare( bxAllocator* allocator /*= nullptr */ )
{
    BX_FREE0( allocator, _prev_joints );
    bxAnim::contextDeinit( &_ctx );
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
    memcpy( _prev_joints, localJoints(), _ctx->numJoints * sizeof( bxAnim_Joint ) );
    _Tick_processBlendTree();
    _Tick_updateTime( deltaTime );
}

void SimplePlayer::_ClipUpdateTime( Clip* clip, float deltaTime )
{
    clip->eval_time = ::fmodf( clip->eval_time + deltaTime, clip->clip->duration );
}

float SimplePlayer::_ClipPhase( const Clip& clip )
{
    return clip.eval_time / clip.clip->duration;
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
        bxAnimExt::processBlendTree( _ctx, 0 | bxAnim::eBLEND_TREE_LEAF, nullptr, 0, &leaf, 1 );
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
        bxAnimExt::processBlendTree( _ctx, 0 | bxAnim::eBLEND_TREE_BRANCH, &branch, 1, leaves, 2 );
    }
}

void SimplePlayer::_Tick_updateTime( float deltaTime )
{
    if( _num_clips == 0 )
    {
 
    }
    else if( _num_clips == 1 )
    {
        _ClipUpdateTime( &_clips[0], deltaTime );
    }
    else
    {
        Clip& c0 = _clips[0];
        Clip& c1 = _clips[1];
        
        const bxAnim_Clip* clipA = c0.clip;
        const bxAnim_Clip* clipB = c1.clip;

        const float blend_alpha = minOfPair( 1.f, _blend_time / _blend_duration );
        const float clip_duration = lerp( blend_alpha, clipA->duration, clipB->duration );
        const float delta_phase = deltaTime / clip_duration;

        float phaseA = _ClipPhase( c0 );
        float phaseB = _ClipPhase( c1 );
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

const bxAnim_Joint* SimplePlayer::localJoints() const
{
    return bxAnim::poseFromStack( _ctx, 0 );
}

bxAnim_Joint* SimplePlayer::localJoints()
{
    return bxAnim::poseFromStack( _ctx, 0 );
}

const bxAnim_Joint* SimplePlayer::prevLocalJoints() const
{
    return _prev_joints;
}

bxAnim_Joint* SimplePlayer::prevLocalJoints()
{
    return _prev_joints;
}

}}///

