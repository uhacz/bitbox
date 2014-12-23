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
#include <gfx/gfx_debug_draw.h>

#include "test_console.h"

static const int MAX_LIGHTS = 64;
static const int TILE_SIZE = 64;

static bxGdiShaderFx* fx = 0;
static bxGdiShaderFx_Instance* fxI = 0;
static bxGdiRenderSource* rsource = 0;
static bxGfxRenderList* rList = 0;
static bxGfxCamera camera;
static bxGfxCamera camera1;
static bxGfxCameraInputContext cameraInputCtx;
//static bxGfxLightManager::PointInstance pointLight0 = { 0 };
//static bxGfxLightManager::PointInstance pointLight1 = { 0 };
//static bxGfxLightManager::PointInstance pointLight2 = { 0 };
static bxGfxLightManager::PointInstance pointLights[MAX_LIGHTS];
static int nPointLights = 0;
//static bxGfxLightList* lList = 0;
//static bxGfxViewFrustum_Tiles* frustumTiles = 0;


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

        bxGfxDebugDraw::startup( _gdiDevice, _resourceManager );

        _gfxContext = BX_NEW( bxDefaultAllocator(), bxGfxContext );
        _gfxContext->startup( _gdiDevice, _resourceManager );
        
        _gfxLights = BX_NEW( bxDefaultAllocator(), bxGfxLights );
        _gfxLights->startup( _gdiDevice, MAX_LIGHTS, TILE_SIZE, _gfxContext->framebufferWidth(), _gfxContext->framebufferHeight() );

        {
            
            //const float r = 5.f;

            //bxPolyShape shape;
            //bxPolyShape_createShpere( &shape, 2 );

            const u32 colors[] = 
            {
                0xFF0000FF, 0x00FF00FF, 0x0000FFFF,
                0xFFFF00FF, 0x00FFFFFF, 0xFFFFFFFF,
            };
            const int nColors = sizeof(colors) / sizeof(*colors);

            //for( int ivertex = 0; ivertex < 1; ++ivertex )
            //{
            //    const float* vertex = shape.position( ivertex );

            //    const Vector3 v( vertex[0], vertex[1], vertex[2] );

            //    const Vector3 pos = normalize( v ) * r;

            //    bxGfxLight_Point l;
            //    m128_to_xyz( l.position.xyz, pos.get128() );
            //    l.radius = 9.5f;
            //    bxColor::u32ToFloat3( colors[ivertex%nColors], l.color.xyz );
            //    l.intensity = 1.f;

            //    pointLights[ivertex] = _gfxLights->lightManager.createPointLight( l );
            //    ++nPointLights;
            //}

            const float a = 5.f;
            const Vector3 corners[] =
            {
                //Vector3( -a, -0.f, -a ),
                //Vector3(  a, -0.f, -a ),
                //Vector3(  a,  a, -a ),
                //Vector3( -a,  a, -a ),

                //Vector3( -a, -0.f, a ),
                //Vector3(  a, -0.f, a ),
                Vector3(  a,  a, a ),
                Vector3( -a,  a, a ),
            };
            const int nCorners = sizeof( corners ) / sizeof( *corners );
            for ( int icorner = 0; icorner < nCorners; ++icorner )
            {
                bxGfxLight_Point l;
                m128_to_xyz( l.position.xyz, corners[icorner].get128() );
                l.radius = 15.f;
                bxColor::u32ToFloat3( colors[icorner%nColors], l.color.xyz );
                l.intensity = 15.f;
                pointLights[icorner] = _gfxLights->lightManager.createPointLight( l );
                ++nPointLights;
            }

            

            
            //bxPolyShape_deallocateShape( &shape );
        }



        fxI = bxGdi::shaderFx_createWithInstance( _gdiDevice, _resourceManager, "native" );
        fxI->setUniform( "fresnel_coeff", 0.95f );
        fxI->setUniform( "rough_coeff", 0.152f );

        rsource = bxGfxContext::shared()->rsource.sphere;

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

        {
            for( int ilight = 0; ilight < nPointLights; ++ilight )
            {
                _gfxLights->lightManager.releaseLight( pointLights[ilight] );
            }
            nPointLights = 0;
        }

        //_gfxLights->lightManager.releaseLight( pointLight1 );
        //_gfxLights->lightManager.releaseLight( pointLight0 );

        _gfxLights->shutdown( _gdiDevice );
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
            , cameraInputCtx.leftInputX * 0.25f 
            , cameraInputCtx.leftInputY * 0.25f 
            , cameraInputCtx.rightInputX * deltaTime * 5.f
            , cameraInputCtx.rightInputY * deltaTime * 5.f
            , cameraInputCtx.upDown * 0.25f );
        
        
        rList->clear();

        {
            const Matrix4 world[] = 
            {
                Matrix4( Matrix3::rotationZYX( Vector3( 0.1f, 1.f, 0.f ) ), Vector3( -2.f, 0.f, 0.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, 0.f, 0.f ) ), Vector3(  0.f, 2.f, 0.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, 0.f, 0.f ) ), Vector3(  0.f,-2.f, 0.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.1f, 0.f, 1.f ) ), Vector3(  2.f, 0.f, 0.f ) ),

                Matrix4( Matrix3::rotationZYX( Vector3( 0.1f, 1.f, 0.f ) ), Vector3( -2.f, 0.f - 0.5f, -2.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, 0.f, 0.f ) ), Vector3(  0.f, 2.f - 0.5f, -2.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, 0.f, 0.f ) ), Vector3(  0.f,-2.f - 0.5f, -2.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.1f, 0.f, 1.f ) ), Vector3(  2.f, 0.f - 0.5f, -2.f ) ),

                Matrix4( Matrix3::rotationZYX( Vector3( 0.1f, 1.f, 0.f ) ), Vector3( -2.f, 0.f + 0.5f, 2.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, 0.f, 0.f ) ), Vector3(  0.f, 2.f + 0.5f, 2.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.0f, 0.f, 0.f ) ), Vector3(  0.f,-2.f + 0.5f, 2.f ) ),
                Matrix4( Matrix3::rotationZYX( Vector3( 0.1f, 0.f, 1.f ) ), Vector3(  2.f, 0.f + 0.5f, 2.f ) ),
            };

            bxGfxRenderListItemDesc itemDesc( rsource, fxI, 0, bxAABB( Vector3(-0.5f), Vector3(0.5f) ) );
            bxGfx::renderList_pushBack( rList, &itemDesc, bxGdi::eTRIANGLES, world + 4, 8 );

            itemDesc.setRenderSource( _gfxContext->shared()->rsource.box );
            bxGfx::renderList_pushBack( rList, &itemDesc, bxGdi::eTRIANGLES, world, 4 );
        }

        {
            const Matrix4 world = appendScale( Matrix4( Matrix3::identity(), Vector3( 0.f, -3.f, 0.0f ) ), Vector3( 100.f, 0.1f, 100.f ) );
            bxGdiRenderSource* box = _gfxContext->shared()->rsource.box;
            
            bxGfxRenderListItemDesc itemDesc( box, fxI, 0, bxAABB( Vector3(-0.5f), Vector3(0.5f) ) );
            bxGfx::renderList_pushBack( rList, &itemDesc, bxGdi::eTRIANGLES, world );
        }

        bxGfx::cameraMatrix_compute( &camera.matrix, camera.params, camera.matrix.world, _gfxContext->framebufferWidth(), _gfxContext->framebufferHeight() );

        for( int ilight = 0; ilight < nPointLights; ++ilight )
        {
            bxGfxLight_Point l = _gfxLights->lightManager.pointLight( pointLights[ilight] );

            const u32 color = bxColor::float3ToU32( l.color.xyz );
            const Vector3 pos( xyz_to_m128( l.position.xyz ) );
            bxGfxDebugDraw::addSphere( Vector4( pos, floatInVec( l.radius*0.1f ) ), color, true );
        }


        _gfxLights->cullLights( camera );
        
        _gfxContext->frameBegin( _gdiContext );

        _gfxLights->bind( _gdiContext );

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