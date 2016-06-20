#include "engine_nodes.h"
#include <util/string_util.h>
#include <gfx/gfx.h>
#include "engine.h"

namespace bx
{

    BX_GRAPH_DEFINE_NODE( LocatorNode, LocatorNode::Interface );
    BX_LOCATORNODE_ATTRIBUTES_DEFINE

        void LocatorNode::Interface::typeInit( int typeIndex )
    {
        BX_LOCATORNODE_ATTRIBUTES_CREATE
    }

    void LocatorNode::Interface::load( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        const Vector3 pos = nodeAttributeVector3( instance.id, attr_pos );
        const Vector3 rot = nodeAttributeVector3( instance.id, attr_rot );
        const Vector3 scale = nodeAttributeVector3( instance.id, attr_scale );

        self( node )->_pose = appendScale( Matrix4( Matrix3::rotationZYX( rot ), pos ), scale );
    }

    //void LocatorNode::_TypeInit( int typeIndex )
    //{
    //    BX_LOCATORNODE_ATTRIBUTES_CREATE
    //}
    //void LocatorNode::_TypeDeinit()
    //{}
    //Node* LocatorNode::_Creator()
    //{
    //    return BX_NEW( bxDefaultAllocator(), LocatorNode );
    //}
    //void LocatorNode::_Destroyer( Node* node )
    //{
    //    BX_DELETE( bxDefaultAllocator(), node );
    //}
    //void LocatorNode::_Load( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //    const Vector3 pos = nodeAttributeVector3( instance.id, attr_pos );
    //    const Vector3 rot = nodeAttributeVector3( instance.id, attr_rot );
    //    const Vector3 scale = nodeAttributeVector3( instance.id, attr_scale );

    //    self( node )->_pose = appendScale( Matrix4( Matrix3::rotationZYX( rot ), pos ), scale );
    //}
    //void LocatorNode::_Unload( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //}
    //void LocatorNode::tick( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //}

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    BX_GRAPH_DEFINE_NODE( MeshNode, MeshNode::Interface );
    BX_MESHNODE_ATTRIBUTES_DEFINE

        void MeshNode::Interface::typeInit( int typeIndex )
    {
        BX_MESHNODE_ATTRIBUTES_CREATE
    }

    void MeshNode::Interface::load( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        GfxMeshInstance* mi = nullptr;
        gfxMeshInstanceCreate( &mi, gfxContextGet( scene->gfx ) );

        const char* mesh = nodeAttributeString( instance.id, attr_mesh );
        bxGdiRenderSource* rsource = gfxGlobalResourcesGet()->mesh.box;
        if( mesh[0] == ':' )
        {
            if( string::equal( mesh, ":box" ) )
                rsource = gfxGlobalResourcesGet()->mesh.box;
            else if( string::equal( mesh, ":sphere" ) )
                rsource = gfxGlobalResourcesGet()->mesh.sphere;
        }

        const char* material = nodeAttributeString( instance.id, attr_material );
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

        const Vector3 pos = nodeAttributeVector3( instance.id, attr_pos );
        const Vector3 rot = nodeAttributeVector3( instance.id, attr_rot );
        const Vector3 scale = nodeAttributeVector3( instance.id, attr_scale );

        Matrix4 pose = appendScale( Matrix4( Matrix3::rotationZYX( rot ), pos ), scale );
        gfxMeshInstanceWorldMatrixSet( mi, &pose, 1 );
        gfxSceneMeshInstanceAdd( scene->gfx, mi );

        self( node )->_mesh_instance = mi;
    }

    void MeshNode::Interface::unload( Node* node, NodeInstanceInfo instance, Scene* scene )
    {
        auto meshNode = self( node );
        gfxMeshInstanceDestroy( &meshNode->_mesh_instance );
    }


    //void MeshNode::_TypeInit( int typeIndex )
    //{
    //    BX_MESHNODE_ATTRIBUTES_CREATE
    //}

    //void MeshNode::_TypeDeinit()
    //{}

    //Node* MeshNode::_Creator()
    //{
    //    return BX_NEW( bxDefaultAllocator(), MeshNode );
    //}

    //void MeshNode::_Destroyer( Node* node )
    //{
    //    BX_DELETE( bxDefaultAllocator(), node );
    //}

    //void MeshNode::_Load( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //    GfxMeshInstance* mi = nullptr;
    //    gfxMeshInstanceCreate( &mi, gfxContextGet( scene->gfx ) );

    //    const char* mesh = nodeAttributeString( instance.id, attr_mesh );
    //    bxGdiRenderSource* rsource = gfxGlobalResourcesGet()->mesh.box;
    //    if( mesh[0] == ':' )
    //    {
    //        if( string::equal( mesh, ":box" ) )
    //            rsource = gfxGlobalResourcesGet()->mesh.box;
    //        else if( string::equal( mesh, ":sphere" ) )
    //            rsource = gfxGlobalResourcesGet()->mesh.sphere;
    //    }

    //    const char* material = nodeAttributeString( instance.id, attr_material );
    //    bxGdiShaderFx_Instance* fxI = gfxMaterialFind( material );
    //    if( !fxI )
    //    {
    //        fxI = gfxMaterialFind( "red" );
    //    }
    //           
    //    GfxMeshInstanceData miData;
    //    miData.renderSourceSet( rsource );
    //    miData.fxInstanceSet( fxI );
    //    miData.locaAABBSet( Vector3( -0.5f ), Vector3( 0.5f ) );
    //    gfxMeshInstanceDataSet( mi, miData );

    //    const Vector3 pos = nodeAttributeVector3( instance.id, attr_pos);
    //    const Vector3 rot = nodeAttributeVector3( instance.id, attr_rot);
    //    const Vector3 scale = nodeAttributeVector3( instance.id, attr_scale );

    //    Matrix4 pose = appendScale( Matrix4( Matrix3::rotationZYX( rot ), pos ), scale );
    //    gfxMeshInstanceWorldMatrixSet( mi, &pose, 1 );
    //    gfxSceneMeshInstanceAdd( scene->gfx, mi );

    //    self(node)->_mesh_instance = mi;
    //}

    //void MeshNode::_Unload( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //    auto meshNode = self( node );
    //    gfxMeshInstanceDestroy( &meshNode->_mesh_instance );
    //}

    //void MeshNode::tick( Node* node, NodeInstanceInfo instance, Scene* scene )
    //{
    //
    //}
}////