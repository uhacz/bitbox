#pragma once

#include "graph_node.h"
#include "engine_nodes_attributes.h"

namespace bx
{
    struct GfxMeshInstance;

    struct LocatorNode : public Node
    {
        struct Interface : public NodeTypeInterface
        {
            BX_GRAPH_NODE_STD_CREATOR_AND_DESTROYER( LocatorNode )

            void typeInit( NodeTypeBehaviour* behaviour, int typeIndex ) override;
            void load( Node* node, NodeInstanceInfo instance, Scene* scene ) override;
            void unload( Node* node, NodeInstanceInfo instance, Scene* scene ) override;
        };

        BX_GRAPH_DECLARE_NODE( LocatorNode, Interface );
        BX_LOCATORNODE_ATTRIBUTES_DECLARE;
    };

    //////////////////////////////////////////////////////////////////////////
    struct MeshNode : public Node
    {
        GfxMeshInstance* _mesh_instance = nullptr;

        struct Interface : public NodeTypeInterface
        {
            BX_GRAPH_NODE_STD_CREATOR_AND_DESTROYER( MeshNode )

            void typeInit( NodeTypeBehaviour* behaviour, int typeIndex ) override;
            void load( Node* node, NodeInstanceInfo instance, Scene* scene )  override;
            void unload( Node* node, NodeInstanceInfo instance, Scene* scene )  override;
        };

        BX_GRAPH_DECLARE_NODE( MeshNode, Interface );
        BX_MESHNODE_ATTRIBUTES_DECLARE;
    };

}///
