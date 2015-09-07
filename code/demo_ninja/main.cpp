#include <system/application.h>
#include <system/window.h>
#include <util/time.h>
#include <util/debug.h>
#include <GL/glew.h>
#include <GL/wglew.h>

#include <stdio.h>

namespace bx
{
    struct GfxContext
    {
        HWND _hWnd;
        HDC _hDC;
        HGLRC _hglrc;

        u32 _flag_coreContext : 1;
        u32 _flag_debugContext : 1;
    };

    GLenum gfxGLCheckError( const char* situation, const char* func = "", const char* file = __FILE__, int line = __LINE__ )
    {
        char info[1024];
        sprintf_s( info, 1023, "at %s, %s(%d)", func, file, line );
        info[1023] = NULL;
        GLenum err = glGetError();
        bool retVal = err != GL_NO_ERROR;
        while( err != GL_NO_ERROR )
        {
            switch( err )
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

    void gfxStartup( GfxContext* ctx, HWND hwnd, bool debugContext, bool coreContext )
    {
        _GfxPreStartup();

        ctx->_hWnd = hwnd;

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

        ctx->_hDC = GetDC( hwnd );

        int formats[1024] = { -1 };
        u32 nFormats = 0;
        BOOL bres = wglChoosePixelFormatARB( ctx->_hDC, attributes, NULL, 1024, formats, &nFormats );
        SYS_ASSERT( bres );

        int selectedFormat = formats[0];

        for( u32 pf = 0; pf < nFormats; ++pf )
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

            bres = wglGetPixelFormatAttribivARB( ctx->_hDC, f, 0, nAttribs, attributesToQuery, values );
            SYS_ASSERT( bres );

            if( values[6] == 32 && values[7] == 0 && values[9] == 0 )
            {
                selectedFormat = formats[pf];
                break;
            }
        }

        PIXELFORMATDESCRIPTOR pfd;
        memset( &pfd, 0, sizeof( pfd ) );
        pfd.nSize = sizeof( PIXELFORMATDESCRIPTOR );
        DescribePixelFormat( ctx->_hDC, selectedFormat, sizeof( PIXELFORMATDESCRIPTOR ), &pfd );
        bres = SetPixelFormat( ctx->_hDC, selectedFormat, &pfd );
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

            const u32 nVersions = sizeof( versions ) / ( sizeof( u32 ) * 2 );

            for( u32 iver = 0; iver < nVersions; ++iver )
            {
                u32 verMajor = versions[iver * 2 + 0];
                u32 verMinor = versions[iver * 2 + 1];

                int attribList[12] = { 0 };
                {
                    u32 idx = 0;
                    attribList[idx++] = WGL_CONTEXT_MAJOR_VERSION_ARB; attribList[idx++] = verMajor;
                    attribList[idx++] = WGL_CONTEXT_MINOR_VERSION_ARB; attribList[idx++] = verMinor;

                    if( coreContext )
                    {
                        attribList[idx++] = WGL_CONTEXT_PROFILE_MASK_ARB;
                        attribList[idx++] = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
                    }
                    else
                    {
                        attribList[idx++] = WGL_CONTEXT_PROFILE_MASK_ARB;
                        attribList[idx++] = WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
                    }

                    if( debugContext )
                    {
                        attribList[idx++] = WGL_CONTEXT_FLAGS_ARB;
                        attribList[idx++] = WGL_CONTEXT_DEBUG_BIT_ARB;
                    }

                };

                ctx->_hglrc = wglCreateContextAttribsARB( ctx->_hDC, 0, attribList );

                if( ctx->_hglrc )
                {
                    break;
                }
            }
        }
        wglMakeCurrent( ctx->_hDC, ctx->_hglrc );
        
        int ierr = glewInit();
        SYS_ASSERT( ierr == GLEW_OK );


        wglSwapIntervalEXT( 0 );

        gfxGLCheckError( "after gfxStartup" );

        

    }
    void gfxShutdown( GfxContext* ctx )
    {
        if( ctx->_hglrc )
        {
            wglDeleteContext( ctx->_hglrc );
            ctx->_hglrc = NULL;
        }

        if( ctx->_hDC && ctx->_hWnd )
        {
            ReleaseDC( ctx->_hWnd, ctx->_hDC );
            ctx->_hDC = NULL;
        }

        ctx->_hWnd = NULL;

    }
}///


class App : public bxApplication
{
public:
    App()
        : _timeMS(0)
    {}

    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();
        
        memset( &_gfx, 0x00, sizeof( bx::GfxContext ) );
        bx::gfxStartup( &_gfx, win->hwnd, true, false );


        return true;
    }
    virtual void shutdown()
    {
        bx::gfxShutdown( &_gfx );

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

        _timeMS += deltaTimeUS / 1000;

        return true;
    }

    bx::GfxContext _gfx;

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
