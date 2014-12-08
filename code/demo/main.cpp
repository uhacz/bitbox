#include <system/application.h>
#include <system/window.h>

#include <gdi/gdi_context.h>
#include <resource_manager/resource_manager.h>
#include <gfx/gfx.h>
#include "util/time.h"

#include <util/handle_manager.h>
#include <gfx/gfx_lights.h>
#include <gfx/gfx_debug_draw.h>

static bxGdiShaderFx* fx = 0;
static bxGdiShaderFx_Instance* fxI = 0;
static bxGdiRenderSource* rsource = 0;
static bxGfxRenderList* rList = 0;
static bxGfxCamera camera;
static bxGfxCamera camera1;
static bxGfxCameraInputContext cameraInputCtx;
static bxGfxLights::PointInstance pointLight0 = { 0 };
static bxGfxLights::PointInstance pointLight1 = { 0 };
static bxGfxLights::PointInstance pointLight2 = { 0 };

class bxDemoApp : public bxApplication
{
public:
    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();
        //_resourceManager = bxResourceManager::startup( "d:/dev/code/bitBox/assets/" );
        _resourceManager = bxResourceManager::startup( "d:/tmp/bitBox/assets/" );
        bxGdi::backendStartup( &_gdiDevice, (uptr)win->hwnd, win->width, win->height, win->full_screen );

        _gdiContext = BX_NEW( bxDefaultAllocator(), bxGdiContext );
        _gdiContext->_Startup( _gdiDevice );

        bxGfxDebugDraw::startup( _gdiDevice, _resourceManager );

        _gfxContext = BX_NEW( bxDefaultAllocator(), bxGfxContext );
        _gfxContext->startup( _gdiDevice, _resourceManager );
        
        _gfxLights = BX_NEW( bxDefaultAllocator(), bxGfxLights );
        _gfxLights->startup( 64 );

        bxGfxLight_Point pl;
        pl.position = float3_t( 0.f, 0.f, 0.f );
        pl.radius = 2.f;
        pl.color = float3_t( 1.f, 1.f, 1.f );
        pl.intensity = 1.f;
        pointLight0 = _gfxLights->createPointLight( pl );

        pl.position.y = 5.f;
        pl.color = float3_t( 0.f, 1.f, 0.f );
        pointLight1 = _gfxLights->createPointLight( pl );

        pl.intensity = 2.f;
        pl.radius = 10.f;
        pointLight2 = _gfxLights->createPointLight( pl );

        _gfxLights->releaseLight( pointLight1 );
        _gfxLights->releaseLight( pointLight2 );

        pl.intensity = 1.f;
        pl.radius = 9.f;
        pointLight1 = _gfxLights->createPointLight( pl );



        fxI = bxGdi::shaderFx_createWithInstance( _gdiDevice, _resourceManager, "native" );
        
        rsource = bxGfxContext::shared()->rsource.box;

        rList = bxGfx::renderList_new( 128, 256, bxDefaultAllocator() );

