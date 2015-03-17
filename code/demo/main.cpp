#include <system/application.h>
#include <system/window.h>

#include <util/time.h>
#include <util/color.h>
#include <util/handle_manager.h>

#include <resource_manager/resource_manager.h>

#include <gdi/gdi_shader.h>
#include <gdi/gdi_context.h>

#include <gfx/gfx.h>
#include <gfx/gfx_camera.h>
#include <gfx/gfx_lights.h>
#include <gfx/gfx_material.h>
#include <gfx/gfx_gui.h>
#include <gfx/gfx_debug_draw.h>

#include "test_console.h"
#include <util/random.h>
#include <util/perlin_noise.h>
#include <util/array.h>
#include <util/pool_allocator.h>

static const int MAX_LIGHTS = 64;
static const int TILE_SIZE = 32;

#include "entity.h"

static bxGdiRenderSource* rsource = 0;
static bxGfxRenderList* rList = 0;
static bxGfxCamera camera;
static bxGfxCamera camera1;
static bxGfxCamera_InputContext cameraInputCtx;

static bxGfxLightManager::PointInstance pointLights[MAX_LIGHTS];
static int nPointLights = 0;

static bxAABB frustumBBox = bxAABB::prepare();
static bxGfxCamera* currentCamera_ = NULL;


static const int MAX_ENTITIES = 1024;
static bxEntity entities[MAX_ENTITIES];
static int nEntities = 0;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct bxTree_Index
{
    u32 i;
};

struct bxTree
{
    bxTree();

    bxTree_Index create()
    {
        bxTree_Index idx = { 0 };
        
        if( _freeList == UINT_MAX )
        {
            idx.i = array::push_back( _nodes, Node() );
        }
        else
        {
            idx.i = _freeList;
            Node& node = _nodes[_freeList];
            _freeList = node._freeListNext;
            node = Node();            
            node.self = idx.i;
        }

        return idx;
    }
    void release( bxTree_Index* i  )
    {
        
    }
    void link( bxTree_Index parent, bxTree_Index child );
    void unlink( bxTree_Index child );

    u64 data( bxTree_Index i ) const;
    void setData( bxTree_Index i, u64 d );

private:

    union Node
    {
        struct  
        {
            u32 _freeListNext;
        };
        struct  
        {
            u32 self;
            u32 parent;
            u32 next;
            u32 firstChild;
            u64 value;
        };
        
        Node()
            : self( 0xFFFF )
            , parent( 0xFFFF )
            , next( 0xFFFF )
            , firstChild( 0xFFFF )
            , value(0)
        {}
    };
        
    array_t< Node > _nodes;
    u32 _freeList;
};

struct bxTree_Iterator
{
    bxTree_Iterator( bxTree& tree, bxTree_Index start );
    
    bool next( bxTree_Index* nextIndex );
        
private:
    bxTree& _tree;
    bxTree_Index _current;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////





//struct bxSpatialGraph_Handle
//{
//    u32 i;
//};
//
//struct bxSpatialGraph
//{
//    bxSpatialGraph();
//
//    bxSpatialGraph_Handle create();
//    void release( bxSpatialGraph_Handle* h );
//
//    void link( bxSpatialGraph_Handle parent, bxSpatialGraph_Handle child );
//    void unlink( bxSpatialGraph_Handle child );
//
//    
//
//};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//union bxVoxelGrid_Coords
//{
//    u64 hash;
//    struct
//    {
//        u64 x : 22;
//        u64 y : 21;
//        u64 z : 21;
//    };
//};
//
//struct bxVoxelGrid_Item
//{
//    uptr data;
//    u32  next;
//};
//
//struct bxStaticVoxelGrid
//{
//    void create( int nX, int nY, int nZ, bxAllocator* allocator = bxDefaultAllocator() );
//    void release();
//
//    void 
//
//    bxAllocator* _allocator;
//    bxVoxelGrid_Item* _items;
//    i32 _nX;
//    i32 _nY;
//    i32 _nZ;
//};

class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        //testBRDF();
        
