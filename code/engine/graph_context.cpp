#include "graph_context.h"
#include <util/string_util.h>
#include <util/array.h>
#include <engine/engine.h>
#include <string.h>

#include "graph.h"


namespace bx
{
    bool nodeInstanceInfoEmpty( const NodeInstanceInfo& info )
    {
        bool result = true;
        result &= info._type_index == -1;
        result &= info._id.hash == 0;

        result &= info._type_name == nullptr;
        result &= info._name == nullptr;

        result &= info._graph == nullptr;
        //result &= info._parent.hash == 0;
        //result &= info._first_child.hash == 0;
        //result &= info._next_slibling.hash == 0;
        return result;
    }

    void nodeInstanceInfoClear( NodeInstanceInfo* info )
    {
        memset( info, 0x00, sizeof( NodeInstanceInfo ) );
        info->_type_index = -1;
    }

    void nodeInstanceInfoRelease( NodeInstanceInfo* info )
    {
        string::free( (char*)info->_name );
        nodeInstanceInfoClear( info );
    }

    NodeType* nodeTypeGet( id_t id )
    {
        SYS_ASSERT( graphContext()->nodeIsValid( id ) );
        NodeInstanceInfo* info = graphContext()->nodeInstanceInfoGet( id );
        NodeType* type = graphContext()->typeGet( info->_type_index );
        return type;
    }

    void GraphToLoad::set( Graph* g, const char* fn )
    {
        graph = g;
        filename = string::duplicate( filename, fn );
    }

    void GraphToLoad::release()
    {
        string::free_and_null( &filename );
    }

    void GraphContext::startup()
    {
        memset( _nodes, 0x00, sizeof( _nodes ) );
        memset( _instance_info, 0x00, sizeof( _instance_info ) );
        memset( _attributes, 0x00, sizeof( _attributes ) );
        memset( _graphs, 0x00, sizeof( _graphs ) );
        _alloc_instance_info.startup( sizeof( NodeInstanceInfo ), eMAX_NODES, bxDefaultAllocator(), 8 );
    }

    void GraphContext::shutdown()
    {
        SYS_ASSERT( id_table::size( _id_table ) == 0 );
        for( int i = 0; i < eMAX_NODES; ++i )
        {
            SYS_ASSERT( _nodes[i] == nullptr );
            SYS_ASSERT( _instance_info[i] == nullptr );
        }
        _alloc_instance_info.shutdown();

        for( int i = 0; i < array::size( _node_types ); ++i )
        {
            _node_types[i].info->_interface->typeDeinit();
            //if( _node_types[i].info->_interface->typeInit )
            //{
            //    ( *_node_types[i].info->_type_deinit )( );
            //}

            _node_types[i].attributes.free();
        }
    }

    NodeInstanceInfo* GraphContext::nodeInstanceInfoAllocate()
    {
        _lock_instance_info_alloc.lock();
        NodeInstanceInfo* info = (NodeInstanceInfo*)_alloc_instance_info.alloc();
        _lock_instance_info_alloc.unlock();
        nodeInstanceInfoClear( info );
        return info;
    }

    void GraphContext::nodeInstanceInfoFree( NodeInstanceInfo* info )
    {
        if( !info )
            return;

        nodeInstanceInfoRelease( info );

        _lock_instance_info_alloc.lock();
        _alloc_instance_info.free( info );
        _lock_instance_info_alloc.unlock();
    }

    int GraphContext::typeFind( const char* name )
    {
        for( int i = 0; i < array::size( _node_types ); ++i )
        {
            if( string::equal( name, _node_types[i].info->_name ) )
                return i;
        }
        return -1;
    }

    int GraphContext::typeAdd( const NodeTypeInfo* info )
    {
        int found = typeFind( info->_name );
        if( found != -1 )
        {
            bxLogError( "Node type already exists '%s'", info->_name );
            return -1;
        }

        int index = array::push_back( _node_types, NodeType() );
        NodeType& type = array::back( _node_types );
        type.index = index;
        type.info = info;
        //type.info._type_name = string::duplicate( nullptr, info._type_name );
        NodeTypeBehaviour* behaviour = (NodeTypeBehaviour*)&type.info->_behaviour;
        info->_interface->typeInit( behaviour, index );
        //if( info->_type_init )
        //{
        //    ( *info->_type_init )( index );
        //}

        return index;
    }

