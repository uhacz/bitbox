#include "engine_nodes.h"
#include <util/string_util.h>
#include <gfx/gfx.h>
#include "engine.h"

namespace bx
{

    BX_GRAPH_DEFINE_NODE( LocatorNode, LocatorNode::Interface );
    BX_LOCATORNODE_ATTRIBUTES_DEFINE

    void LocatorNode::Interface::typeInit( NodeTypeBehaviour* behaviour, int typeIndex )
    {
        BX_LOCATORNODE_ATTRIBUTES_CREATE;
        behaviour->exec_mask = EExecMask::eLOAD_UNLOAD;
        behaviour->is_dag = 1;
    }

    void LocatorNode::Interface::load( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        IDagNode::load( node, instance, scene );

        const Vector3 pos = nodeAttributeVector3( instance._id, attr_pos );
        const Vector3 rot = nodeAttributeVector3( instance._id, attr_rot );
        const Vector3 scale = nodeAttributeVector3( instance._id, attr_scale );
        const Matrix4 pose = appendScale( Matrix4( Matrix3::rotationZYX( rot ), pos ), scale );
        
        worldMatrixSet( scene->scene_graph, instance._id, pose );

        //NodeUtil::addToSceneGraph( instance, pose );
    }

    void LocatorNode::Interface::unload( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        IDagNode::unload( node, instance, scene );
        //NodeUtil::removeFromSceneGraph( instance );
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    BX_GRAPH_DEFINE_NODE( MeshNode, MeshNode::Interface );
    BX_MESHNODE_ATTRIBUTES_DEFINE

    void MeshNode::Interface::typeInit( NodeTypeBehaviour* behaviour, int typeIndex )
    {
        BX_MESHNODE_ATTRIBUTES_CREATE;
        behaviour->exec_mask = EExecMask::eLOAD_UNLOAD;
        behaviour->is_dag = 1;
    }

    void MeshNode::Interface::load( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        IDagNode::load( node, instance, scene );

        GfxMeshInstance* mi = nullptr;
        gfxMeshInstanceCreate( &mi, gfxContextGet( scene->gfx ) );

        const char* mesh = nodeAttributeString( instance._id, attr_mesh );
        bxGdiRenderSource* rsource = gfxGlobalResourcesGet()->mesh.box;
        if( mesh[0] == ':' )
        {
            if( string::equal( mesh, ":box" ) )
                rsource = gfxGlobalResourcesGet()->mesh.box;
            else if( string::equal( mesh, ":sphere" ) )
                rsource = gfxGlobalResourcesGet()->mesh.sphere;
        }

        const char* material = nodeAttributeString( instance._id, attr_material );
        bxGdiShaderFx_Instance* fxI = gfxMaterialFind( material );
        if( !fxI )
        {
            fxI = gfxMaterialFind( "red" );
        }

        GfxMeshInstanceData miData;
        miData.renderSourceSet( rsource );
        miData.fxInstanceSet( fxI );
        miData.locaAABBSet( Vector3( -0.5f ), Vector3( 0.5f ) );
        gfxMeshInstanceDataSet( mi, miData );

        const Vector3 pos = nodeAttributeVector3( instance._id, attr_pos );
        const Vector3 rot = nodeAttributeVector3( instance._id, attr_rot );
        const Vector3 scale = nodeAttributeVector3( instance._id, attr_scale );
        const Matrix4 pose = appendScale( Matrix4( Matrix3::rotationZYX( rot ), pos ), scale );
        gfxMeshInstanceWorldMatrixSet( mi, &pose, 1 );
        gfxSceneMeshInstanceAdd( scene->gfx, mi );

        self( node )->_mesh_instance = mi;

        worldMatrixSet( scene->scene_graph, instance._id, pose );
    }

    void MeshNode::Interface::unload( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        auto meshNode = self( node );
        gfxMeshInstanceDestroy( &meshNode->_mesh_instance );

        IDagNode::unload( node, instance, scene );
    }
}////