        bxWindow* win = bxWindow_get();
        //_resourceManager = bxResourceManager::startup( "d:/dev/code/bitBox/assets/" );
        _resourceManager = bxResourceManager::startup( "d:/tmp/bitBox/assets/" );
        bxGdi::backendStartup( &_gdiDevice, (uptr)win->hwnd, win->width, win->height, win->full_screen );

        _gdiContext = BX_NEW( bxDefaultAllocator(), bxGdiContext );
        _gdiContext->_Startup( _gdiDevice );

        bxGfxGUI::_Startup( _gdiDevice, _resourceManager, win );

        _gfxContext = BX_NEW( bxDefaultAllocator(), bxGfxContext );
        _gfxContext->_Startup( _gdiDevice, _resourceManager );

        bxGfxDebugDraw::_Startup( _gdiDevice, _resourceManager );

        _gfxLights = BX_NEW( bxDefaultAllocator(), bxGfxLights );
        _gfxLights->_Startup( _gdiDevice, MAX_LIGHTS, TILE_SIZE, _gfxContext->framebufferWidth(), _gfxContext->framebufferHeight() );

        _gfxMaterials = BX_NEW( bxDefaultAllocator(), bxGfxMaterialManager );
        _gfxMaterials->_Startup( _gdiDevice, _resourceManager );
        
        _gfxPostprocess = BX_NEW1( bxGfxPostprocess );
        _gfxPostprocess->_Startup( _gdiDevice, _resourceManager, _gfxContext->framebufferWidth(), _gfxContext->framebufferHeight() );

        _componentMesh._Startup();
        _entityManager.register_releaseCallback( bxMeshComponent_Manager::_Callback_releaseEntity, &_componentMesh );

        {
            const u32 colors[] = 
            {
                0xFF0000FF, 0x00FF00FF, 0x0000FFFF,
                0xFFFF00FF, 0x00FFFFFF, 0xFFFFFFFF,
            };
            const int nColors = sizeof(colors) / sizeof(*colors);

            const int gridX = 5;
            const int gridZ = 5;
            const float cellSize = 5.f;
            const float y = 1.5f;
            int counter = 0;
            for ( int iz = 0; iz < gridZ; ++iz )
            {
                const float z = -gridZ * cellSize * 0.5f + iz*cellSize;
                
                for ( int ix = 0; ix < gridX; ++ix )
                {
                    const float x = -gridX * cellSize * 0.5f + ix*cellSize;
                    const Vector3 pos = Vector3( x, y, z );
                    bxGfxLight_Point l;
                    m128_to_xyz( l.position.xyz, pos.get128() );
                    l.radius = 50.f;
                    bxColor::u32ToFloat3( colors[counter%nColors], l.color.xyz );
                    l.intensity = 100000.f;

                    //pointLights[counter] = _gfxLights->lightManager.createPointLight( l );
                    //++nPointLights;
                    ++counter;
                }
            }
        }

        rsource = bxGfxContext::shared()->rsource.sphere;

        rList = bxGfx::renderList_new( 1024 * 4, 1024 * 8, bxDefaultAllocator() );

