#pragma once


#include <resource_manager/resource_manager.h>
#include <gdi/gdi_context.h>
#include <phx/phx.h>
#include <gfx/gfx.h>

#include <util/ascii_script.h>

#include "profiler.h"
#include "camera_manager.h"
#include <util/vectormath/vectormath.h>


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
    
    enum EExecMask
    {
        eLOAD_UNLOAD = BIT_OFFSET( 0 ),
        eTICK0 = BIT_OFFSET( 1 ),
        eTICK1 = BIT_OFFSET( 2 ),
    };

    struct NodeInstanceInfo
    {
        i32 type_index;
        id_t id;

        const char* type_name;
        const char* name;

        Graph* graph;
    };

    struct NodeTypeInterface
    {
        virtual ~NodeTypeInterface() {} 

        virtual void typeInit( int typeIndex ) { (void)typeIndex; }
        virtual void typeDeinit() {}

        virtual Node* creator() = 0;
        virtual void destroyer( Node* node ) = 0;

        virtual void load( Node* node, NodeInstanceInfo instance, Scene* scene ) { (void)node; (void)instance; (void)scene; }
        virtual void unload( Node* node, NodeInstanceInfo instance, Scene* scene ) { (void)node; (void)instance; (void)scene; }
        virtual void tick0( Node* node, NodeInstanceInfo instance, Scene* scene ) { (void)node; (void)instance; (void)scene; }
        virtual void tick1( Node* node, NodeInstanceInfo instance, Scene* scene ) { (void)node; (void)instance; (void)scene; }
    };

    struct NodeTypeInfo
    {
        NodeTypeInterface* _interface = nullptr;
        const char* _name = nullptr;
        u32 _exec_mask = 0;
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

#define BX_GRAPH_DECLARE_NODE( typeName, interfaceName, execMask )\
    static NodeTypeInfo __type_info; \
    static interfaceName __type_interface; \
    __forceinline static typeName* self( Node* node ) { return (typeName*)node; }\
    static NodeTypeInfo __typeInfoFill()\
    {\
        NodeTypeInfo info;\
        info._interface = &__type_interface;\
        info._name = MAKE_STR(typeName);\
        info._exec_mask = execMask; \
        return info;\
    }

#define BX_GRAPH_DEFINE_NODE( typeName, interfaceName )\
    interfaceName typeName::__type_interface;\
    NodeTypeInfo typeName::__type_info = typeName::__typeInfoFill()
    
#define BX_GRAPH_NODE_STD_CREATOR_AND_DESTROYER( typeName )\
    bx::Node* creator () { return BX_NEW( bxDefaultAllocator(), typeName ); }\
    void destroyer( bx::Node* node ) { BX_DELETE( bxDefaultAllocator(), node ); }

    struct GfxMeshInstance;
    
#include "engine_nodes_attributes.h"
    struct LocatorNode : public Node
    {
        Matrix4 _pose = Matrix4::identity();

        struct Interface : public NodeTypeInterface
        {
            BX_GRAPH_NODE_STD_CREATOR_AND_DESTROYER( LocatorNode )
            
            void typeInit( int typeIndex ) override;
            void load( Node* node, NodeInstanceInfo instance, Scene* scene ) override;
        };

        BX_GRAPH_DECLARE_NODE( LocatorNode, Interface, EExecMask::eLOAD_UNLOAD );
        BX_LOCATORNODE_ATTRIBUTES_DECLARE;
    };
    
    struct MeshNode : public Node
    {
        GfxMeshInstance* _mesh_instance = nullptr;

        struct Interface : public NodeTypeInterface
        {
            BX_GRAPH_NODE_STD_CREATOR_AND_DESTROYER( MeshNode )

            void typeInit( int typeIndex ) override;
            void load( Node* node, NodeInstanceInfo instance, Scene* scene )  override;
            void unload( Node* node, NodeInstanceInfo instance, Scene* scene )  override;
        };

        BX_GRAPH_DECLARE_NODE( MeshNode, Interface, EExecMask::eLOAD_UNLOAD );
        BX_MESHNODE_ATTRIBUTES_DECLARE;
    };
}////