#include "gfx.h"
#include <util/type.h>
#include <util/debug.h>
#include <util/memory.h>
#include <util/buffer_utils.h>
#include <util/common.h>

#include <GL/glew.h>
#include <GL/wglew.h>

#include <stdio.h>

namespace bx
{
    static const u32 cMAX_INSTANCES = 256;

    //////////////////////////////////////////////////////////////////////////
    ////
    struct GfxFramebuffer
    {
        u32 _colorTextureId;
        u32 _depthTextureId;

        u16 _width;
        u16 _height;

        GfxFramebuffer()
            : _colorTextureId(0)
            , _depthTextureId(0)
            , _width(0)
            , _height(0)
        {}
    };
    void gfxFramebufferCreate( GfxFramebuffer* fb, int w, int h )
    {
        fb->_width = w;
        fb->_height = h;
    }
    void gfxFramebufferDestroy( GfxFramebuffer* fb )
    {
    
    }
    

    //////////////////////////////////////////////////////////////////////////
    ////
    struct GfxViewShaderData
    {
        Matrix4 _cameraView;
        Matrix4 _cameraProj;
        Matrix4 _cameraViewProj;
        Matrix4 _cameraWorld;
        float4_t _cameraEyePos;
        float4_t _cameraViewDir;
        float2_t _renderTargetRcp;
        float2_t _renderTargetSize;
        float _cameraFov;
        float _cameraAspect;
        float _cameraZNear;
        float _cameraZFar;
    };
    void gfxViewShaderDataFill( GfxViewShaderData* sdata, const GfxCamera& camera, int rtWidth, int rtHeight )
    {
        //SYS_STATIC_ASSERT( sizeof( FrameData ) == 376 );

        const Matrix4 sc = Matrix4::scale( Vector3( 1, 1, 0.5f ) );
        const Matrix4 tr = Matrix4::translation( Vector3( 0, 0, 1 ) );
        const Matrix4 proj = sc * tr * camera.proj;

        sdata->_cameraView = camera.view;
        sdata->_cameraProj = proj;
        sdata->_cameraViewProj = proj * camera.view;
        sdata->_cameraWorld = camera.world;

        const float fov = gfxCameraFov( camera );
        const float aspect = gfxCameraAspect( camera );
        sdata->_cameraFov = fov;
        sdata->_cameraAspect = aspect;

        const float zNear = camera.zNear;
        const float zFar = camera.zFar;
        sdata->_cameraZNear = zNear;
        sdata->_cameraZFar = zFar;
        //sdata->_reprojectDepthScale = ( zFar - zNear ) / ( -zFar * zNear );
        //sdata->_reprojectDepthBias = zFar / ( zFar * zNear );

        sdata->_renderTargetRcp = float2_t( 1.f / (float)rtWidth, 1.f / (float)rtHeight );
        sdata->_renderTargetSize = float2_t( (float)rtWidth, (float)rtHeight );

        //frameData->cameraParams = Vector4( fov, aspect, camera.params.zNear, camera.params.zFar );
        {
            const float m11 = proj.getElem( 0, 0 ).getAsFloat();//getCol0().getX().getAsFloat();
            const float m22 = proj.getElem( 1, 1 ).getAsFloat();//getCol1().getY().getAsFloat();
            const float m33 = proj.getElem( 2, 2 ).getAsFloat();//getCol2().getZ().getAsFloat();
            const float m44 = proj.getElem( 3, 2 ).getAsFloat();//getCol3().getZ().getAsFloat();

            const float m13 = proj.getElem( 0, 2 ).getAsFloat();//getCol3().getZ().getAsFloat();
            const float m23 = proj.getElem( 1, 2 ).getAsFloat();//getCol3().getZ().getAsFloat();

            //sdata->_reprojectInfo = float4_t( 1.f / m11, 1.f / m22, m33, -m44 );
            ////frameData->_reprojectInfo = float4_t( 
            ////    -2.f / ( (float)rtWidth*m11 ), 
            ////    -2.f / ( (float)rtHeight*m22 ), 
            ////    (1.f - m13) / m11, 
            ////    (1.f + m23) / m22 );
            //sdata->_reprojectInfoFromInt = float4_t(
            //    ( -sdata->_reprojectInfo.x * 2.f ) * sdata->_renderTarget_rcp.x,
            //    ( -sdata->_reprojectInfo.y * 2.f ) * sdata->_renderTarget_rcp.y,
            //    sdata->_reprojectInfo.x,
            //    sdata->_reprojectInfo.y
            //    );
            //frameData->_reprojectInfoFromInt = float4_t(
            //    frameData->_reprojectInfo.x,
            //    frameData->_reprojectInfo.y,
            //    frameData->_reprojectInfo.z + frameData->_reprojectInfo.x * 0.5f,
            //    frameData->_reprojectInfo.w + frameData->_reprojectInfo.y * 0.5f
            //    );
        }

        m128_to_xyzw( sdata->_cameraEyePos.xyzw, Vector4( gfxCameraEye( camera ), oneVec ).get128() );
        m128_to_xyzw( sdata->_cameraViewDir.xyzw, Vector4( gfxCameraDir( camera ), zeroVec ).get128() );
        //m128_to_xyzw( frameData->_renderTarget_rcp_size.xyzw, Vector4( 1.f / float( rtWidth ), 1.f / float( rtHeight ), float( rtWidth ), float( rtHeight ) ).get128() );    
    }