        camera.matrix.world = Matrix4::translation( Vector3( 0.f, 2.f, 10.f ) );
        camera1.matrix.world = Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.0f ) ), Vector3(0.f, 0.f, 35.f ) ); //Matrix4::translation( Vector3( 0.f ) );
        camera1.params = camera.params;

        bxGfx::cameraMatrix_compute( &camera1.matrix, camera1.params, camera1.matrix.world, _gfxContext->framebufferWidth(), _gfxContext->framebufferHeight() );

        currentCamera_ = &camera;


        bxGdiRenderSource* rsources[] =
        {
            _gfxContext->shared()->rsource.box,
            _gfxContext->shared()->rsource.sphere,
        };
        bxGdiShaderFx_Instance* materials[] = 
        {
            _gfxMaterials->findMaterial( "red" ),
            _gfxMaterials->findMaterial( "green" ),
            _gfxMaterials->findMaterial( "blue" ),
            _gfxMaterials->findMaterial( "white" ),
        };

        const int gridX = 5;
        const int gridY = 5;
        const int gridZ = 5;

        const float cellSize = 2.5f;
        const float yOffset = 4.f;
        const float zOffset = -0.f;

        int counter = 0;
        for( int iz = 0; iz < gridZ; ++iz )
        {
            const float z = -gridZ * cellSize * 0.5f + iz*cellSize + zOffset;
            for( int iy = 0; iy < gridY; ++iy )
            {
                const float y = -gridY * cellSize * 0.5f + iy*cellSize + yOffset;
                for( int ix = 0; ix < gridX; ++ix )
                {
                    const float x = -gridX * cellSize * 0.5f + ix*cellSize;
                    const Vector3 rndOffset = Vector3( bxNoise_perlin( x, y, z ) );
                    const Vector3 rndScale( 1.f );
                    const Matrix4 pose = appendScale( Matrix4( Matrix3::identity(), Vector3( x, y, z ) + rndOffset ), rndScale );


                    bxEntity e = _entityManager.create();
                    bxMeshComponent_Instance cMeshI = _componentMesh.create( e, 1 );

                    bxMeshComponent_Data cMeshData = _componentMesh.mesh( cMeshI );
                    cMeshData.rsource = rsources[nEntities%2];
                    cMeshData.shader = materials[nEntities%3];
                    cMeshData.surface = bxGdi::renderSource_surface( cMeshData.rsource, bxGdi::eTRIANGLES );
                    cMeshData.passIndex = 0;
                    _componentMesh.setMesh( cMeshI, cMeshData );

                    bxComponent_Matrix mx = _componentMesh.matrix( cMeshI );
                    mx.pose[0] = pose;

                    entities[ nEntities++ ] = e;
                }
            }
        }

        {
            const Matrix4 world = appendScale( Matrix4( Matrix3::identity(), Vector3( 0.f, -3.f, 0.0f ) ), Vector3( 100.f, 0.1f, 100.f ) );
            bxGdiRenderSource* box = _gfxContext->shared()->rsource.box;

            bxGdiShaderFx_Instance* fxI = _gfxMaterials->findMaterial( "blue" );
            
            bxEntity e = _entityManager.create();
            bxMeshComponent_Instance cMeshI = _componentMesh.create( e, 1 );

            bxMeshComponent_Data cMeshData = _componentMesh.mesh( cMeshI );
            cMeshData.rsource = box;
            cMeshData.shader = fxI;
            cMeshData.surface = bxGdi::renderSource_surface( cMeshData.rsource, bxGdi::eTRIANGLES );
            cMeshData.passIndex = 0;
            _componentMesh.setMesh( cMeshI, cMeshData );

            bxComponent_Matrix mx = _componentMesh.matrix( cMeshI );
            mx.pose[0] = world;

            entities[ nEntities++ ] = e;
        }

        //{
        //    Matrix4 world = appendScale( Matrix4( Matrix3::identity(), Vector3( -3.f,-1.f, 0.0f ) ), Vector3( 5.f, 5.f, 5.f ) );
        //    bxGdiRenderSource* rsource = _gfxContext->shared()->rsource.sphere;

        //    bxGdiShaderFx_Instance* fxI = _gfxMaterials->findMaterial( "red" );
        //    bxGfxRenderList_ItemDesc itemDesc( rsource, fxI, 0, bxAABB( Vector3(-0.5f), Vector3(0.5f) ) );
        //    bxGfx::renderList_pushBack( rList, &itemDesc, bxGdi::eTRIANGLES, world );

        //    world *= Matrix4::translation( Vector3( 0.f, 0.f, 3.f ) );
        //    itemDesc.setShader( _gfxMaterials->findMaterial( "blue" ), 0 );
        //    itemDesc.setRenderSource( _gfxContext->shared()->rsource.box );
        //    bxGfx::renderList_pushBack( rList, &itemDesc, bxGdi::eTRIANGLES, world );
        //}


        return true;
    }
    virtual void shutdown()
    {
        for( int ie = 0; ie < nEntities; ++ie )
        {
            _entityManager.release( &entities[ie] );
        }
        _componentMesh._Shutdown();

        bxGfx::renderList_delete( &rList, bxDefaultAllocator() );
        rsource = 0;
        {
            for( int ilight = 0; ilight < nPointLights; ++ilight )
            {
                _gfxLights->lightManager.releaseLight( pointLights[ilight] );
            }
            nPointLights = 0;
        }

        _gfxPostprocess->_Shutdown( _gdiDevice, _resourceManager );
        BX_DELETE01( _gfxPostprocess );

        _gfxMaterials->_Shutdown( _gdiDevice, _resourceManager );
        BX_DELETE0( bxDefaultAllocator(), _gfxMaterials );

        _gfxLights->_Shutdown( _gdiDevice );
        BX_DELETE0( bxDefaultAllocator(), _gfxLights );

        bxGfxDebugDraw::_Shutdown( _gdiDevice, _resourceManager );
        
        _gfxContext->shutdown( _gdiDevice, _resourceManager );
        BX_DELETE0( bxDefaultAllocator(), _gfxContext );
        
        bxGfxGUI::shutdown( _gdiDevice, _resourceManager, bxWindow_get() );

        _gdiContext->_Shutdown();
        BX_DELETE0( bxDefaultAllocator(), _gdiContext );

        bxGdi::backendShutdown( &_gdiDevice );
        bxResourceManager::shutdown( &_resourceManager );
    }
    virtual bool update( u64 deltaTimeUS )
    {
        const double deltaTimeS = bxTime::toSeconds( deltaTimeUS );
        const float deltaTime = (float)deltaTimeS;



        bxWindow* win = bxWindow_get();
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        bxGfxGUI::newFrame( (float)deltaTimeS );

        {
            ImGui::Begin( "System" );
            ImGui::Text( "deltaTime: %.5f", deltaTime );
            ImGui::Text( "FPS: %.5f", 1.f / deltaTime );
            ImGui::End();
        }

        _gui_lights.show( _gfxLights, pointLights, nPointLights );
        
        static Matrix4* cameraMatrix = &camera.matrix.world;
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_LSHIFT ) )
        {
            currentCamera_ = (currentCamera_ == &camera) ? &camera1 : &camera;

        }
