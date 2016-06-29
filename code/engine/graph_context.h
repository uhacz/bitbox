#pragma once
#include <util/type.h>
#include <util/debug.h>
#include <util/pool_allocator.h>
#include <util/containers.h>
#include <util/thread/mutex.h>
#include <util/id_table.h>
#include "graph_attributes.h"
#include "graph_node.h"


namespace bx
{
    struct Engine;
    //////////////////////////////////////////////////////////////////////////
    struct NodeType
    {
        i32 index = -1;
        const NodeTypeInfo* info = nullptr;
        AttributeStruct attributes;
    };
    NodeType* nodeTypeGet( id_t id );
    
    //////////////////////////////////////////////////////////////////////////
    struct NodeToDestroy
    {
        Node* node = nullptr;
        NodeInstanceInfo* info = nullptr;
        AttributeInstance* attributes = nullptr;
    };
    struct GraphToLoad
    {
        Graph* graph = nullptr;
        char* filename = nullptr;

        void set( Graph* g, const char* fn );
        void release();
    };
    struct GraphToUnload
    {
        Graph* graph = nullptr;
        bool destroyAfterUnload = false;
    };

    struct GraphContext
    {
        enum
        {
            eMAX_NODES = 1024 * 8,
            eMAX_GRAPHS = 32,
        };
        array_t< NodeType >         _node_types;
        id_table_t< eMAX_NODES >    _id_table;
        Node*                       _nodes[eMAX_NODES];
        AttributeInstance*          _attributes[eMAX_NODES];
        NodeInstanceInfo*           _instance_info[eMAX_NODES];

        bxPoolAllocator             _alloc_instance_info;

        Graph*                      _graphs[eMAX_GRAPHS];
        u32                         _graphs_count = 0;

        array_t< NodeToDestroy >    _nodes_to_destroy;
        //array_t< Graph* >           _graphs;
        queue_t< GraphToLoad >      _graphs_to_load;
        queue_t< GraphToUnload >    _graphs_to_unload;
        queue_t< Graph* >           _graphs_to_destroy;

        bxRecursiveBenaphore        _lock_nodes;
        bxRecursiveBenaphore        _lock_instance_info_alloc;
        bxBenaphore                 _lock_graphs_alloc;
        bxBenaphore                 _lock_graphs_to_load;
        bxBenaphore                 _lock_graphs_to_unload;
        bxBenaphore                 _lock_nodes_to_destroy;

        //////////////////////////////////////////////////////////////////////////
        void startup();
        void shutdown();

        void nodesLock() { _lock_nodes.lock(); }
        void nodesUnlock() { _lock_nodes.unlock(); }
        //void graphsLock() { _lock_graphs.lock(); }
        //void graphsUnlock() { _lock_graphs.unlock(); }

        //////////////////////////////////////////////////////////////////////////
        NodeInstanceInfo* nodeInstanceInfoAllocate();
        void nodeInstanceInfoFree( NodeInstanceInfo* info );

        NodeInstanceInfo* nodeInstanceInfoGet( id_t id )
        {
            return _instance_info[id.index];
        }
        AttributeInstance* attributesInstanceGet( id_t id )
        {
            return _attributes[id.index];
        }

        //////////////////////////////////////////////////////////////////////////
        NodeType* typeGet( int index )
        {
            return &_node_types[index];
        }

        int typeFind( const char* name );
        int typeAdd( const NodeTypeInfo* info );

        //////////////////////////////////////////////////////////////////////////
        bool nodeIsValid( id_t id ) const
        {
            return id_table::has( _id_table, id );
        }
        void idAcquire( id_t* id );
        void idRelease( id_t id );

        //////////////////////////////////////////////////////////////////////////
        void nodeCreate( id_t id, int typeIndex, const char* nodeName );
        void nodeDestroy( Node* node, NodeInstanceInfo* info, AttributeInstance* attrI );
        
        //////////////////////////////////////////////////////////////////////////
        Graph* graphAllocate();
        void graphFree( Graph* graph );

        void graphsLoad( Engine* engine, Scene* scene );
        void graphsUnload( Scene* scene );
        void graphsDestroy();

        void nodesDestroy();
    };

    //////////////////////////////////////////////////////////////////////////
    void graphContextStartup();
    void graphContextShutdown();
    void graphContextCleanup( Engine* engine, Scene* scene );
    void graphContextTick( Engine* engine, Scene* scene );
    GraphContext* graphContext();

    //////////////////////////////////////////////////////////////////////////
    bool nodeRegister( const NodeTypeInfo* typeInfo );
    bool nodeCreate( id_t* out, const char* typeName, const char* nodeName );
    void nodeDestroy( id_t* inOut );
    //////////////////////////////////////////////////////////////////////////
    void graphCreate( Graph** graph );
    void graphDestroy( Graph** graph, bool destroyNodes = true );
    void graphLoad( Graph* graph, const char* filename );
    //////////////////////////////////////////////////////////////////////////
}////