    //////////////////////////////////////////////////////////////////////////
    ////
    struct GfxView
    {
        u32 _viewParamsBufferId;
        u32 _instanceWorldBufferId;
        u32 _instanceWorldITBufferId;

        i32 _maxInstances;

        GfxView()
            : _viewParamsBufferId( 0 )
            , _instanceWorldBufferId( 0 )
            , _instanceWorldITBufferId( 0 )
            , _maxInstances( 0 )
        {}
    };
    void gfxViewCreate( GfxView* view, int maxInstances )
    {
        glGenBuffers( 1, &view->_viewParamsBufferId );
        glGenBuffers( 1, &view->_instanceWorldBufferId );
        glGenBuffers( 1, &view->_instanceWorldITBufferId );

        glBindBuffer( GL_UNIFORM_BUFFER, view->_viewParamsBufferId );
        glBufferStorage( GL_UNIFORM_BUFFER, sizeof( GfxViewShaderData ), nullptr, GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT );
        glBindBuffer( GL_UNIFORM_BUFFER, 0 );

        glBindBuffer( GL_SHADER_STORAGE_BUFFER, view->_instanceWorldBufferId );
        glBufferStorage( GL_SHADER_STORAGE_BUFFER, maxInstances * sizeof( Matrix4 ), nullptr, GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT );

        glBindBuffer( GL_SHADER_STORAGE_BUFFER, view->_instanceWorldITBufferId );
        glBufferStorage( GL_SHADER_STORAGE_BUFFER, maxInstances * sizeof( Matrix3 ), nullptr, GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT );
        glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
    }

    void gfxViewDestroy( GfxView* view )
    {
        glDeleteBuffers( 1, &view->_instanceWorldITBufferId );
        glDeleteBuffers( 1, &view->_instanceWorldBufferId );
        glDeleteBuffers( 1, &view->_viewParamsBufferId );
    }

    enum EVertexAttribLocation
    {
        ePOSITION = 0,
        eNORMAL = 1,
        eTEXCOORD0 = 2,
        eCOLOR = 3,

        eATTRIB_COUNT,
    };
    static const char* vertexAttribName[eATTRIB_COUNT] =
    {
        "POSITION",
        "NORMAL",
        "TEXCOORD",
        "COLOR",
    };


    struct GfxCommandQueue
    {
        GfxView _view;
        u32 _acquireCounter;
        GfxContext* _ctx;

        GfxCommandQueue()
            : _acquireCounter(0)
            
        {}
    };

    struct GfxContext
    {
        HWND _hWnd;
        HDC _hDC;
        HGLRC _hGLrc;

        u16 _windowWidth;
        u16 _windowHeight;

        GfxFramebuffer _framebuffer;
        GfxCommandQueue _commandQueue;

        u32 _flag_coreContext : 1;
        u32 _flag_debugContext : 1;

        GfxContext()
            : _hWnd(0)
            , _hDC(0)
            , _hGLrc(0)
            , _flag_coreContext(0)
            , _flag_debugContext(0)
        {}
    };
    

