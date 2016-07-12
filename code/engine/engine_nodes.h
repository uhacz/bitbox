#pragma once

#include "graph_node.h"
#include "engine_nodes_attributes.h"
#include "engine.h"

namespace bx
{
    struct GfxMeshInstance;

    struct IDagNode : INode
    {
        /// this callback is for child node when linking with parent
        virtual void onParentLink( NodeInstanceInfo parentInstance, NodeInstanceInfo childInstance, Scene* scene ) 
        {
            SceneGraph* sg = scene->scene_graph;
            TransformInstance parentTi = sg->get( parentInstance._id );
            TransformInstance childTi = sg->get( childInstance._id );
            if( sg->isValid( parentTi ) && sg->isValid( childTi ) )
            {
                sg->link( parentTi, childTi );
            }
        }
        virtual void onUnlink( NodeInstanceInfo childInstance, Scene* scene ) 
        {
            SceneGraph* sg = scene->scene_graph;
            TransformInstance ti = sg->get( childInstance._id );
            if( sg->isValid( ti ) )
            {
                sg->unlink( ti );
            }
        }

        virtual void load( Node* node, NodeInstanceInfo instance, Scene* scene ) 
        { 
            scene->scene_graph->create( instance._id, Matrix4::identity() );
        }
        virtual void unload( Node* node, NodeInstanceInfo instance, Scene* scene )
        {
            SceneGraph* sg = scene->scene_graph;
            TransformInstance ti = sg->get( instance._id );
            if( sg->isValid( ti ) )
            {
                sg->destroy( ti );
            }
        }

        void worldMatrixSet( SceneGraph* sg, id_t nodeId, const Matrix4& pose )
        {
            TransformInstance ti = sg->get( nodeId );
            if( sg->isValid( ti ) )
                sg->setWorldPose( ti, pose );
        }
        const Matrix4& worldMatrixGet( SceneGraph* sg, id_t nodeId )
        {
            TransformInstance ti = sg->get( nodeId );
            SYS_ASSERT( sg->isValid( ti ) );
            return sg->worldPose( ti );
        }
        bool worldMatrixGet( Matrix4* pose, SceneGraph* sg, id_t nodeId )
        {
            TransformInstance ti = sg->get( nodeId );
            if( sg->isValid( ti ) )
            {
                pose[0] = sg->worldPose( ti );
                return true;
            }
            return false;
        }

    };


    struct LocatorNode : public Node
    {
        struct Interface : public IDagNode
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

        struct Interface : public IDagNode
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
