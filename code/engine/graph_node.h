#pragma once

#include <util/containers.h>
#include <util/vectormath/vectormath.h>
#include "scene_graph.h"

namespace bx
{
    struct Scene;
    struct Graph;
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

    //////////////////////////////////////////////////////////////////////////
    AttributeIndex nodeAttributeAddFloat( int typeIndex, const char* name, float defaultValue );
    AttributeIndex nodeAttributeAddInt( int typeIndex, const char* name, int defaultValue );
    AttributeIndex nodeAttributeAddFloat3( int typeIndex, const char* name, const float3_t& defaultValue );
    AttributeIndex nodeAttributeAddString( int typeIndex, const char* name, const char* defaultValue );

    bool nodeAttributeFloat( float* out, id_t id, const char* name );
    bool nodeAttributeInt( int* out, id_t id, const char* name );
    bool nodeAttributeVector3( Vector3* out, id_t id, const char* name );
    bool nodeAttributeString( const char** out, id_t id, const char* name );

    float       nodeAttributeFloat( id_t id, AttributeIndex index );
    int         nodeAttributeInt( id_t id, AttributeIndex index );
    Vector3     nodeAttributeVector3( id_t id, AttributeIndex index );
    const char* nodeAttributeString( id_t id, AttributeIndex index );

    bool nodeAttributeFloatSet( id_t id, const char* name, float value );
    bool nodeAttributeIntSet( id_t id, const char* name, int value );
    bool nodeAttributeVector3Set( id_t id, const char* name, const Vector3 value );
    bool nodeAttributeStringSet( id_t id, const char* name, const char* value );

    void nodeAttributeFloatSet( id_t id, AttributeIndex index, float value );
    void nodeAttributeIntSet( id_t id, AttributeIndex index, int value );
    void nodeAttributeVector3Set( id_t id, AttributeIndex index, const Vector3 value );
    void nodeAttributeStringSet( id_t id, AttributeIndex index, const char* value );
    //////////////////////////////////////////////////////////////////////////
    struct NodeInstanceInfo
    {
        i32 _type_index;
        id_t _id;

        const char* _type_name;
        const char* _name;

        Graph* _graph;

        //SceneGraph* sceneGraph() const;
        //SceneGraph* sceneGraph();
    };

    struct NodeTypeBehaviour
    {
        u16 exec_mask = 0;
        u16 is_dag = 0;
    };

    //////////////////////////////////////////////////////////////////////////
    struct INode
    {
        virtual ~INode() {}

        virtual void typeInit( NodeTypeBehaviour* behaviour, int typeIndex ) { (void)typeIndex; }
        virtual void typeDeinit() {}

        virtual Node* creator() = 0;
        virtual void destroyer( Node* node ) = 0;

        virtual void initialize( Node* node, NodeInstanceInfo instance, Scene* scene ) { (void)node; (void)instance; (void)scene; }
        virtual void deinitialize( Node* node, NodeInstanceInfo instance, Scene* scene ) { (void)node; (void)instance; (void)scene; }

        /// this callback is for child node when linking with parent
        virtual void onParentLink( NodeInstanceInfo parentInstance, NodeInstanceInfo childInstance, Scene* scene ) { 
            (void)parentInstance; (void)childInstance; (void)scene; 
        }
        /// this callback is for parent node when adding child
        virtual void onChildLink( NodeInstanceInfo parentInstance, NodeInstanceInfo childInstance, Scene* scene ) {
            (void)parentInstance; (void)childInstance; (void)scene;
        }
        virtual void onUnlink( NodeInstanceInfo childInstance, Scene* scene ) {
            (void)childInstance; (void)scene;
        }

        virtual void load( Node* node, NodeInstanceInfo instance, Scene* scene ) { (void)node; (void)instance; (void)scene; }
        virtual void unload( Node* node, NodeInstanceInfo instance, Scene* scene ) { (void)node; (void)instance; (void)scene; }
        
        virtual void tick0( Node* node, NodeInstanceInfo instance, Scene* scene ) { (void)node; (void)instance; (void)scene; }
        virtual void tick1( Node* node, NodeInstanceInfo instance, Scene* scene ) { (void)node; (void)instance; (void)scene; }
    };

    //////////////////////////////////////////////////////////////////////////
    struct NodeTypeInfo
    {
        INode* _interface = nullptr;
        const char* _name = nullptr;
        NodeTypeBehaviour _behaviour;
    };

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    bool nodeIsAlive( id_t id );
    NodeInstanceInfo nodeInstanceInfoGet( id_t id );
    Node* nodeInstanceGet( id_t id );

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
#define BX_GRAPH_DECLARE_NODE( typeName, interfaceName )\
    static NodeTypeInfo __type_info; \
    static interfaceName __type_interface; \
    __forceinline static typeName* self( Node* node ) { return (typeName*)node; }\
    static NodeTypeInfo __typeInfoFill()\
    {\
        NodeTypeInfo info;\
        info._interface = &__type_interface;\
        info._name = MAKE_STR(typeName);\
        return info;\
    }

#define BX_GRAPH_DEFINE_NODE( typeName, interfaceName )\
    interfaceName typeName::__type_interface;\
    NodeTypeInfo typeName::__type_info = typeName::__typeInfoFill()

#define BX_GRAPH_NODE_STD_CREATOR_AND_DESTROYER( typeName )\
    bx::Node* creator () { return BX_NEW( bxDefaultAllocator(), typeName ); }\
    void destroyer( bx::Node* node ) { BX_DELETE( bxDefaultAllocator(), node ); }

}///

//namespace bx
//{
//    struct NodeUtil
//    {
//        static TransformInstance addToSceneGraph( const NodeInstanceInfo& nodeInfo, const Matrix4& pose );
//        static void removeFromSceneGraph( const NodeInstanceInfo& nodeInfo );
//    };
//}
