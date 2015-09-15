#include <system/application.h>
#include <system/window.h>
#include <util/time.h>
#include <util/config.h>
#include <util/signal_filter.h>

#include "gfx.h"

namespace bx
{

struct CameraInputContext
{
    f32 _leftInputX;
    f32 _leftInputY;
    f32 _rightInputX;
    f32 _rightInputY;
    f32 _upDown;

    CameraInputContext()
        : _leftInputX( 0.f ), _leftInputY( 0.f )
        , _rightInputX( 0.f ), _rightInputY( 0.f )
        , _upDown( 0.f )
    {}

    void fetchInput( const bxInput* input, float mouseSensitivityInPix, float dt );
    Matrix4 movementFPP( const Matrix4& world );
    int anyMovement() const 
    {
        const float sum = ::abs( _leftInputX ) + ::abs( _leftInputY ) + ::abs( _rightInputX ) + ::abs( _rightInputY ) + ::abs( _upDown );
        return sum > 0.01f;
    }
};

void CameraInputContext::fetchInput( const bxInput* input, float mouseSensitivityInPix, float dt )
{
    const int fwd = bxInput_isKeyPressed( &input->kbd, 'W' );
    const int back = bxInput_isKeyPressed( &input->kbd, 'S' );
    const int left = bxInput_isKeyPressed( &input->kbd, 'A' );
    const int right = bxInput_isKeyPressed( &input->kbd, 'D' );
    const int up = bxInput_isKeyPressed( &input->kbd, 'Q' );
    const int down = bxInput_isKeyPressed( &input->kbd, 'Z' );

    //bxLogInfo( "%d", fwd );

    const int mouse_lbutton = input->mouse.currentState()->lbutton;
    const int mouse_mbutton = input->mouse.currentState()->mbutton;
    const int mouse_rbutton = input->mouse.currentState()->rbutton;

    const int mouse_dx = input->mouse.currentState()->dx;
    const int mouse_dy = input->mouse.currentState()->dy;
    const float mouse_dxF = (float)mouse_dx;
    const float mouse_dyF = (float)mouse_dy;

    const float leftInputY = (mouse_dyF + mouse_dxF) * mouse_rbutton; // -float( back ) + float( fwd );
    const float leftInputX = mouse_dxF * mouse_mbutton; //-float( left ) + float( right );
    const float upDown = mouse_dyF * mouse_mbutton; //-float( down ) + float( up );

    const float rightInputX = mouse_dxF * mouse_lbutton * mouseSensitivityInPix;
    const float rightInputY = mouse_dyF * mouse_lbutton * mouseSensitivityInPix;

    const float rc = 0.05f;

    _leftInputX = signalFilter_lowPass( leftInputX, _leftInputX, rc, dt );
    _leftInputY = signalFilter_lowPass( leftInputY, _leftInputY, rc, dt );
    _rightInputX = signalFilter_lowPass( rightInputX, _rightInputX, rc, dt );
    _rightInputY = signalFilter_lowPass( rightInputY, _rightInputY, rc, dt );
    _upDown = signalFilter_lowPass( upDown, _upDown, rc, dt );
}

Matrix4 CameraInputContext::movementFPP( const Matrix4& world )
{
    Vector3 localPosDispl( 0.f );
    localPosDispl += Vector3::zAxis() * _leftInputY * 0.15f;
    localPosDispl += Vector3::xAxis() * _leftInputX * 0.15f;
    localPosDispl -= Vector3::yAxis() * _upDown     * 0.15f;


    //bxLogInfo( "%f; %f", rightInputX, rightInputY );

    const floatInVec rotDx( _rightInputX );
    const floatInVec rotDy( _rightInputY );

    const Matrix3 wmatrixRot( world.getUpper3x3() );

    const Quat worldRotYdispl = Quat::rotationY( -rotDx );
    const Quat worldRotXdispl = Quat::rotation( -rotDy, wmatrixRot.getCol0() );
    const Quat worldRotDispl = worldRotYdispl * worldRotXdispl;

    const Quat curWorldRot( wmatrixRot );

    const Quat worldRot = normalize( worldRotDispl * curWorldRot );
    const Vector3 worldPos = mulAsVec4( world, localPosDispl );

    return Matrix4( worldRot, worldPos );
}

}///

class App : public bxApplication
{
public:
    App()
        : _gfx( nullptr )
        , _renderData( nullptr )
        , _resourceManager( nullptr )
        , _timeMS(0)
    {}

    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();
        bxConfig::global_init();
        const char* assetDir = bxConfig::global_string( "assetDir" );
        _resourceManager = bxResourceManager::startup( assetDir );

        bx::gfxStartup( &_gfx, win->hwnd, true, false );

        _testShader = bx::gfxShaderCreate( _gfx, _resourceManager );
        bx::gfxLinesDataCreate( &_renderData, _gfx, 1024 * 8 );

        _camera.world = Matrix4::translation( Vector3( 0.f, 0.f, 5.f ) );

        return true;
    }
    virtual void shutdown()
    {
        bx::gfxLinesDataDestroy( &_renderData, _gfx );
        bx::gfxShaderDestroy( &_testShader, _gfx, _resourceManager );
        bx::gfxShutdown( &_gfx );
        bxResourceManager::shutdown( &_resourceManager );

        bxConfig::global_deinit();
    }

    virtual bool update( u64 deltaTimeUS )
    {
        bxWindow* win = bxWindow_get();
        if( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        const double deltaTimeS = bxTime::toSeconds( deltaTimeUS );
        const float deltaTime = (float)deltaTimeS;

        _cameraInputCtx.fetchInput( &win->input, 0.1f, deltaTime );
        _camera.world = _cameraInputCtx.movementFPP( _camera.world );
                                    
        bx::GfxCommandQueue* cmdQueue = bx::gfxAcquireCommandQueue( _gfx );

        const Vector3 positions[] =
        {
            Vector3( -0.5f, -0.5f, 0.f ), Vector3( 0.5f, 0.5f, 0.f ),
        };
        const Vector3 normals[] =
        {
            Vector3::zAxis(), Vector3::yAxis(),
        };
        const u32 colors[] = 
        {
            0xFFFFFFFF, 0xFF00FFFF,
        };

        bx::gfxLinesDataAdd( _renderData, 2, positions, normals, colors );
        bx::gfxLinesDataUpload( cmdQueue, _renderData );
                
        bx::gfxFrameBegin( _gfx, cmdQueue );

        bx::gfxCameraComputeMatrices( &_camera );
        bx::gfxCameraSet( cmdQueue, _camera );
        bx::gfxViewportSet( cmdQueue, _camera );

        bx::gfxShaderEnable( cmdQueue, _testShader );
        bx::gfxLinesDataFlush( cmdQueue, _renderData );
        bx::gfxLinesDataClear( _renderData );

        bx::gfxReleaseCommandQueue( &cmdQueue );
        bx::gfxFrameEnd( _gfx );

        _timeMS += deltaTimeUS / 1000;

        

        return true;
    }

    bx::CameraInputContext _cameraInputCtx;
    bx::GfxCamera _camera;
    bx::GfxContext* _gfx;
    bx::GfxLinesData* _renderData;
    bx::GfxShaderId _testShader;


    bxResourceManager* _resourceManager;

    u64 _timeMS;
};

int main( int argc, const char** argv )
{
    bxWindow* window = bxWindow_create( "ninja", 1280, 720, false, 0 );
    if( window )
    {
        App app;
        if( bxApplication_startup( &app, argc, argv ) )
        {
            bxApplication_run( &app );
        }

        bxApplication_shutdown( &app );
        bxWindow_release();
    }

    return 0;
}
