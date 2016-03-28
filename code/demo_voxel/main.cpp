#include <system/application.h>
#include <system/window.h>

#include <util/time.h>
#include <util/config.h>
#include <util/common.h>

#include <gdi/gdi_context.h>

#include <voxel/voxel.h>
//#include <voxel/voxel_file.h>
//#include <voxel/voxel_scene.h>
//#include <voxel/voxel_gfx.h>

//#include <gfx/gfx_camera.h>
//#include <gfx/gfx_debug_draw.h>
#include <gfx/gfx_gui.h>

#include <engine/engine.h>
#include "gfx/gfx_debug_draw.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct bxVoxelFramebuffer
{
    enum
    {
        eCOLOR = 0,
        eDEPTH,
        eCOUNT,
    };

    bxGdiTexture textures[eCOUNT];

    int width()  const { return textures[0].width; }
    int height() const { return textures[0].height; }
};

//struct bxVoxel_Scene
//{
//    bxVoxel_Container* _container;
//    bxGfxCamera_Manager* _cameraManager;
//    bxGfxCamera_InputContext cameraInputCtx;
//};

bx::Octree* octree = nullptr;
//static bxVoxelFramebuffer fb;
//static bxVoxel_Scene vxscene;
class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bx::Engine::startup( &_engine );
        bx::Scene::startup( &_scene, &_engine );
        bx::DevCamera::startup( &_dev_camera, &_scene, &_engine );
        
        bx::octreeCreate( &octree, 64.f );

        float start = 2.f;
        float end = 8.f;
        for( float z = start; z <= end; z += 1.f )
        {
            for( float y = start; y <= end; y += 1.f )
            {
                for( float x = start; x <= end; x += 1.f )
                {
                    Vector3 p( x, y, z );
                    bx::octreePointInsert( octree, p, 0xFFFFFFFF );
                }
            }
        }

        //bx::octreePointInsert( octree, Vector3( 8.5f, 8.5f, 9.0f ), 0xff );

        //const int fbWidth = 1920;
        //const int fbHeight = 1080;
        //fb.textures[bxVoxelFramebuffer::eCOLOR] = _engine.gdiDevice->createTexture2D( fbWidth, fbHeight, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
        //fb.textures[bxVoxelFramebuffer::eDEPTH] = _engine.gdiDevice->createTexture2Ddepth( fbWidth, fbHeight, 1, bxGdi::eTYPE_DEPTH32F, bxGdi::eBIND_DEPTH_STENCIL | bxGdi::eBIND_SHADER_RESOURCE );

        //time = 0.f;

        //bxVoxel::_Startup( _engine.gdiDevice, _engine.resourceManager );
        //vxscene._container = bxVoxel::container_new();

        //vxscene._cameraManager = bxGfx::cameraManager_new();

        //bxVoxel_SceneScriptCallback voxelScriptCallback;
        //voxelScriptCallback._container = vxscene._container;
        //voxelScriptCallback._currentId.hash = 0;

        //bxGfxCamera_SceneScriptCallback cameraScriptCallback;
        //cameraScriptCallback._menago = vxscene._cameraManager;

        //bxAsciiScript sceneScript;
        //bxScene::script_addCallback( &sceneScript, "voxel", &voxelScriptCallback );
        //bxScene::script_addCallback( &sceneScript, "camera", &cameraScriptCallback );
        //bxScene::script_addCallback( &sceneScript, "camera_push", &cameraScriptCallback );

        //const char* sceneName = bxConfig::global_string( "scene" );
        //bxFS::File scriptFile = _engine.resourceManager->readTextFileSync( sceneName );

        //if ( scriptFile.ok() )
        //{
        //    bxScene::script_run( &sceneScript, scriptFile.txt );
        //}
        //scriptFile.release();

        //bxVoxel::container_load( _engine.gdiDevice, _engine.resourceManager, vxscene._container );

        return true;
    }
    virtual void shutdown()
    {
        //bxGfx::cameraManager_clear( vxscene._cameraManager );
        //bxGfx::cameraManager_delete( &vxscene._cameraManager );

        //bxVoxel::container_unload( _engine.gdiDevice, vxscene._container );
        //bxVoxel::container_delete( &vxscene._container );
        //bxVoxel::_Shutdown( _engine.gdiDevice, _engine.resourceManager );
        //
        //for ( int ifb = 0; ifb < bxVoxelFramebuffer::eCOUNT; ++ifb )
        //{
        //    _engine.gdiDevice->releaseTexture( &fb.textures[ifb] );
        //}
        bx::octreeDestroy( &octree );

        bx::DevCamera::shutdown( &_dev_camera, &_engine );
        bx::Scene::shutdown( &_scene, &_engine );
        bx::Engine::shutdown( &_engine );
    }
    virtual bool update( u64 deltaTimeUS )
    {
        bxWindow* win = bxWindow_get();
        const bxInput* input = bxInput_get();

        if ( bxInput_isKeyPressedOnce( &input->kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        rmt_BeginCPUSample( FRAME );

        const double deltaTimeS = bxTime::toSeconds( deltaTimeUS );
        const float deltaTime = (float)deltaTimeS;
        bxGfxGUI::newFrame( (float)deltaTimeS );

        {
            ImGui::Begin( "System" );
            ImGui::Text( "deltaTime: %.5f", deltaTime );
            ImGui::Text( "FPS: %.5f", 1.f / deltaTime );
            ImGui::End();
        }

        _dev_camera.tick( input, deltaTime );

        //bxGfxCamera_InputContext* cameraInputCtx = &vxscene.cameraInputCtx;

        //bxGfx::cameraUtil_updateInput( cameraInputCtx, &win->input, 0.1f, deltaTime );
        //bxGfxCamera* topCamera = bxGfx::camera_get( vxscene._cameraManager, bxGfx::camera_top( vxscene._cameraManager ) );
        //topCamera->matrix.world = bxGfx::cameraUtil_movement( topCamera->matrix.world
        //                                                      , cameraInputCtx->leftInputX * 0.5f
        //                                                      , cameraInputCtx->leftInputY * 0.5f
        //                                                      , cameraInputCtx->rightInputX * deltaTime * 10.f
        //                                                      , cameraInputCtx->rightInputY * deltaTime * 10.f
        //                                                      , cameraInputCtx->upDown * 0.5f );

        //bxGfx::cameraManager_update( vxscene._cameraManager, deltaTime );

        
        //const bxGfxCamera& currentCamera = bxGfx::camera_current( vxscene._cameraManager );
        bx::GfxCamera* camera = _engine.camera_manager->stack()->top();
        bx::gfxCameraComputeMatrices( camera );

        {
            bx::phxSceneSync( _scene.phx );
        }

        { /// game update
            //octreeDebugDraw( octree );

            const Matrix4 cameraWorld = gfxCameraWorldMatrixGet( camera );
            {
                const Matrix4 objWorld = Matrix4::identity();
                const Matrix4 cameraInObj = inverse( objWorld ) * cameraWorld;
                
                const Vector3 cameraX = cameraInObj.getCol0().getXYZ();
                const Vector3 cameraY = cameraInObj.getCol1().getXYZ();
                const Vector3 cameraZ = -cameraInObj.getCol2().getXYZ();
                
                const Vector4 rootPosSize = octreeNodePosSize( octree, 0 );
                const Vector3 center = rootPosSize.getXYZ();
                const floatInVec ext = ( rootPosSize.getW() * halfVec );

                Matrix3 objWorldRot = objWorld.getUpper3x3();

               
                const Vector3 objExtX = Vector3( ext, zeroVec, zeroVec );
                const Vector3 objExtY = Vector3( zeroVec, ext, zeroVec );
                const Vector3 objExtZ = Vector3( zeroVec, zeroVec, ext );

                const floatInVec extX = sum( absPerElem( Vector3( dot( objExtX, cameraX ), dot( objExtY, cameraX ), dot( objExtZ, cameraX ) ) ) );
                const floatInVec extY = sum( absPerElem( Vector3( dot( objExtX, cameraY ), dot( objExtY, cameraY ), dot( objExtZ, cameraY ) ) ) );
                const floatInVec extZ = sum( absPerElem( Vector3( dot( objExtX, cameraZ ), dot( objExtY, cameraZ ), dot( objExtZ, cameraZ ) ) ) );
                                
                Vector3 e( extX, extY, extZ );
                Vector3 a = center - cameraX*extX - cameraY*extY;
                Vector3 b = center + cameraX*extX - cameraY*extY;
                Vector3 c = center + cameraX*extX + cameraY*extY;
                Vector3 d = center - cameraX*extX + cameraY*extY;

                bxGfxDebugDraw::addLine( a, b, 0x666666FF, 1 );
                bxGfxDebugDraw::addLine( b, c, 0x666666FF, 1 );
                bxGfxDebugDraw::addLine( c, d, 0x666666FF, 1 );
                bxGfxDebugDraw::addLine( d, a, 0x666666FF, 1 );

                Vector3 ad = d - a;
                Vector3 ab = b - a;

                //float w = rootPosSize.getW();
                float xscale = ::abs( dot( cameraZ, Vector3::xAxis() ).getAsFloat() );
                xscale = cubicpulse( 0.5f, 0.5f, xscale );

                float yscale = ::abs( dot( cameraZ, Vector3::yAxis() ).getAsFloat() );
                yscale = cubicpulse( 0.5f, 0.5f, yscale );

                const float adLen = length( ad ).getAsFloat();
                const float abLen = length( ab ).getAsFloat();
                const float xstep = 0.5f; //lerp( xscale, 1.f, ::sqrt( 2.f ) );
                const float ystep = 0.5f; //lerp( yscale, 1.f, ::sqrt( 2.f ) );
                
                float iy = ystep * 0.5f;
                const Vector3 ro = cameraWorld.getTranslation();
                rmt_BeginCPUSample( raycast );
                while( iy <= adLen )
                {
                    Vector3 yoffset = cameraY * iy;
                    float ix = xstep * 0.5f;
                    while( ix <= abLen )
                    {
                        Vector3 rayEnd = a;
                        rayEnd += cameraX * ix;
                        rayEnd += yoffset;

                        //
                        const Vector3 rd = normalize( rayEnd - ro );
                        int nodeIndex = octreeRaycast( octree, ro, rd );
                        if( nodeIndex != -1 )
                        {
                            //bxGfxDebugDraw::addSphere( Vector4( rayEnd, 0.1f ), 0x666666FF, 1 );
                            Vector4 nodePosSize = octreeNodePosSize( octree, nodeIndex );
                            bxGfxDebugDraw::addSphere( Vector4( nodePosSize.getXYZ(), nodePosSize.getW() * halfVec ), 0x000000FF, true );
                        }
                        ix += xstep;
                    }
                    iy += ystep;
                }
                rmt_EndCPUSample();



                //bxGfxDebugDraw::addSphere( Vector4( a, 0.5f ), 0x666666FF, 1 );
                //bxGfxDebugDraw::addSphere( Vector4( b, 0.5f ), 0x666666FF, 1 );
            }
            
            //const Vector3 ro = cameraWorld.getTranslation();
            //const Vector3 rd = -cameraWorld.getCol2().getXYZ();
            //rmt_BeginCPUSample( raycast );
            //int nodeIndex = octreeRaycast( octree, ro, rd );
            //rmt_EndCPUSample();

            

            bxGfxDebugDraw::addAxes( appendScale( Matrix4::identity(), Vector3( 5.f ) ) );
        }

        {
            bx::phxSceneSimulate( _scene.phx, deltaTime );
        }

        bx::gfxContextTick( _engine.gfx_context, _engine.gdi_device );

        bx::GfxCommandQueue* cmdq = nullptr;
        bx::gfxCommandQueueAcquire( &cmdq, _engine.gfx_context, _engine.gdi_context );
        bx::gfxContextFrameBegin( _engine.gfx_context, _engine.gdi_context );

        bx::gfxSceneDraw( _scene.gfx, cmdq, camera );

        bxGfxGUI::draw( _engine.gdi_context );
        
        bx::gfxContextFrameEnd( _engine.gfx_context, _engine.gdi_context );
        bx::gfxCommandQueueRelease( &cmdq );


        //{
        //    bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::xAxis(), 0xFF0000FF, true );
        //    bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::yAxis(), 0x00FF00FF, true );
        //    bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3::zAxis(), 0x0000FFFF, true );
        //}


        //gdiContext->clear();
        //gdiContext->changeRenderTargets( &fb.textures[0], 1, fb.textures[bxVoxelFramebuffer::eDEPTH] );
        //gdiContext->clearBuffers( 0.f, 0.f, 0.f, 0.f, 1.f, 1, 1 );
        //bxGdi::context_setViewport( gdiContext, fb.textures[0] );

        //bxVoxel::gfx_displayListBuild( vxscene._container, currentCamera );
        //bxVoxel::gfx_displayListDraw( gdiContext, vxscene._container, currentCamera );


        //bxGfxDebugDraw::flush( gdiContext, currentCamera.matrix.viewProj );
        //bxGfx::rasterizeFramebuffer( gdiContext, fb.textures[bxVoxelFramebuffer::eCOLOR], currentCamera );
        
        time += deltaTime;

        rmt_EndCPUSample();
        return true;
    }
    float time = 0.f;
    bx::Engine _engine;
    bx::Scene _scene;
    bx::DevCamera _dev_camera;
};

int main( int argc, const char* argv[] )
{
    bxWindow* window = bxWindow_create( "demo", 1280, 720, false, 0 );
    if ( window )
    {
        bxDemoApp app;
        if ( bxApplication_startup( &app, argc, argv ) )
        {
            bxApplication_run( &app );
        }

        bxApplication_shutdown( &app );
        bxWindow_release();
    }


    return 0;
}