    void GraphContext::idAcquire( id_t* id )
    {
        _lock_nodes.lock();
        id[0] = id_table::create( _id_table );
        _lock_nodes.unlock();
    }

    void GraphContext::idRelease( id_t id )
    {
        _lock_nodes.lock();
        id_table::destroy( _id_table, id );
        _lock_nodes.unlock();
    }

    void GraphContext::nodeCreate( id_t id, int typeIndex, const char* nodeName )
    {
        NodeType* type = &_node_types[typeIndex];
        //Node* node = ( *type->info->_creator )( );
        Node* node = type->info->_interface->creator();
        SYS_ASSERT( node != nullptr );

        SYS_ASSERT( _nodes[id.index] == nullptr );
        _nodes[id.index] = node;

        NodeInstanceInfo* instance = nodeInstanceInfoAllocate();
        _instance_info[id.index] = instance;
        instance->_type_index = type->index;
        instance->_id = id;
        instance->_type_name = type->info->_name;
        instance->_name = string::duplicate( nullptr, nodeName );

        AttributeInstance::startup( &_attributes[id.index], type->attributes );
    }

    void GraphContext::nodeDestroy( Node* node, NodeInstanceInfo* info, AttributeInstance* attrI )
    {
        const NodeType& type = _node_types[info->_type_index];
        SYS_ASSERT( type.index == info->_type_index );

        AttributeInstance::shutdown( &attrI );

        nodeInstanceInfoFree( info );
        type.info->_interface->destroyer( node );
        //( *type.info->_destroyer )( node );
    }

    Graph* GraphContext::graphAllocate()
    {
        if( _graphs_count >= eMAX_GRAPHS )
        {
            SYS_ASSERT( false );
            return nullptr;
        }
        
        int index = -1;
        {
            bxScopeBenaphore lock( _lock_graphs_alloc );

            for( int i = 0; i < eMAX_GRAPHS; ++i )
            {
                if( _graphs[i] == nullptr )
                {
                    index = i;
                    break;
                }
            }
        }

        if( index == -1 )
        {
            SYS_ASSERT( false );
            return nullptr;
        }

        Graph* g = BX_NEW( bxDefaultAllocator(), Graph );
        _graphs[index] = g;
        _graphs_count += 1;
        return g;
    }

    void GraphContext::graphFree( Graph* graph )
    {
        if( _graphs_count == 0 )
        {
            SYS_ASSERT( false );
            return;
        }

        int index = -1;
        {
            bxScopeBenaphore lock( _lock_graphs_alloc );

            for( int i = 0; i < eMAX_GRAPHS; ++i )
            {
                if( _graphs[i] == graph )
                {
                    index = i;
                    break;
                }
            }
        }

        SYS_ASSERT( index != -1 );

        BX_DELETE0( bxDefaultAllocator(), _graphs[index] );
    }

    void GraphContext::nodesDestroy()
    {
        bool done = false;
        do
        {
            NodeToDestroy ntd;
            _lock_nodes_to_destroy.lock();
            done = array::empty( _nodes_to_destroy );
            if( !done )
            {
                ntd = array::back( _nodes_to_destroy );
                array::pop_back( _nodes_to_destroy );
            }
            _lock_nodes_to_destroy.unlock();

            if( !done )
            {
                nodeDestroy( ntd.node, ntd.info, ntd.attributes );
            }

        } while( !done );

    }

    void GraphContext::graphsLoad( Engine* engine, Scene* scene )
    {
        bool done = false;
        do
        {
            GraphToLoad gtl;
            done = !pop_from_back_with_lock( &gtl, _graphs_to_load, _lock_graphs_to_load );
            if( !done )
            {
                bxAsciiScript sceneScript;
                engine->_camera_script_callback->addCallback( &sceneScript );
                engine->_graph_script_callback->addCallback( &sceneScript );
                engine->_graph_script_callback->_graph = gtl.graph;

                const char* sceneName = gtl.filename;
                bxFS::File scriptFile = engine->resource_manager->readTextFileSync( sceneName );

                if( scriptFile.ok() )
                {
                    bxScene::script_run( &sceneScript, scriptFile.txt );
                }
                scriptFile.release();
                gtl.release();

                gtl.graph->nodesLoad( scene );
            }
        } while( !done );
    }