    GLenum gfxGLCheckError( const char* situation, const char* func = "", const char* file = __FILE__, int line = __LINE__ )
    {
        char info[1024];
        sprintf_s( info, 1023, "at %s, %s(%d)", func, file, line );
        info[1023] = NULL;
        GLenum err = glGetError();
        bool retVal = err != GL_NO_ERROR;
        while ( err != GL_NO_ERROR )
        {
            switch ( err )
            {
            case GL_INVALID_ENUM: printf_s( "GL_INVALID_ENUM: %s, %s", situation, info ); break;
            case GL_INVALID_VALUE: printf_s( "GL_INVALID_VALUE: %s, %s", situation, info ); break;
            case GL_INVALID_OPERATION: printf_s( "GL_INVALID_OPERATION: %s, %s", situation, info ); break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: printf_s( "GL_INVALID_FRAMEBUFFER_OPERATION: %s, %s", situation, info ); break;
            case GL_STACK_OVERFLOW: printf_s( "GL_STACK_OVERFLOW: %s, %s", situation, info ); break;
            case GL_STACK_UNDERFLOW: printf_s( "GL_STACK_UNDERFLOW: %s, %s", situation, info ); break;
            default:
                printf_s( "Unknown OpenGL error!" ); break;
            }
            err = glGetError();
        }
        return retVal;
    }

    void _GfxPreStartup()
    {
        HGLRC curC = wglGetCurrentContext();
        HDC curH = wglGetCurrentDC();

        // fake window to get address of wglCreateContextAttribsARB
        //
        PIXELFORMATDESCRIPTOR pfd = {
            sizeof( PIXELFORMATDESCRIPTOR ), 1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_SWAP_EXCHANGE,
            PFD_TYPE_RGBA,
            (BYTE)32,
            0, 0, 0, 0, 0, 0, 0, 0, (BYTE)0, 0, 0, 0, 0,
            (BYTE)0, (BYTE)0,
            0, PFD_MAIN_PLANE, 0, 0, 0, 0
        };

        HINSTANCE hInst = GetModuleHandle( NULL );
        WNDCLASS wincl;
        wincl.hInstance = hInst;
        wincl.lpszClassName = "PFrmt";
        wincl.lpfnWndProc = DefWindowProc;
        wincl.style = 0;
        wincl.hIcon = NULL;
        wincl.hCursor = NULL;
        wincl.lpszMenuName = NULL;
        wincl.cbClsExtra = 0;
        wincl.cbWndExtra = 0;
        wincl.hbrBackground = NULL;
        RegisterClass( &wincl );

        // Create a dummy window to get our extension entry points
        HWND hPFwnd = CreateWindow( "PFrmt", "PFormat", WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 8, 8, HWND_DESKTOP, NULL, hInst, NULL );

        HDC hdc = GetDC( hPFwnd );
        SetPixelFormat( hdc, ChoosePixelFormat( hdc, &pfd ), &pfd );
        HGLRC hglrc = wglCreateContext( hdc );
        SYS_ASSERT( hglrc != 0 );
        wglMakeCurrent( hdc, hglrc );

        wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress( "wglCreateContextAttribsARB" );
        wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress( "wglChoosePixelFormatARB" );
        wglGetPixelFormatAttribivARB = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)wglGetProcAddress( "wglGetPixelFormatAttribivARB" );

        // restore previous context
        //
        wglMakeCurrent( curH, curC );

        // delete fake one
        //
        wglDeleteContext( hglrc );
        ReleaseDC( hPFwnd, hdc );

