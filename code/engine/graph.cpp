#include "graph.h"



#include <algorithm>

#include "graph_context.h"

namespace bx
{
    void Graph::startup()
    {
        _flag_recompute = 0;
    }

    void Graph::shutdown( bool destroyNodes )
    {
        for( int i = 0; i < array::size( _id_nodes ); ++i )
        {
            graphNodeRemove( _id_nodes[i], destroyNodes );
        }
    }

    int Graph::nodeFind( id_t id )
    {
        return array::find1( array::begin( _id_nodes ), array::end( _id_nodes ), array::OpEqual<id_t>( id ) );
    }

    union NodeSortKey
    {
        u64 hash = 0;
        struct
        {
            u64 index_in_graph : 16;
            u64 instance_index : 16;
            u64 type : 16;
            u64 depth : 16;
        };
    };
    void Graph::compile()
    {
        _Compile( &_id_nodes_tick_sort0, EExecMask::eTICK0 );
        _Compile( &_id_nodes_tick_sort1, EExecMask::eTICK1 );
    }

    void Graph::_Compile( array_t<id_t>* tickSort, u32 execMask )
    {
        array_t< u64 > sortArray;
        array::reserve( sortArray, array::size( _id_nodes ) );

        for( int i = 0; i < array::size( _id_nodes ); ++i )
        {
            id_t id = _id_nodes[i];
            NodeInstanceInfo info = nodeInstanceInfoGet( id );
            NodeType* type = graphContext()->typeGet( info._type_index );
            if( type->info->_behaviour.exec_mask & execMask )
            {
                NodeSortKey key;
                key.depth = 1;
                key.type = info._type_index;
                key.instance_index = id.index;
                key.index_in_graph = i;

                array::push_back( sortArray, key.hash );
            }
        }

        std::sort( array::begin( sortArray ), array::end( sortArray ), std::less< u64 >() );

        array::clear( *tickSort );
        array::reserve( *tickSort, array::size( sortArray ) );

        for( int i = 0; i < array::size( sortArray ); ++i )
        {
            NodeSortKey key;
            key.hash = sortArray[i];
            array::push_back( *tickSort, _id_nodes[key.index_in_graph] );
        }
    }

    void Graph::nodesLoad( Scene* scene )
    {
        bool done = false;
        bool recomputeGraph = false;
        do
        {
            id_t id;
            done = !pop_from_back_with_lock( &id, _nodes_to_load, _lock_nodes_to_load );

            if( !done && graphContext()->nodeIsValid( id ) )
            {
                NodeInstanceInfo info = nodeInstanceInfoGet( id );
                Node* node = nodeInstanceGet( id );
                NodeType* type = graphContext()->typeGet( info._type_index );
                type->info->_interface->load( node, info, scene );
                //( *type->info->_load )( node, info, scene );

                _lock_nodes.lock();
                array::push_back( _id_nodes, id );
                _lock_nodes.unlock();

                recomputeGraph = true;
            }

        } while( !done );

        _flag_recompute |= recomputeGraph;
    }

    void Graph::nodesUnload( Scene* scene )
    {
        bool done = false;
        bool recomputeGraph = false;
        do
        {
            Graph::NodeToUnload ntu;
            done = !pop_from_back_with_lock( &ntu, _nodes_to_unload, _lock_nodes_to_unload );

            if( !done && graphContext()->nodeIsValid( ntu.id ) )
            {
                NodeInstanceInfo* info = graphContext()->nodeInstanceInfoGet( ntu.id );
                Node* node = nodeInstanceGet( ntu.id );
                NodeType* type = graphContext()->typeGet( info->_type_index );
                type->info->_interface->unload( node, *info, scene );

                info->_graph = nullptr;

                _lock_nodes.lock();
                int found = nodeFind( ntu.id );
                SYS_ASSERT( found != -1 );
                array::erase_swap( _id_nodes, found );
                _lock_nodes.unlock();

                if( ntu.destroyAfterUnload )
                {
                    nodeDestroy( &ntu.id );
                }

                recomputeGraph = true;
            }
        } while( !done );

        _flag_recompute |= recomputeGraph;
    }