    void GraphContext::graphsUnload( Scene* scene )
    {
        bool done = false;
        do
        {
            GraphToUnload gtu;
            done = !pop_from_back_with_lock( &gtu, _graphs_to_unload, _lock_graphs_to_unload );

            if( !done )
            {
                gtu.graph->shutdown( gtu.destroyAfterUnload );
                gtu.graph->nodesUnload( scene );
                queue::push_front( _graphs_to_destroy, gtu.graph );
            }

        } while( !done );
    }

    void GraphContext::graphsDestroy()
    {
        while( !queue::empty( _graphs_to_destroy ) )
        {
            Graph* g = queue::back( _graphs_to_destroy );
            queue::pop_back( _graphs_to_destroy );

            //int found = array::find1( array::begin( _graphs ), array::end( _graphs ), array::OpEqual<Graph*>( g ) );
            //SYS_ASSERT( found != -1 );
            //array::erase( _graphs, found );

            graphFree( g );

            //BX_DELETE0( bxDefaultAllocator(), g );
        }
    }

    //////////////////////////////////////////////////////////////////////////

    static GraphContext* _ctx = nullptr;
    GraphContext* graphContext()
    {
        return _ctx;
    }
    //////////////////////////////////////////////////////////////////////////
    void graphContextStartup()
    {
        SYS_ASSERT( _ctx == nullptr );
        _ctx = BX_NEW( bxDefaultAllocator(), GraphContext );
        _ctx->startup();
    }
    void graphContextShutdown()
    {
        if( !_ctx )
            return;

        _ctx->shutdown();
        BX_DELETE0( bxDefaultAllocator(), _ctx );
    }

    void graphContextCleanup( Engine* engine, Scene* scene )
    {
        for( int i = 0; i < GraphContext::eMAX_GRAPHS; ++i )
        {
            Graph* graph = _ctx->_graphs[i];
            graphDestroy( &graph, true );
        }

        graphContextTick( engine, scene );
    }

    void graphContextTick( Engine* engine, Scene* scene )
    {
        _ctx->graphsUnload( scene );
        _ctx->graphsLoad( engine, scene );
        _ctx->graphsDestroy();
        _ctx->nodesDestroy();

        _ctx->nodesLock();
        for( int i = 0; i < GraphContext::eMAX_GRAPHS; ++i )
        {
            if( !_ctx->_graphs[i] )
                continue;

            _ctx->_graphs[i]->tick0( scene );
        }

        for( int i = 0; i < GraphContext::eMAX_GRAPHS; ++i )
        {
            if( !_ctx->_graphs[i] )
                continue;

            _ctx->_graphs[i]->tick1( scene );
        }
        _ctx->nodesUnlock();
    }



}////

namespace bx
{
    //////////////////////////////////////////////////////////////////////////
    bool nodeRegister( const NodeTypeInfo* typeInfo )
    {
        int ires = graphContext()->typeAdd( typeInfo );
        return ( ires == -1 ) ? false : true;
    }

    bool nodeCreate( id_t* out, const char* typeName, const char* nodeName )
    {
        int typeIndex = graphContext()->typeFind( typeName );
        if( typeIndex == -1 )
        {
            bxLogError( "Node type '%s' not found", typeName );
            return false;
        }

        graphContext()->idAcquire( out );
        graphContext()->nodeCreate( *out, typeIndex, nodeName );

        return graphContext()->nodeIsValid( *out );
    }
    void nodeDestroy( id_t* inOut )
    {
        if( !graphContext()->nodeIsValid( *inOut ) )
            return;

        NodeToDestroy ntd;
        ntd.node = graphContext()->_nodes[inOut->index];
        ntd.info = graphContext()->_instance_info[inOut->index];
        ntd.attributes = graphContext()->_attributes[inOut->index];

        graphContext()->_nodes[inOut->index] = nullptr;
        graphContext()->_instance_info[inOut->index] = nullptr;
        graphContext()->_attributes[inOut->index] = nullptr;

        graphContext()->idRelease( *inOut );
        inOut[0] = makeInvalidHandle<id_t>();

        SYS_ASSERT( ntd.info->_graph == nullptr );

        graphContext()->_lock_nodes_to_destroy.lock();
        array::push_back( graphContext()->_nodes_to_destroy, ntd );
        graphContext()->_lock_nodes_to_destroy.unlock();
    }

