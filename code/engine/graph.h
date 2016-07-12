#pragma once
#include "graph_node.h"

#include <util/ascii_script.h>
#include <util/array.h>
#include <util/array_util.h>
#include <util/queue.h>
#include <util/string_util.h>
#include <util/thread/mutex.h>
#include <atomic>

namespace bx
{
    //////////////////////////////////////////////////////////////////////////
    struct Graph
    {
        struct NodeToUnload
        {
            id_t id = makeInvalidHandle<id_t>();
            bool destroyAfterUnload = false;
        };

        array_t<id_t>           _id_nodes;
        queue_t<id_t>           _nodes_to_load;
        queue_t<NodeToUnload>   _nodes_to_unload;
        array_t<id_t>           _id_nodes_tick_sort0;
        array_t<id_t>           _id_nodes_tick_sort1;

        bxBenaphore _lock_nodes;
        bxBenaphore _lock_nodes_to_load;
        bxBenaphore _lock_nodes_to_unload;

        std::atomic_uint32_t _flag_recompute;

        //SceneGraph _scene_graph;

        void startup();
        void shutdown( bool destroyNodes );
        int nodeFind( id_t id );
        
        //////////////////////////////////////////////////////////////////////////
        void compile();
        void _Compile( array_t<id_t>* tickSort, u32 execMask );

        //////////////////////////////////////////////////////////////////////////
        void nodesLoad( Scene* scene );
        void nodesUnload( Scene* scene );
        void nodesTick( const array_t<id_t>& tickArray, Scene* scene );
        
        //////////////////////////////////////////////////////////////////////////
        void tick0( Scene* scene );
        void tick1( Scene* scene );
    };
    //////////////////////////////////////////////////////////////////////////
    bool graphNodeAdd( Graph* graph, id_t id );
    void graphNodeRemove( id_t id, bool destroyNode = false );
    void graphNodeLink( id_t parent, id_t child, Scene* scene );
    void graphNodeUnlink( id_t child, Scene* scene );
        
    //////////////////////////////////////////////////////////////////////////
    struct GraphSceneScriptCallback : public bxAsciiScript_Callback
    {
        virtual void addCallback( bxAsciiScript* script );
        virtual void onCreate( const char* typeName, const char* objectName );
        virtual void onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData );
        virtual void onCommand( const char* cmdName, const bxAsciiScript_AttribData& args );

        Graph* _graph = nullptr;
        Scene* _scene = nullptr;
        id_t _current_id = makeInvalidHandle<id_t>();
    };

}///