    void Graph::nodesTick( const array_t<id_t>& tickArray, Scene* scene )
    {
        const int n = array::size( tickArray );
        for( int i = 0; i < n; ++i )
        {
            id_t id = tickArray[i];

            NodeInstanceInfo info = nodeInstanceInfoGet( id );
            Node* node = nodeInstanceGet( id );
            NodeType* type = graphContext()->typeGet( info._type_index );
            type->info->_interface->tick0( node, info, scene );
            //( *type->info->_tick )( node, info, scene );
        }
    }

    void Graph::tick0( Scene* scene )
    {
        _lock_nodes.lock();

        if( _flag_recompute )
        {
            _flag_recompute = false;
            compile();
        }

        //graph_nodesTick( graph, scene );
        nodesTick( _id_nodes_tick_sort0, scene );

        _lock_nodes.unlock();
    }

    void Graph::tick1( Scene* scene )
    {
        _lock_nodes.lock();
        nodesTick( _id_nodes_tick_sort1, scene );
        _lock_nodes.unlock();
    }

}///

namespace bx
{

    //////////////////////////////////////////////////////////////////////////
    void graphPreTick( Graph* graph, Scene* scene );
    void graphTick( Graph* graph, Scene* scene );

    //////////////////////////////////////////////////////////////////////////
    bool graphNodeAdd( Graph* graph, id_t id )
    {
        bool result = false;

        //graph->_lock_nodes.lock();
        //int found = graph->nodeFind( id );
        //graph->_lock_nodes.unlock();

        //if( found == -1 )
        {
            graphContext()->nodesLock();
            NodeInstanceInfo* info = graphContext()->nodeInstanceInfoGet( id );
            if( info )
            {
                if( info->_graph )
                {
                    bxLogError( "Node is already in graph." );
                }
                else
                {
                    info->_graph = graph;
                    result = true;
                }
            }
            graphContext()->nodesUnlock();
        }

        if( result )
        {
            //graph->_lock_nodes.lock();
            //array::push_back( graph->_id_nodes, id );
            //graph->_lock_nodes.unlock();

            graph->_lock_nodes_to_load.lock();
            queue::push_front( graph->_nodes_to_load, id );
            graph->_lock_nodes_to_load.unlock();
            //graph->_flag_recompute = true;
        }

        return result;
    }
    void graphNodeRemove( id_t id, bool destroyNode )
    {
        Graph* g = nullptr;

        graphContext()->nodesLock();
        NodeInstanceInfo* info = graphContext()->nodeInstanceInfoGet( id );
        if( info )
        {
            g = info->_graph;
            //info->_graph = nullptr;
        }
        graphContext()->nodesUnlock();

        if( g )
        {
            Graph::NodeToUnload ntu;
            ntu.id = id;
            ntu.destroyAfterUnload = destroyNode;

            g->_lock_nodes_to_unload.lock();
            queue::push_front( g->_nodes_to_unload, ntu );
            g->_lock_nodes_to_unload.unlock();
            //g->_flag_recompute = true;
        }
    }
    void graphNodeLink( id_t parent, id_t child, Scene* scene )
    {
        Graph* graphParent = nullptr;
        Graph* graphChild = nullptr;

        graphContext()->nodesLock();
        NodeInstanceInfo* infoParent = graphContext()->nodeInstanceInfoGet( parent );
        NodeInstanceInfo* infoChild = graphContext()->nodeInstanceInfoGet( child );
        if( infoParent && infoChild )
        {
            graphParent = infoParent->_graph;
            graphChild = infoChild->_graph;
        }
        graphContext()->nodesUnlock();

        if( graphParent && graphChild )
        {
            if( graphParent == graphChild )
            {
                INode* iParent = graphContext()->interfaceGet( parent );
                INode* iChild = graphContext()->interfaceGet( child );

                iParent->onChildLink( *infoParent, *infoChild, scene );
                iChild->onParentLink( *infoParent, *infoChild, scene );

                //SceneGraph* sg = &graphParent->_scene_graph;
                //TransformInstance parentTi = sg->get( parent );
                //TransformInstance childTi = sg->get( child );
                //if( sg->isValid( parentTi ) && sg->isValid( childTi ) )
                //{
                //    sg->link( parentTi, childTi );
                //}
                //else
                //{
                //    bxLogError( "One of nodes has invalid TransformInstance component" );
                //}
            }
            else
            {
                bxLogError( "Can not link nodes from different graphs" );
            }
        }
        else
        {
            bxLogError( "One of graph is null (parent or child)" );
        }
    }

