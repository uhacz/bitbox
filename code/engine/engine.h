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
    struct Node;
    struct Graph;

    typedef Node* ( *NodeCreator )( );
    typedef void( *NodeInit )( Node* self );
    typedef void( *NodeDeinit )( Node* self );
    typedef void( *NodeLoad )( Node* self, Scene* scene );
    typedef void( *NodeUnload )( Node* self, Scene* scene );
    typedef void( *NodeTick )( Node* self, Scene* scene );

    struct NodeDesc
    {
        const char* _type_name = nullptr;

        NodeCreator _creator = nullptr;
        NodeInit _init = nullptr;
        NodeDeinit _deinit = nullptr;
        NodeLoad _load = nullptr;
        NodeUnload _unload = nullptr;
        NodeTick _tick = nullptr;
    };

    struct NodeInstanceInfo
    {
        u32 _type_id;
        const char* _type_name;
        
        id_t _instance_id;
        const char* _instance_name;

        Graph* _graph = nullptr;
        Node* _parent = nullptr;
        Node* _first_child = nullptr;
        Node* _next_slibling = nullptr;
    };

    void graphGlobalStartup();
    void graphGlobalShutdown();

    void graphCreate( Graph** graph );
    void graphDestroy( Graph** graph );
    void graphTick( Graph* graph );
    bool graphNodeAdd( Graph* graph, Node* node );
    void graphNodeRemove( Node* node );
    bool graphNodeLink( Node* parent, Node* child );
    bool graphNodeUnlink( Node* parent, Node* child );

    bool nodeRegister( NodeDesc* nodeDesc );
    bool nodeCreate( Node** out, const char* typeName );
    void nodeDestroy( Node** intOut );
    void nodeInstanceInfoGet( NodeInstanceInfo* nif, Node* node );



}///