        SendMessage( hPFwnd, WM_CLOSE, 0, 0 );
        UnregisterClass( "PFrmt", hInst );
    }

    void gfxStartup( GfxContext** ctx, void* hwnd, bool debugContext, bool coreContext )
    {
        _GfxPreStartup();

        const u32 nAttribs = 10;
        int attributes[nAttribs * 2 + 2] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
            WGL_SWAP_METHOD_ARB, WGL_SWAP_EXCHANGE_ARB,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB, 32,
            WGL_DEPTH_BITS_ARB, 0,
            WGL_ACCUM_BITS_ARB, 0,
            WGL_STENCIL_BITS_ARB, 0,
            NULL, NULL,
        };

        HWND hWnd = (HWND)hwnd;
        HDC hdc = GetDC( hWnd );
        HGLRC hglrc = NULL;

        int formats[1024] = { -1 };
        u32 nFormats = 0;
        BOOL bres = wglChoosePixelFormatARB( hdc, attributes, NULL, 1024, formats, &nFormats );
        SYS_ASSERT( bres );

        int selectedFormat = formats[0];

        for ( u32 pf = 0; pf < nFormats; ++pf )
        {
            int f = formats[pf];

            int attributesToQuery[nAttribs] = {
                WGL_DRAW_TO_WINDOW_ARB, // 0
                WGL_ACCELERATION_ARB, // 1
                WGL_SWAP_METHOD_ARB, // 2
                WGL_SUPPORT_OPENGL_ARB, // 3
                WGL_DOUBLE_BUFFER_ARB, // 4
                WGL_PIXEL_TYPE_ARB, // 5
                WGL_COLOR_BITS_ARB, // 6
                WGL_DEPTH_BITS_ARB, // 7
                WGL_ACCUM_BITS_ARB, // 8
                WGL_STENCIL_BITS_ARB, // 9
            };

            int values[nAttribs] = { -1 };

            bres = wglGetPixelFormatAttribivARB( hdc, f, 0, nAttribs, attributesToQuery, values );
            SYS_ASSERT( bres );

            if ( values[6] == 32 && values[7] == 0 && values[9] == 0 )
            {
                selectedFormat = formats[pf];
                break;
            }
        }

        PIXELFORMATDESCRIPTOR pfd;
        memset( &pfd, 0, sizeof( pfd ) );
        pfd.nSize = sizeof( PIXELFORMATDESCRIPTOR );
        DescribePixelFormat( hdc, selectedFormat, sizeof( PIXELFORMATDESCRIPTOR ), &pfd );
        bres = SetPixelFormat( hdc, selectedFormat, &pfd );
        SYS_ASSERT( bres );

        {
            const u32 versions[] = {
                4, 5,
                4, 4,
                4, 3,
                4, 2,
                4, 1,
                4, 0,
                3, 3
            };

            const u32 nVersions = sizeof( versions ) / (sizeof( u32 ) * 2);

            for ( u32 iver = 0; iver < nVersions; ++iver )
            {
                u32 verMajor = versions[iver * 2 + 0];
                u32 verMinor = versions[iver * 2 + 1];

                int attribList[12] = { 0 };
                {
                    u32 idx = 0;
                    attribList[idx++] = WGL_CONTEXT_MAJOR_VERSION_ARB; attribList[idx++] = verMajor;
                    attribList[idx++] = WGL_CONTEXT_MINOR_VERSION_ARB; attribList[idx++] = verMinor;

                    if ( coreContext )
                    {
                        attribList[idx++] = WGL_CONTEXT_PROFILE_MASK_ARB;
                        attribList[idx++] = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
                    }
                    else
                    {
                        attribList[idx++] = WGL_CONTEXT_PROFILE_MASK_ARB;
                        attribList[idx++] = WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
                    }

                    if ( debugContext )
                    {
                        attribList[idx++] = WGL_CONTEXT_FLAGS_ARB;
                        attribList[idx++] = WGL_CONTEXT_DEBUG_BIT_ARB;
                    }

                };

               hglrc = wglCreateContextAttribsARB( hdc, 0, attribList );

                if ( hglrc )
                {
                    break;
                }
            }
        }
        wglMakeCurrent( hdc, hglrc );

        int ierr = glewInit();
        SYS_ASSERT( ierr == GLEW_OK );


        wglSwapIntervalEXT( 0 );

        gfxGLCheckError( "after gfxStartup" );

        GfxContext* context = BX_NEW( bxDefaultAllocator(), GfxContext );

        context->_hWnd = hWnd;
        context->_hDC = hdc;
        context->_hGLrc = hglrc;
        context->_flag_coreContext = coreContext;
        context->_flag_debugContext = debugContext;
        context->_commandQueue._ctx = context;

        RECT rect;
        GetClientRect( hWnd, &rect );
        context->_windowWidth  = (u16)( rect.right - rect.left );
        context->_windowHeight = (u16)( rect.bottom - rect.top );

        gfxViewCreate( &context->_commandQueue._view, cMAX_INSTANCES );
        gfxFramebufferCreate( &context->_framebuffer, 1920, 1080 );

        ctx[0] = context;
        
    }

    void gfxShutdown( GfxContext** ctx )
    {
        GfxContext* context = ctx[0];
        
        gfxFramebufferDestroy( &context->_framebuffer );
        gfxViewDestroy( &context->_commandQueue._view );

        if ( context->_hGLrc )
        {
            wglDeleteContext( context->_hGLrc );
            context->_hGLrc = NULL;
        }

        if ( context->_hDC && context->_hWnd )
        {
            ReleaseDC( context->_hWnd, context->_hDC );
            context->_hDC = NULL;
        }

        context->_hWnd = NULL;

        BX_DELETE0( bxDefaultAllocator(), ctx[0] );

    }

    GfxCommandQueue* gfxAcquireCommandQueue( GfxContext* ctx )
    {
        GfxCommandQueue* cmdQueue = &ctx->_commandQueue;
        SYS_ASSERT( cmdQueue->_acquireCounter == 0 );
        ++cmdQueue->_acquireCounter;
        return cmdQueue;
    }

    void gfxReleaseCommandQueue( GfxCommandQueue** cmdQueue )
    {
        SYS_ASSERT( cmdQueue[0]->_acquireCounter == 1 );
        --cmdQueue[0]->_acquireCounter;
        cmdQueue[0] = nullptr;
    }

    void gfxFrameBegin( GfxContext* ctx, GfxCommandQueue* cmdQueue )
    {
        glClearColor( 0.f, 0.f, 0.f, 1.f );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        glViewport( 0, 0, ctx->_windowWidth, ctx->_windowHeight );
    }

    void gfxFrameEnd( GfxContext* ctx )
    {
        SwapBuffers( ctx->_hDC );
    }

}////

