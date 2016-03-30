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

    struct NodeTypeInfo
    {
        typedef void( *TypeInit )( );
        typedef void( *TypeDeinit )( );
        typedef Node* ( *Creator )( );
        typedef void ( *Destroyer )( Node* node );
        typedef void( *Load )( Node* self, Scene* scene );
        typedef void( *Unload )( Node* self, Scene* scene );
        typedef void( *Tick )( Node* self, Scene* scene );

        const char* _type_name = nullptr;

        TypeInit _type_init = nullptr;
        TypeDeinit _type_deinit = nullptr;
        Destroyer _destroyer = nullptr;
        Creator _creator = nullptr;
        Load _load = nullptr;
        Unload _unload = nullptr;
        Tick _tick = nullptr;
    };

    struct NodeInstanceInfo
    {
        i32 _type_index;
        id_t _instance_id;
        
        const char* _type_name;
        const char* _instance_name;

        Graph* _graph;
        id_t _parent;
        id_t _first_child;
        id_t _next_slibling;
    };

    void graphGlobalStartup();
    void graphGlobalShutdown();
    bool nodeRegister( const NodeTypeInfo& typeInfo );

    void graphCreate( Graph** graph );
    void graphDestroy( Graph** graph );
    void graphTick( Graph* graph );
    bool graphNodeAdd( Graph* graph, id_t id );
    void graphNodeRemove( id_t id );
    bool graphNodeLink( id_t parent, id_t child );
    bool graphNodeUnlink( id_t child );

    bool nodeCreate( id_t* out, const char* typeName, const char* nodeName );
    void nodeDestroy( id_t* inOut );
    bool nodeIsAlive( id_t id );
    NodeInstanceInfo nodeInstanceInfoGet( id_t id );
    Node* nodeInstanceGet( id_t id );
}///