//            cameraMatrix = ( cameraMatrix == &camera.matrix.world ) ? &camera1.matrix.world : &camera.matrix.world;


        bxGfx::cameraUtil_updateInput( &cameraInputCtx, &win->input, 1.f, deltaTime );
        
        currentCamera_->matrix.world = bxGfx::cameraUtil_movement( currentCamera_->matrix.world
            , cameraInputCtx.leftInputX * 0.25f 
            , cameraInputCtx.leftInputY * 0.25f 
            , cameraInputCtx.rightInputX * deltaTime * 5.f
            , cameraInputCtx.rightInputY * deltaTime * 5.f
            , cameraInputCtx.upDown * 0.25f );
        
        rList->clear();

        bxComponent::mesh_createRenderList( rList, _componentMesh );

        //{
        //    bxRandomGen rnd( 0xBAADF08D );

        //    static float phase = 0.f;
        //    phase = fmodf( phase + deltaTime, 8.f * PI );

        //    const float rot = sinf( phase + deltaTime );
        //    const float posA = sinf( rot ) * 0.2f;
        //    const float posB = cosf( posA * 2 * PI + rot ) * 0.2f;

        //    const int gridX = 5;
        //    const int gridY = 5;
        //    const int gridZ = 5;

        //    const float cellSize = 2.5f;
        //    const float yOffset = 4.f;
        //    const float zOffset = -0.f;
        //    bxGdiRenderSource* rsources[] =
        //    {
        //        _gfxContext->shared()->rsource.box,
        //        _gfxContext->shared()->rsource.sphere,
        //    };
        //    bxGdiShaderFx_Instance* materials[] = 
        //    {
        //        _gfxMaterials->findMaterial( "red" ),
        //        _gfxMaterials->findMaterial( "green" ),
        //        _gfxMaterials->findMaterial( "blue" ),
        //        _gfxMaterials->findMaterial( "white" ),
        //    };
        //    int counter = 0;
        //    for( int iz = 0; iz < gridZ; ++iz )
        //    {
        //        const float z = -gridZ * cellSize * 0.5f + iz*cellSize + zOffset;
        //        for( int iy = 0; iy < gridY; ++iy )
        //        {
        //            const float y = -gridY * cellSize * 0.5f + iy*cellSize + yOffset;
        //            for( int ix = 0; ix < gridX; ++ix )
        //            {
        //                const float x = -gridX * cellSize * 0.5f + ix*cellSize;
        //                const Vector3 rndOffset = bxRand::randomVector3( rnd, Vector3( -1.f ), Vector3( 1.f ) );
        //                const Vector3 rndScale( rnd.getf( 1.f, 2.f ) );
        //                const Matrix4 pose = appendScale( Matrix4( Matrix3::identity(), Vector3( x, y, z ) + rndOffset ), rndScale );
        //                //const Matrix4 pose = Matrix4( Matrix3::identity(), Vector3( x, y, z ) );

        //                bxGdiRenderSource* rs = rsources[counter % 2];
        //                bxGdiShaderFx_Instance* mat = materials[++counter % 3];

        //                bxGfxRenderList_ItemDesc itemDesc( rs, mat, 0, bxAABB( Vector3( -0.5f ), Vector3( 0.5f ) ) );
        //                bxGfx::renderList_pushBack( rList, &itemDesc, bxGdi::eTRIANGLES, &pose, 1 );
        //            }
        //        }
        //    }
        //}

        //{
        //    const Matrix4 world = appendScale( Matrix4( Matrix3::identity(), Vector3( 0.f, -3.f, 0.0f ) ), Vector3( 100.f, 0.1f, 100.f ) );
        //    bxGdiRenderSource* box = _gfxContext->shared()->rsource.box;
        //    
        //    bxGdiShaderFx_Instance* fxI = _gfxMaterials->findMaterial( "blue" );
        //    bxGfxRenderList_ItemDesc itemDesc( box, fxI, 0, bxAABB( Vector3(-0.5f), Vector3(0.5f) ) );
        //    bxGfx::renderList_pushBack( rList, &itemDesc, bxGdi::eTRIANGLES, world );
        //}

        //{
        //    Matrix4 world = appendScale( Matrix4( Matrix3::identity(), Vector3( -3.f,-1.f, 0.0f ) ), Vector3( 5.f, 5.f, 5.f ) );
        //    bxGdiRenderSource* rsource = _gfxContext->shared()->rsource.sphere;

        //    bxGdiShaderFx_Instance* fxI = _gfxMaterials->findMaterial( "red" );
        //    bxGfxRenderList_ItemDesc itemDesc( rsource, fxI, 0, bxAABB( Vector3(-0.5f), Vector3(0.5f) ) );
        //    bxGfx::renderList_pushBack( rList, &itemDesc, bxGdi::eTRIANGLES, world );

        //    world *= Matrix4::translation( Vector3( 0.f, 0.f, 3.f ) );
        //    itemDesc.setShader( _gfxMaterials->findMaterial( "blue" ), 0 );
        //    itemDesc.setRenderSource( _gfxContext->shared()->rsource.box );
        //    bxGfx::renderList_pushBack( rList, &itemDesc, bxGdi::eTRIANGLES, world );
        //}

        bxGfx::cameraMatrix_compute( &camera.matrix, camera.params, camera.matrix.world, _gfxContext->framebufferWidth(), _gfxContext->framebufferHeight() );
        bxGfx::cameraMatrix_compute( &camera1.matrix, camera1.params, camera1.matrix.world, _gfxContext->framebufferWidth(), _gfxContext->framebufferHeight() );
        
        //for( int ilight = 0; ilight < nPointLights; ++ilight )
        //{
        //    bxGfxLight_Point l = _gfxLights->lightManager.pointLight( pointLights[ilight] );

        //    const u32 color = bxColor::float3ToU32( l.color.xyz );
        //    const Vector3 pos( xyz_to_m128( l.position.xyz ) );
        //    bxGfxDebugDraw::addSphere( Vector4( pos, floatInVec( l.radius*0.1f ) ), color, true );
        //}
        //{
        //    Vector3 corners[8];
        //    bxGfx::viewFrustum_extractCorners( corners, camera.matrix.viewProj );
        //    frustumBBox = bxAABB::prepare();
        //    for( int i = 0; i < 8; ++i )
        //    {
        //        frustumBBox = bxAABB::extend( frustumBBox, corners[i] );
        //    }

        //    bxGfxDebugDraw::addBox( Matrix4::translation( bxAABB::center( frustumBBox ) ), bxAABB::size( frustumBBox )*0.5f, 0xFF0000FF, true );
        //}

        _gfxLights->cullLights( camera );
        
        _gfxContext->frame_begin( _gdiContext );
        
        _gfxContext->frame_zPrepass( _gdiContext, *currentCamera_, &rList, 1 );
        {
            bxGdiTexture nrmVSTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_NORMAL_VS );
            bxGdiTexture hwDepthTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH );
            _gfxPostprocess->ssao( _gdiContext, nrmVSTexture, hwDepthTexture );
        }

        _gfxContext->frame_drawShadows( _gdiContext, &rList, 1, *currentCamera_, *_gfxLights );

        _gfxContext->bindCamera( _gdiContext, *currentCamera_ );
        {
            bxGdiTexture outputTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
            _gfxPostprocess->sky( _gdiContext, outputTexture, _gfxLights->sunLight() );
        }

        _gfxLights->bind( _gdiContext );
        _gfxContext->frame_drawColor( _gdiContext, *currentCamera_, &rList, 1, _gfxPostprocess->ssaoOutput() );

        _gdiContext->clear();
        _gfxContext->bindCamera( _gdiContext, *currentCamera_ );
        {
            bxGdiTexture colorTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
            bxGdiTexture outputTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
            bxGdiTexture depthTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH );
            bxGdiTexture shadowTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SHADOWS_VOLUME );
            _gfxPostprocess->fog( _gdiContext, outputTexture, colorTexture, depthTexture, shadowTexture, _gfxLights->sunLight() );
        }
        {
            bxGdiTexture inputTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
            bxGdiTexture outputTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
            _gfxPostprocess->toneMapping( _gdiContext, outputTexture, inputTexture, deltaTime );
        }
        
        {
            bxGdiTexture inputTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
            bxGdiTexture outputTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP );
            _gfxPostprocess->fxaa( _gdiContext, outputTexture, inputTexture );
        }

        {
            bxGdiTexture colorTextures[] = 
            {
                _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SWAP ),
                _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH ),
                _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_NORMAL_VS ),
                _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SHADOWS ),
                _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_SHADOWS_VOLUME ),
                _gfxPostprocess->ssaoOutput(),
                //_gfxShadows->_depthTexture,
            };
            const char* colorNames[] = 
            {
                "color", "depth", "normalsVS" , "shadows", "shadowsVolume", "ssao",// "cascades",
            };
            const int nTextures = sizeof(colorTextures)/sizeof(*colorTextures);
            static int current = 0;
            
            {
                ImGui::Begin();
                ImGui::Combo( "Visible RT", &current, colorNames, nTextures );
                ImGui::End();
            }
            
            bxGdiTexture colorTexture = _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_COLOR );
            _gdiContext->changeRenderTargets( &colorTexture, 1, _gfxContext->framebuffer( bxGfx::eFRAMEBUFFER_DEPTH ) );
            bxGfxDebugDraw::flush( _gdiContext, currentCamera_->matrix.viewProj );
            
            colorTexture = colorTextures[current];
            _gfxContext->frame_rasterizeFramebuffer( _gdiContext, colorTexture, *currentCamera_ );
        }
        
        bxGfxGUI::draw( _gdiContext );
        _gfxContext->frame_end( _gdiContext );

        return true;
    }

    bxGdiDeviceBackend* _gdiDevice;
    bxGdiContext* _gdiContext;
    bxGfxContext* _gfxContext;
    bxGfxLights* _gfxLights;
    bxGfxPostprocess* _gfxPostprocess;
    bxGfxMaterialManager* _gfxMaterials;
    
    bxResourceManager* _resourceManager;

    bxGfxLightsGUI _gui_lights;
    bxGfxShaderFxGUI _gui_shaderFx;

    /// scene stuff
    bxEntity_Manager _entityManager;
    bxMeshComponent_Manager _componentMesh;
};

int main( int argc, const char* argv[] )
{
    bxWindow* window = bxWindow_create( "demo", 1280, 720, false, 0 );
    if( window )
    {
        bxDemoApp app;
        if( bxApplication_startup( &app, argc, argv ) )
        {
            bxApplication_run( &app );
        }

        bxApplication_shutdown( &app );
        bxWindow_release();
    }
    
    
    return 0;
}