namespace bx
{
    float gfxCameraAspect( const GfxCamera& cam )
    {
        return ( abs( cam.vAperture ) < 0.0001f ) ? cam.hAperture : cam.hAperture / cam.vAperture;
    }

    float gfxCameraFov( const GfxCamera& cam )
    {
        return 2.f * atan( ( 0.5f * cam.hAperture ) / ( cam.focalLength * 0.03937f ) );
    }
    Vector3 gfxCameraEye( const GfxCamera& cam )
    {
        return cam.world.getTranslation();
    }
    Vector3 gfxCameraDir( const GfxCamera& cam )
    {
        return -cam.world.getCol2().getXYZ();
    }

    void gfxCameraViewport( GfxViewport* vp, const GfxCamera& cam, int dstWidth, int dstHeight, int srcWidth, int srcHeight )
    {
        const int windowWidth = dstWidth;
        const int windowHeight = dstHeight;

        const float aspectRT = (float)srcWidth / (float)srcHeight;
        const float aspectCamera = gfxCameraAspect(cam);

        int imageWidth;
        int imageHeight;
        int offsetX = 0, offsetY = 0;


        if( aspectCamera > aspectRT )
        {
            imageWidth = windowWidth;
            imageHeight = (int)( windowWidth / aspectCamera + 0.0001f );
            offsetY = windowHeight - imageHeight;
            offsetY = offsetY / 2;
        }
        else
        {
            float aspect_window = (float)windowWidth / (float)windowHeight;
            if( aspect_window <= aspectRT )
            {
                imageWidth = windowWidth;
                imageHeight = (int)( windowWidth / aspectRT + 0.0001f );
                offsetY = windowHeight - imageHeight;
                offsetY = offsetY / 2;
            }
            else
            {
                imageWidth = (int)( windowHeight * aspectRT + 0.0001f );
                imageHeight = windowHeight;
                offsetX = windowWidth - imageWidth;
                offsetX = offsetX / 2;
            }
        }
        vp[0] = GfxViewport( offsetX, offsetY, imageWidth, imageHeight );
    }

    void gfxCameraComputeMatrices( GfxCamera* cam )
    {
        const float fov = gfxCameraFov( *cam );
        const float aspect = gfxCameraAspect( *cam );
        
        cam->view = inverse( cam->world );
        cam->proj = Matrix4::perspective( fov, aspect, cam->zNear, cam->zFar );
        cam->viewProj = cam->proj * cam->view;
    }
    
    void gfxViewCameraSet( GfxCommandQueue* cmdQueue, const GfxCamera& camera )
    {
        GfxViewShaderData shaderData;
        memset( &shaderData, 0x00, sizeof( GfxViewShaderData ) );
        gfxViewShaderDataFill( &shaderData, camera, cmdQueue->_ctx->_framebuffer._width, cmdQueue->_ctx->_framebuffer._height );

        glBindBuffer( GL_UNIFORM_BUFFER, cmdQueue->_view._viewParamsBufferId );
        glBufferSubData( GL_UNIFORM_BUFFER, 0, sizeof( GfxViewShaderData ), &shaderData );
        glBindBuffer( GL_UNIFORM_BUFFER, 0 );



    }