    bool nodeIsAlive( id_t id )
    {
        return graphContext()->nodeIsValid( id );
    }

    NodeInstanceInfo nodeInstanceInfoGet( id_t id )
    {
        SYS_ASSERT( graphContext()->nodeIsValid( id ) );
        return *graphContext()->_instance_info[id.index];
    }

    Node* nodeInstanceGet( id_t id )
    {
        SYS_ASSERT( graphContext()->nodeIsValid( id ) );
        return graphContext()->_nodes[id.index];
    }
    ////
}

namespace bx
{
    void graphCreate( Graph** graph )
    {
        Graph* g = _ctx->graphAllocate();
        g->startup();
        graph[0] = g;
    }
    void graphDestroy( Graph** graph, bool destroyNodes )
    {
        if( !graph[0] )
            return;

        GraphToUnload gtu;
        gtu.graph = graph[0];
        gtu.destroyAfterUnload = destroyNodes;
        graph[0] = nullptr;

        graphContext()->_lock_graphs_to_unload.lock();
        queue::push_front( graphContext()->_graphs_to_unload, gtu );
        graphContext()->_lock_graphs_to_unload.unlock();

        //graph[0]->shutdown( destroyNodes );
        //BX_DELETE0( bxDefaultAllocator(), graph[0] );
    }
    void graphLoad( Graph* graph, const char* filename )
    {
        (void)graph;
        (void)filename;

        GraphToLoad gtl;
        gtl.set( graph, filename );

        //gtl.graph = graph;
        //gtl.filename = string::duplicate( nullptr, filename );

        graphContext()->_lock_graphs_to_load.lock();
        queue::push_front( graphContext()->_graphs_to_load, gtl );
        graphContext()->_lock_graphs_to_load.unlock();
    }
}////

namespace bx
{
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    namespace
    {
        AttributeIndex nodeAttributeAdd( int typeIndex, const char* name, AttributeType::Enum attributeType, const void* defaultValue )
        {
            NodeType* type = graphContext()->typeGet( typeIndex );
            SYS_ASSERT( type->attributes.find( name ) == -1 );
            AttributeIndex index = type->attributes.add( name, attributeType );
            type->attributes.setDefaultValue( index, defaultValue );
            return index;
        }
        bool nodeAttributeSetByName( id_t id, const char* name, AttributeType::Enum type, const void* data )
        {
            NodeType* nodeType = nodeTypeGet( id );
            int index = nodeType->attributes.find( name );
            if( index != -1 )
            {
                graphContext()->attributesInstanceGet( id )->dataSet( index, type, data );
                return true;
            }
            return false;
        }
    }
    AttributeIndex nodeAttributeAddFloat( int typeIndex, const char* name, float defaultValue )
    {
        return nodeAttributeAdd( typeIndex, name, AttributeType::eFLOAT, &defaultValue );
    }
    AttributeIndex nodeAttributeAddInt( int typeIndex, const char* name, int defaultValue )
    {
        return nodeAttributeAdd( typeIndex, name, AttributeType::eINT, &defaultValue );
    }
    AttributeIndex nodeAttributeAddFloat3( int typeIndex, const char* name, const float3_t& defaultValue )
    {
        return nodeAttributeAdd( typeIndex, name, AttributeType::eFLOAT3, &defaultValue );
    }
    AttributeIndex nodeAttributeAddString( int typeIndex, const char* name, const char* defaultValue )
    {
        NodeType* type = graphContext()->typeGet( typeIndex );
        SYS_ASSERT( type->attributes.find( name ) == -1 );

        AttributeIndex index = type->attributes.add( name, AttributeType::eSTRING );
        type->attributes.setDefaultValueString( index, defaultValue );

        return index;
    }