    void graphNodeUnlink( id_t child, Scene* scene )
    {
        Graph* g = nullptr;

        graphContext()->nodesLock();
        NodeInstanceInfo* info = graphContext()->nodeInstanceInfoGet( child );
        if( info )
        {
            g = info->_graph;
        }
        graphContext()->nodesUnlock();

        if( g )
        {
            INode* iface = graphContext()->interfaceGet( child );
            iface->onUnlink( *info, scene );
            //SceneGraph* sg = &g->_scene_graph;
            //TransformInstance childTi = sg->get( child );
            //if (sg->isValid( childTi ) )
            //{
            //    sg->unlink( childTi );
            //}
            //else 
            //{
            //    bxLogError( "Node has invalid TransformInstance component" );
            //}
        }
        else
        {
            bxLogError( "Node has invalid graph" );
        }
    }

    //
    void GraphSceneScriptCallback::addCallback( bxAsciiScript* script )
    {
        for( int i = 0; i < array::size( graphContext()->_node_types ); ++i )
        {
            const char* typeName = graphContext()->_node_types[i].info->_name;
            bxScene::script_addCallback( script, typeName, this );
        }

        bxScene::script_addCallback( script, "link", this );

        _current_id = makeInvalidHandle<id_t>();
    }

    void GraphSceneScriptCallback::onCreate( const char* typeName, const char* objectName )
    {
        bool bres = nodeCreate( &_current_id, typeName, objectName );
        if( bres )
        {
            bres = graphNodeAdd( _graph, _current_id );
            if( !bres )
            {
                SYS_NOT_IMPLEMENTED;
            }
        }
        else
        {
            _current_id = makeInvalidHandle<id_t>();
            bxLogError( "Failed to create node '%s:%s", typeName, objectName );
        }
    }

    namespace
    {
        bool nodeAttributeSetByName1( id_t id, const char* name, const void* data, unsigned dataSize )
        {
            bool result = false;
            NodeType* nodeType = nodeTypeGet( id );
            int index = nodeType->attributes.find( name );
            if( index != -1 )
            {
                AttributeType::Enum type = nodeType->attributes.type( index );
                SYS_ASSERT( type != AttributeType::eTYPE_COUNT );

                if( type != AttributeType::eSTRING )
                {
                    const unsigned stride = nodeType->attributes.stride( index );
                    if( stride <= dataSize )
                    {
                        graphContext()->attributesInstanceGet( id )->dataSet( index, type, data );
                        result = true;
                    }
                }
                else
                {
                    graphContext()->attributesInstanceGet( id )->stringSet( index, (const char*)data );
                    result = true;
                }
            }
            return result;
        }
    }///

    void GraphSceneScriptCallback::onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData )
    {
        if( !nodeIsAlive( _current_id ) )
            return;

        bool bres = nodeAttributeSetByName1( _current_id, attrName, attribData.dataPointer(), attribData.dataSizeInBytes() );
        if( !bres )
        {
            bxLogError( "node attribute '%s' set failed", attrName );
        }
    }

    void GraphSceneScriptCallback::onCommand( const char* cmdName, const bxAsciiScript_AttribData& args )
    {
        (void)cmdName;
        (void)args;
    }



}////