    void gfxViewInstanceSet( GfxCommandQueue* cmdQueue, int nMatrices, const Matrix4* matrices )
    {

    }
    
    void gfxViewportSet( GfxCommandQueue* cmdQueue, const GfxCamera& camera )
    {
        GfxViewport vp;

        int srcw = cmdQueue->_ctx->_framebuffer._width;
        int srch = cmdQueue->_ctx->_framebuffer._height;
        int dstw = cmdQueue->_ctx->_windowWidth;
        int dsth = cmdQueue->_ctx->_windowHeight;
        gfxCameraViewport( &vp, camera, dstw, dsth, srcw, srch );
        glViewport( vp.x, vp.y, vp.w, vp.h );
    }

    void gfxViewportSet( GfxCommandQueue* cmdQueue, const GfxViewport& vp )
    {
        (void)cmdQueue;
        glViewport( vp.x, vp.y, vp.w, vp.h );
    }
}///

namespace bx
{
    struct GfxLinesContext
    {
        u32 _programId;
        u32 _paramsBufferId;
        
        GfxLinesContext()
            : _programId( 0 )
            , _paramsBufferId( 0 )
        {}
    };

    static const char shaderSource[] =
    {
        "#if defined( bx__vertex__ )\n"
        "in vec3 POSITION;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4( POSITION, 1.0 );\n"
        "}\n"
        "#endif\n"

        "#if defined( bx__fragment__ )\n"
        "out vec4 _color;\n"
        "void main()\n"
        "{\n"
        "   _color = vec4( 1.0, 0.0, 0.0, 1.0 );\n"
        "}\n"
        "#endif\n"
    };


    void gfxPrintShaderInfoLog( GLuint shaderId ) 
    {
        int maxLength = 2048;
        int actualLength = 0;
        char log[2048];
        glGetShaderInfoLog( shaderId, maxLength, &actualLength, log );
        printf_s( "shader info log for GL index %u:\n%s\n", shaderId, log );
    }
    void gfxPrintProgramIngoLog( GLuint programId )
    {
        int maxLength = 2048;
        int actualLength = 0;
        char log[2048];
        glGetProgramInfoLog( programId, maxLength, &actualLength, log );
        printf_s( "program info log for GL index %u:\n%s\n", programId, log );
    }

    u32 gfxShaderCreate( GfxContext* ctx, const char* shaderSource )
    {
        u32 vertexShaderId = glCreateShader( GL_VERTEX_SHADER );
        u32 fragmentShaderId = glCreateShader( GL_FRAGMENT_SHADER );
        u32 programId = glCreateProgram();

        for( int iattr = 0; iattr < eATTRIB_COUNT; ++iattr )
        {
            glBindAttribLocation( programId, iattr, vertexAttribName[iattr] );
        }

        const char version[] = "#version 330\n";
        const char vertexMacro[] = "#define bx__vertex__\n";
        const char fragmentMacro[] = "#define bx__fragment__\n";

        const char* vertexSources[] =
        {
            version, vertexMacro, shaderSource,
        };
        const char* fragmentSources[] =
        {
            version, fragmentMacro, shaderSource,
        };

        glShaderSource( vertexShaderId, 3, vertexSources, NULL );
        glShaderSource( fragmentShaderId, 3, fragmentSources, NULL );

        int params = -1;
        glCompileShader( vertexShaderId );
        glGetShaderiv( vertexShaderId, GL_COMPILE_STATUS, &params );
        if( params != GL_TRUE )
        {
            fprintf_s( stderr, "ERROR: GL vertex shader did not compile\n" );
            gfxPrintShaderInfoLog( vertexShaderId );
            SYS_ASSERT( false );
        }

        glCompileShader( fragmentShaderId );
        glGetShaderiv( fragmentShaderId, GL_COMPILE_STATUS, &params );
        if( params != GL_TRUE )
        {
            fprintf_s( stderr, "ERROR: GL vertex shader did not compile\n" );
            gfxPrintShaderInfoLog( fragmentShaderId );
            SYS_ASSERT( false );
        }

        glAttachShader( programId, vertexShaderId );
        glAttachShader( programId, fragmentShaderId );
        glLinkProgram( programId );
        glGetProgramiv( programId, GL_LINK_STATUS, &params );
        if( params != GL_TRUE )
        {
            fprintf_s( stderr, "ERROR: GL shader program did not link\n" );
            gfxPrintProgramIngoLog( programId );
            SYS_ASSERT( false );
        }

        glDeleteShader( fragmentShaderId );
        glDeleteShader( vertexShaderId );

        return programId;
    }

