#pragma once


#include <resource_manager/resource_manager.h>
#include <gdi/gdi_context.h>
#include <phx/phx.h>
#include <gfx/gfx.h>

#include <util/ascii_script.h>

#include "profiler.h"
#include "camera_manager.h"


struct bxInput;

namespace bx
{
    struct GraphSceneScriptCallback;
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
        GraphSceneScriptCallback*         _graph_script_callback = nullptr;
        CameraManagerSceneScriptCallback* _camera_script_callback = nullptr;

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
    typedef i32 AttributeIndex;
    struct Node {};
    struct Graph;
    
    struct NodeInstanceInfo
    {
        i32 type_index;
        id_t id;

        const char* type_name;
        const char* name;

        Graph* graph;
    };

    struct NodeTypeInfo
    {
        typedef void( *TypeInit )( int typeIndex );
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

    //////////////////////////////////////////////////////////////////////////
    void graphContextStartup();
    void graphContextShutdown();
    void graphContextCleanup( Scene* scene );
    void graphContextTick( Scene* scene );

    //////////////////////////////////////////////////////////////////////////
    void graphCreate( Graph** graph );
    void graphDestroy( Graph** graph, bool destroyNodes = true );
    void graphLoad( Graph* graph, const char* filename );
    bool graphNodeAdd( Graph* graph, id_t id );
    void graphNodeRemove( id_t id, bool destroyNode = false );
    void graphNodeLink( id_t parent, id_t child );
    void graphNodeUnlink( id_t child );

    //////////////////////////////////////////////////////////////////////////
    bool nodeRegister( const NodeTypeInfo* typeInfo );
    bool nodeCreate( id_t* out, const char* typeName, const char* nodeName );
    void nodeDestroy( id_t* inOut );
    bool nodeIsAlive( id_t id );
    NodeInstanceInfo nodeInstanceInfoGet( id_t id );
    Node* nodeInstanceGet( id_t id );

    //////////////////////////////////////////////////////////////////////////
    AttributeIndex nodeAttributeAddFloat ( int typeIndex, const char* name, float defaultValue );
    AttributeIndex nodeAttributeAddInt   ( int typeIndex, const char* name, int defaultValue );
    AttributeIndex nodeAttributeAddFloat3( int typeIndex, const char* name, const float3_t& defaultValue );
    AttributeIndex nodeAttributeAddString( int typeIndex, const char* name, const char* defaultValue );

    bool nodeAttributeFloat  ( float* out       , id_t id, const char* name );
    bool nodeAttributeInt    ( int* out         , id_t id, const char* name );
    bool nodeAttributeVector3( Vector3* out     , id_t id, const char* name );
    bool nodeAttributeString ( const char** out , id_t id, const char* name );

    float       nodeAttributeFloat  ( id_t id, AttributeIndex index );
    int         nodeAttributeInt    ( id_t id, AttributeIndex index );
    Vector3     nodeAttributeVector3( id_t id, AttributeIndex index );
    const char* nodeAttributeString ( id_t id, AttributeIndex index );

    bool nodeAttributeFloatSet  ( id_t id, const char* name, float value );
    bool nodeAttributeIntSet    ( id_t id, const char* name, int value );
    bool nodeAttributeVector3Set( id_t id, const char* name, const Vector3 value );
    bool nodeAttributeStringSet ( id_t id, const char* name, const char* value );

    void nodeAttributeFloatSet  ( id_t id, AttributeIndex index, float value );
    void nodeAttributeIntSet    ( id_t id, AttributeIndex index, int value );
    void nodeAttributeVector3Set( id_t id, AttributeIndex index, const Vector3 value );
    void nodeAttributeStringSet ( id_t id, AttributeIndex index, const char* value );

    //////////////////////////////////////////////////////////////////////////
    struct GraphSceneScriptCallback : public bxAsciiScript_Callback
    {
        virtual void addCallback( bxAsciiScript* script );
        virtual void onCreate( const char* typeName, const char* objectName );
        virtual void onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData );
        virtual void onCommand( const char* cmdName, const bxAsciiScript_AttribData& args );

        Graph* _graph = nullptr;
        id_t _current_id = makeInvalidHandle<id_t>();
    };
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

        static void _TypeInit( int typeIndex );
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


        static AttributeIndex attr_position;
        static AttributeIndex attr_rotation;
        static AttributeIndex attr_scale;
        static AttributeIndex attr_mesh;
        static AttributeIndex attr_material;
    };
}////