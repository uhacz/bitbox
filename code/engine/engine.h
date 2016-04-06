#pragma once


#include <resource_manager/resource_manager.h>
#include <gdi/gdi_context.h>
#include <phx/phx.h>
#include <gfx/gfx.h>

#include "profiler.h"
#include "camera_manager.h"

struct bxInput;

namespace bx
{
    struct Engine
    {
        bxGdiDeviceBackend*   gdi_device = nullptr;
        bxGdiContext*         gdi_context = nullptr;
        bxResourceManager*    resource_manager = nullptr;
        CameraManager*        camera_manager = nullptr;

        GfxContext*       gfx_context = nullptr;
        PhxContext*       phx_context = nullptr;
        
        //// tool for profiling
        Remotery* _remotery = nullptr;

        ////
        CameraManagerSceneScriptCallback* _camera_script_callback;

        static void startup( Engine* e );
        static void shutdown( Engine* e );
    };
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    struct Scene
    {
        bx::GfxScene* gfx = nullptr;
        bx::PhxScene* phx = nullptr;

        static void startup( Scene* scene, Engine* engine );
        static void shutdown( Scene* scene, Engine* engine );
    };
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    struct DevCamera
    {
        GfxCamera* camera = nullptr;
        gfx::CameraInputContext input_ctx;

        void tick( const bxInput* input, float deltaTime );

        static void startup( DevCamera* devCamera, Scene* scene, Engine* engine );
        static void shutdown( DevCamera* devCamera, Engine* engine );
    };

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    struct Node {};
    struct Graph;

    struct NodeInstanceInfo
    {
        i32 _type_index;
        id_t _instance_id;

        const char* _type_name;
        const char* _instance_name;

        Graph* _graph;
    };

    struct NodeTypeInfo
    {
        typedef void( *TypeInit )( );
        typedef void( *TypeDeinit )( );
        typedef Node* ( *Creator )( );
        typedef void ( *Destroyer )( Node* node );
        typedef void( *Load )( Node* node, NodeInstanceInfo instance, Scene* scene );
        typedef void( *Unload )( Node* node, NodeInstanceInfo instance, Scene* scene );
        typedef void( *Tick )( Node* node, NodeInstanceInfo instance, Scene* scene );

        const char* _type_name = nullptr;

        TypeInit _type_init = nullptr;
        TypeDeinit _type_deinit = nullptr;
        Destroyer _destroyer = nullptr;
        Creator _creator = nullptr;
        Load _load = nullptr;
        Unload _unload = nullptr;
        Tick _tick = nullptr;
    };

    void graphContextStartup();
    void graphContextShutdown();
    void graphContextTick( Scene* scene );
    bool nodeRegister( const NodeTypeInfo* typeInfo );

    void graphCreate( Graph** graph );
    void graphDestroy( Graph** graph, bool destroyNodes = true );
    void graphLoad( Graph* graph, const char* filename );
    bool graphNodeAdd( Graph* graph, id_t id );
    void graphNodeRemove( id_t id, bool destroyNode = false );
    void graphNodeLink( id_t parent, id_t child );
    void graphNodeUnlink( id_t child );

    bool nodeCreate( id_t* out, const char* typeName, const char* nodeName );
    void nodeDestroy( id_t* inOut );
    bool nodeIsAlive( id_t id );
    NodeInstanceInfo nodeInstanceInfoGet( id_t id );
    Node* nodeInstanceGet( id_t id );
}///

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
namespace bx
{
    struct GfxMeshInstance;

    struct MeshNode : public Node
    {
        GfxMeshInstance* _mesh_instance = nullptr;

        static void _TypeInit();
        static void _TypeDeinit();
        static Node* _Creator();
        static void _Destroyer( Node* node );
        static void _Load( Node* node, NodeInstanceInfo instance, Scene* scene );
        static void _Unload( Node* node, NodeInstanceInfo instance, Scene* scene );
        //static void tick( Node* node, NodeInstanceInfo instance, Scene* scene );
        
        //////////////////////////////////////////////////////////////////////////
        ////
        static NodeTypeInfo __typeInfoFill()
        {
            NodeTypeInfo info;
            info._type_name = "Mesh";
            info._type_init = MeshNode::_TypeInit;
            info._type_deinit = MeshNode::_TypeDeinit;
            info._destroyer = MeshNode::_Destroyer;
            info._creator = MeshNode::_Creator;
            info._load = MeshNode::_Load;
            info._unload = MeshNode::_Unload;
            //info._tick = MeshNode::tick;
            return info;
        }
        static NodeTypeInfo __type_info;
        static MeshNode* self( Node* node ) { return (MeshNode*)node; }
    };
}////