    void gfxLinesContextCreate( GfxLinesContext** linesCtx, GfxContext* ctx, bxResourceManager* resourceManager )
    {
        GfxLinesContext* lctx = BX_NEW( bxDefaultAllocator(), GfxLinesContext );

        lctx->_programId = gfxShaderCreate( ctx, shaderSource );;
        
        linesCtx[0] = lctx;
    }

    void gfxLinesContextDestroy( GfxLinesContext** linesCtx, GfxContext* ctx )
    {
        if ( !linesCtx[0] )
            return;

        GfxLinesContext* lctx = linesCtx[0];
        glDeleteProgram( lctx->_programId );

        BX_DELETE0( bxDefaultAllocator(), linesCtx[0] );
    }

    //////////////////////////////////////////////////////////////////////////
    ////
    struct GfxLinesData
    {
        struct Data
        {
            void* memoryHandle;
            i32 size;
            i32 capacity;

            float3_t* position;
            float3_t* normal;
            u32* color;
        };

        Data _data;
        u32 _vertexArrayId;
        u32 _vertexBufferPositionId;
        u32 _vertexBufferNormalId;
        u32 _vertexBufferColorId;

        GfxLinesData()
            : _vertexArrayId( 0 )
            , _vertexBufferPositionId( 0 )
            , _vertexBufferNormalId( 0 )
            , _vertexBufferColorId( 0 )
        {
            memset( &_data, 0x00, sizeof( GfxLinesData::Data ) );
        }
    };
    void gfxLinesDataAllocate( GfxLinesData::Data* data, int newcap, bxAllocator* alloc )
    {
        int memSize = 0;
        memSize += newcap * sizeof( *data->position );
        memSize += newcap * sizeof( *data->normal );
        memSize += newcap * sizeof( *data->color );

        void* mem = BX_MALLOC( alloc, memSize, 16 );
        memset( mem, 0x00, memSize );

        GfxLinesData::Data newdata;
        newdata.memoryHandle = mem;
        newdata.size = data->size;
        newdata.capacity = newcap;

        bxBufferChunker chunker( mem, memSize );
        newdata.position = chunker.add< float3_t >( newcap );
        newdata.normal = chunker.add< float3_t >( newcap );
        newdata.color = chunker.add< u32 >( newcap );
        chunker.check();

        if( data->size )
        {
            BX_CONTAINER_COPY_DATA( &newdata, data, position );
            BX_CONTAINER_COPY_DATA( &newdata, data, normal );
            BX_CONTAINER_COPY_DATA( &newdata, data, color );
        }

        BX_FREE0( alloc, data->memoryHandle );
        *data = newdata;
    }


    void gfxLinesDataCreate( GfxLinesData** lines, GfxContext* ctx, int initialCapacity )
    {
        GfxLinesData* ldata = BX_NEW( bxDefaultAllocator(), GfxLinesData );
        GfxLinesData::Data* data = &ldata->_data;
        gfxLinesDataAllocate( data, initialCapacity, bxDefaultAllocator() );

        u32 vertexArrayId;
        glGenVertexArrays( 1, &vertexArrayId );
        glBindVertexArray( vertexArrayId );

        u32 vertexBufferId;
        {
            glGenBuffers( 1, &vertexBufferId );
            glBindBuffer( GL_ARRAY_BUFFER, vertexBufferId );
            glBufferData( GL_ARRAY_BUFFER, initialCapacity * sizeof( *data->position ), data->position, GL_DYNAMIC_DRAW );
            glEnableVertexAttribArray( ePOSITION );
            glVertexAttribPointer( ePOSITION, 3, GL_FLOAT, GL_FALSE, 0, 0 );

            gfxGLCheckError( "create vertex buffer" );

            ldata->_vertexBufferPositionId = vertexBufferId;
        }
        {
            glGenBuffers( 1, &vertexBufferId );
            glBindBuffer( GL_ARRAY_BUFFER, vertexBufferId );
            glBufferData( GL_ARRAY_BUFFER, initialCapacity * sizeof( *data->normal ), data->normal, GL_DYNAMIC_DRAW );
            glEnableVertexAttribArray( eNORMAL );
            glVertexAttribPointer( eNORMAL, 3, GL_FLOAT, GL_FALSE, 0, 0 );

            gfxGLCheckError( "create vertex buffer" );

            ldata->_vertexBufferNormalId = vertexBufferId;
        }

        {
            glGenBuffers( 1, &vertexBufferId );
            glBindBuffer( GL_ARRAY_BUFFER, vertexBufferId );
            glBufferData( GL_ARRAY_BUFFER, initialCapacity * sizeof( *data->color ), data->color, GL_DYNAMIC_DRAW );
            glEnableVertexAttribArray( eCOLOR );
            glVertexAttribPointer( eCOLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0 );

            gfxGLCheckError( "create vertex buffer" );

            ldata->_vertexBufferColorId = vertexBufferId;
        }

        glBindVertexArray( 0 );

        ldata->_vertexArrayId = vertexArrayId;

        lines[0] = ldata;
    }