        camera.matrix.world = Matrix4::translation( Vector3( 0.f, 0.5f, 15.f ) );
        camera1.matrix.world = Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.0f ) ), Vector3(0.f, 0.f, 5.f ) ); //Matrix4::translation( Vector3( 0.f ) );
        camera1.params.zNear = 1.f;
        camera1.params.zFar = 20.f;

        bxGfx::cameraMatrix_compute( &camera1.matrix, camera1.params, camera1.matrix.world, _gfxContext->framebufferWidth(), _gfxContext->framebufferHeight() );

        return true;
    }
    virtual void shutdown()
    {
        bxGfx::renderList_delete( &rList, bxDefaultAllocator() );
        rsource = 0;
        bxGdi::shaderFx_releaseWithInstance( _gdiDevice, &fxI );

        _gfxLights->releaseLight( pointLight1 );
        _gfxLights->releaseLight( pointLight0 );

        _gfxLights->shutdown();
        BX_DELETE0( bxDefaultAllocator(), _gfxLights );

        _gfxContext->shutdown( _gdiDevice, _resourceManager );
        BX_DELETE0( bxDefaultAllocator(), _gfxContext );
        
        bxGfxDebugDraw::shutdown( _gdiDevice, _resourceManager );

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
        if( bxInput_isPeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        bxGfx::cameraUtil_updateInput( &cameraInputCtx, &win->input, 1.f, deltaTime );
        camera.matrix.world = bxGfx::cameraUtil_movement( camera.matrix.world
            , cameraInputCtx.leftInputX
            , cameraInputCtx.leftInputY
            , cameraInputCtx.rightInputX * deltaTime * 10.f
            , cameraInputCtx.rightInputY * deltaTime * 10.f
            , cameraInputCtx.upDown );
        
        
        rList->clear();

        {
            const Matrix4 world[] = 
            {
                Matrix4( Matrix3::rotationZYX( Vector3( 0.1f, 1.f, 0.f ) ), Vector3( -2.f, 0.f, 0.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, 0.f, 0.f ) ), Vector3(  0.f, 2.f, 0.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, 0.f, 0.f ) ), Vector3(  0.f,-2.f, 0.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.1f, 0.f, 1.f ) ), Vector3(  2.f, 0.f, 0.f ) ),
            };
            const bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
            int dataIndex = rList->renderDataAdd( rsource, fxI, 0, bxAABB( Vector3(-0.5f), Vector3(0.5f) ) );
            u32 surfaceIndex = rList->surfacesAdd( &surf, 1 );
            u32 instanceIndex = rList->instancesAdd( world, 4 );
            rList->itemSubmit( dataIndex, surfaceIndex, instanceIndex );
        }

        //bxGfxDebugDraw::addSphere( Vector4( 0.f, 0.f, 0.f, 1.f ), 0xFF0000FF, true );
        //bxGfxDebugDraw::addSphere( Vector4( 4.f, 0.f, 0.f, 0.5f ), 0xFF00FFFF, true );
        //bxGfxDebugDraw::addSphere( Vector4( -4.f, 0.f, 0.f, 0.25f ), 0xFF0000FF, true );
        //bxGfxDebugDraw::addSphere( Vector4( 0.f, 0.f,-4.f, 0.75f ), 0xFF0F00FF, true );
        //bxGfxDebugDraw::addBox( Matrix4( Matrix3::rotationZYX( Vector3( 0.2f, 1.f, 0.5f ) ), Vector3(0.f,0.f,1.f) ), Vector3( 0.5f ), 0x00FF00FF, true );
        //bxGfxDebugDraw::addBox( Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, 0.f, 0.75f ) ), Vector3( 3.f, 0.f, 1.f ) ), Vector3( 0.5f ), 0x11FF00FF, true );
        //bxGfxDebugDraw::addBox( Matrix4( Matrix3::rotationZYX( Vector3( 0.3f, .2f, 0.15f ) ), Vector3( 0.f, 0.f,-1.f ) ), Vector3( 0.15f ), 0x66FF00FF, true );
        //bxGfxDebugDraw::addBox( Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, .7f, 0.35f ) ), Vector3( 0.f, 3.f, 1.f ) ), Vector3( 0.25f ), 0x99FF00FF, true );
        //bxGfxDebugDraw::addBox( Matrix4( Matrix3::rotationZYX( Vector3( 0.9f, 0.f, 0.0f ) ), Vector3( 0.f, -3.f, 1.f ) ), Vector3( 0.35f ), 0xFFFF00FF, true );

        //bxGfxDebugDraw::addLine( Vector3( 0.f ), Vector3( 0.f, 1.f, 0.f ), 0xFF0000FF, true );
        //bxGfxDebugDraw::addLine( Vector3( 0.f,1.f,0.f ), Vector3( 1.f, 1.f, 0.f ), 0x00FF00FF, true );
        //bxGfxDebugDraw::addLine( Vector3( 1.f, 1.f, 0.f ), Vector3( 1.f, 1.f, 1.f ), 0x0000FFFF, true );
        //bxGfxDebugDraw::addLine( Vector3( 1.f, 1.f, 1.f ), Vector3( 1.f,-1.f, 1.f ), 0xFF6666FF, true );
        //bxGfxDebugDraw::addLine( Vector3( 1.f, -1.f, 1.f ), Vector3( 1.f, -1.f,-1.f ), 0xFFFFFFFF, true );

        const Vector4 sphere( -5.f, -1.f, -5.f, 1.0f );
        bxGfxDebugDraw::addSphere( sphere, 0xFFFF00FF, true );

        bxGfx::viewFrustum_debugDraw( camera1.matrix.viewProj, 0x00FF00FF );
        {
            const Matrix4 projInv = inverse( camera1.matrix.viewProj );
            const int tileSize = 128;
            const int numTilesX = iceil( 1920, tileSize );
            const int numTilesY = iceil( 1080, tileSize );

            const u32 colors[] =
            {
                0xFF0000FF, 0x00FF00FF, 0x0000FFFF,
                0xFFFF00FF, 0x00FFFFFF, 0xFFFFFFFF,
            };

            for ( int itileY = 3; itileY < 4; ++itileY )
            {
                for( int itileX = 3; itileX < 4; ++itileX )
                {
                    bxGfxViewFrustum vF = bxGfx::viewFrustum_tile( projInv, itileX, itileY, numTilesX, numTilesY, tileSize );
                    int collision = bxGfx::viewFrustum_SphereIntersectLRTB( vF, sphere );
                    //if( collision )
                    {
                        Vector3 corners[8];
                        bxGfx::viewFrustum_computeTileCorners( corners, projInv, itileX, itileY, numTilesX, numTilesY, tileSize );
                        bxGfx::viewFrustum_debugDraw( corners, colors[(numTilesX*itileY + itileX) % 6] );
                    }
                }

            }

        }
        //bxGdiContextBackend* gdiContext = _gdiDevice->ctx;

        //float clearColor[5] = { 1.f, 0.f, 0.f, 1.f, 1.f };
        //_gdiContext->clearBuffers( clearColor, 1, 1 );
        //gdiContext->clearBuffers( 0, 0, bxGdiTexture(), clearColor, 1, 1 );

        bxGfx::cameraMatrix_compute( &camera.matrix, camera.params, camera.matrix.world, _gfxContext->framebufferWidth(), _gfxContext->framebufferHeight() );

        _gfxContext->frameBegin( _gdiContext );

        _gfxContext->frameDraw( _gdiContext, camera, &rList, 1 );

        bxGfxDebugDraw::flush( _gdiContext, camera.matrix.viewProj );
        
        _gfxContext->rasterizeFramebuffer( _gdiContext, camera );
        _gfxContext->frameEnd( _gdiContext );

        return true;
    }

    bxGdiDeviceBackend* _gdiDevice;
    bxGdiContext* _gdiContext;
    bxGfxContext* _gfxContext;
    bxGfxLights* _gfxLights;
    bxResourceManager* _resourceManager;

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