    bool nodeAttributeFloat( float* out, id_t id, const char* name )
    {
        NodeType* type = nodeTypeGet( id );
        int index = type->attributes.find( name );
        if( index != -1 )
        {
            out[0] = nodeAttributeFloat( id, index );
            return true;
        }

        return false;
    }
    bool nodeAttributeInt( int* out, id_t id, const char* name )
    {
        NodeType* type = nodeTypeGet( id );
        int index = type->attributes.find( name );
        if( index != -1 )
        {
            out[0] = nodeAttributeInt( id, index );
            return true;
        }

        return false;
    }

    bool nodeAttributeVector3( Vector3* out, id_t id, const char* name )
    {
        NodeType* type = nodeTypeGet( id );
        int index = type->attributes.find( name );
        if( index != -1 )
        {
            out[0] = nodeAttributeVector3( id, index );
            return true;
        }

        return false;
    }

    bool nodeAttributeString( const char** out, id_t id, const char* name )
    {
        NodeType* type = nodeTypeGet( id );
        int index = type->attributes.find( name );
        if( index != -1 )
        {
            out[0] = nodeAttributeString( id, index );
            return true;
        }
        return false;
    }

    float nodeAttributeFloat( id_t id, AttributeIndex index )
    {
        float* ptr = (float*)graphContext()->attributesInstanceGet( id )->pointerGet( index, AttributeType::eFLOAT );
        return *ptr;
    }
    int nodeAttributeInt( id_t id, AttributeIndex index )
    {
        int* ptr = (int*)graphContext()->attributesInstanceGet( id )->pointerGet( index, AttributeType::eINT );
        return *ptr;
    }
    Vector3 nodeAttributeVector3( id_t id, AttributeIndex index )
    {
        float3_t* ptr = (float3_t*)graphContext()->attributesInstanceGet( id )->pointerGet( index, AttributeType::eFLOAT3 );
        return Vector3( ptr->x, ptr->y, ptr->z );
    }
    const char* nodeAttributeString( id_t id, AttributeIndex index )
    {
        AttributeType::String* ptr = ( AttributeType::String* )graphContext()->attributesInstanceGet( id )->pointerGet( index, AttributeType::eSTRING );
        return ptr->c_str;
    }

    bool nodeAttributeFloatSet( id_t id, const char* name, float value )
    {
        return nodeAttributeSetByName( id, name, AttributeType::eFLOAT, &value );
    }
    bool nodeAttributeIntSet( id_t id, const char* name, int value )
    {
        return nodeAttributeSetByName( id, name, AttributeType::eINT, &value );
    }
    bool nodeAttributeVector3Set( id_t id, const char* name, const Vector3 value )
    {
        return nodeAttributeSetByName( id, name, AttributeType::eFLOAT3, &value );
    }
    bool nodeAttributeStringSet( id_t id, const char* name, const char* value )
    {
        NodeType* type = nodeTypeGet( id );
        int index = type->attributes.find( name );
        if( index != -1 )
        {
            graphContext()->attributesInstanceGet( id )->stringSet( index, value );
            return true;
        }
        return false;
    }

    void nodeAttributeFloatSet( id_t id, AttributeIndex index, float value )
    {
        SYS_ASSERT( graphContext()->nodeIsValid( id ) );
        graphContext()->attributesInstanceGet( id )->dataSet( index, AttributeType::eFLOAT, &value );
    }
    void nodeAttributeIntSet( id_t id, AttributeIndex index, int value )
    {
        SYS_ASSERT( graphContext()->nodeIsValid( id ) );
        graphContext()->attributesInstanceGet( id )->dataSet( index, AttributeType::eINT, &value );
    }
    void nodeAttributeVector3Set( id_t id, AttributeIndex index, const Vector3 value )
    {
        SYS_ASSERT( graphContext()->nodeIsValid( id ) );
        graphContext()->attributesInstanceGet( id )->dataSet( index, AttributeType::eFLOAT3, &value );
    }
    void nodeAttributeStringSet( id_t id, AttributeIndex index, const char* value )
    {
        SYS_ASSERT( graphContext()->nodeIsValid( id ) );
        graphContext()->attributesInstanceGet( id )->stringSet( index, value );
    }
}///




