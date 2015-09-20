#include "tjdb.h"
#include <GL/glew.h>
#include <GL/wglew.h>

#include <util/debug.h>
#include <system/window.h>
#include "resource_manager/resource_manager.h"

namespace tjdb
{
    struct WININFO
    {
        HGLRC       hRC;
    }__gfxInfo;

    void initGfx( bxWindow* win )
    {
        static const PIXELFORMATDESCRIPTOR pfd =
        {
            sizeof( PIXELFORMATDESCRIPTOR ),
            1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA,
            32,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            32,             // zbuffer
            0,              // stencil!
            0,
            PFD_MAIN_PLANE,
            0, 0, 0, 0
        };  

        HDC hDC = win->hdc;

        unsigned int pixelFormat = ChoosePixelFormat( hDC, &pfd );
        SYS_ASSERT( pixelFormat );

        BOOL bres = SetPixelFormat( hDC, pixelFormat, &pfd );
        SYS_ASSERT( bres == TRUE );

        __gfxInfo.hRC = wglCreateContext( hDC );
        SYS_ASSERT( __gfxInfo.hRC != 0 );

        bres = wglMakeCurrent( hDC, __gfxInfo.hRC );
        SYS_ASSERT( __gfxInfo.hRC != 0 );

        int ierr = glewInit();
        SYS_ASSERT( ierr == GLEW_OK );
    }

    void deinitGfx()
    {
        wglMakeCurrent( 0, 0 );
        wglDeleteContext( __gfxInfo.hRC );
    }


    void startup( bxWindow* win, bxResourceManager* resourceManager )
    {
        initGfx( win );
    }

    void shutdown()
    {
        deinitGfx();
    }

    void draw( bxWindow* win )
    {
        glClearColor( 0.f, 0.f, 0.f, 1.f );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        glViewport( 0, 0, 1280, 720 );


        
        SwapBuffers( win->hdc );
    }

}///