    void gfxLinesDataDestroy( GfxLinesData** lines, GfxContext* ctx )
    {
        GfxLinesData* ldata = lines[0];
        if( !ldata )
            return;

        GfxLinesData::Data* data = &ldata->_data;

        glDeleteBuffers( 1, &ldata->_vertexBufferColorId );
        glDeleteBuffers( 1, &ldata->_vertexBufferNormalId );
        glDeleteBuffers( 1, &ldata->_vertexBufferPositionId );
        glDeleteVertexArrays( 1, &ldata->_vertexArrayId );

        BX_FREE0( bxDefaultAllocator(), data->memoryHandle );
        memset( data, 0x00, sizeof( GfxLinesData::Data ) );

        BX_DELETE0( bxDefaultAllocator(), lines[0] );
    }
    void gfxLinesDataAdd( GfxLinesData* lines, int count, const Vector3* points, const Vector3* normals, const u32* colors )
    {
        GfxLinesData::Data* data = &lines->_data;
        if( data->size + count > data->capacity )
        {
            int newcap = maxOfPair( data->size + count, data->capacity ) * 2;
            gfxLinesDataAllocate( data, newcap, bxDefaultAllocator() );
        }

        const int size = data->size;
        for( int i = 0; i < count; ++i )
            storeXYZ( points[i], data->position[ size + i].xyz );

        for( int i = 0; i < count; ++i )
            storeXYZ( normals[i], data->normal[size + i].xyz );

        for( int i = 0; i < count; ++i )
            data->color[size + i] = colors[i];

        data->size += count;
    }
    void gfxLinesDataClear( GfxLinesData* lines )
    {
        lines->_data.size = 0;
    }
    void gfxLinesDataUpload( GfxCommandQueue* cmdQueue, const GfxLinesData* lines )
    {
        const GfxLinesData::Data* data = &lines->_data;
        const int size = data->size;
        glBindBuffer( GL_ARRAY_BUFFER, lines->_vertexBufferPositionId );
        glBufferSubData( GL_ARRAY_BUFFER, 0, size * sizeof( *data->position ), data->position );
        
        gfxGLCheckError( "update vertex buffer" );

        glBindBuffer( GL_ARRAY_BUFFER, lines->_vertexBufferNormalId );
        glBufferSubData( GL_ARRAY_BUFFER, 0, size * sizeof( *data->normal ), data->normal );

        gfxGLCheckError( "update vertex buffer" );

        glBindBuffer( GL_ARRAY_BUFFER, lines->_vertexBufferColorId );
        glBufferSubData( GL_ARRAY_BUFFER, 0, size * sizeof( *data->color ), data->color );

        gfxGLCheckError( "update vertex buffer" );

        glBindBuffer( GL_ARRAY_BUFFER, 0 );
    }

    void gfxLinesDataFlush( GfxCommandQueue* cmdQueue, GfxLinesContext* linesCtx, GfxLinesData* lines )
    {
        glUseProgram( linesCtx->_programId );
        gfxGLCheckError( "use program" );

        glBindVertexArray( lines->_vertexArrayId );
        gfxGLCheckError( "bind vertex array" );

        glDrawArrays( GL_LINES, 0, lines->_data.size );
        gfxGLCheckError( "draw" );

        glBindVertexArray( 0 );
        glUseProgram( 0 );
    }

}////
