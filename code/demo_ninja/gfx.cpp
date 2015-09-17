#include "gfx.h"
#include <util/type.h>
#include <util/debug.h>
#include <util/memory.h>
#include <util/buffer_utils.h>
#include <util/common.h>
#include <util/id_table.h>
#include <util/poly/poly_shape.h>

#include <GL/glew.h>
#include <GL/wglew.h>

#include <stdio.h>
#include <gdi/gdi_sort_list.h>

namespace bx
{
    enum
    {
        eMAX_INSTANCES = 256,
        eMAX_SHADERS = 64,
        eMAX_MESHES = 256,

        eBINDING_VIEW_DATA = 0,
        eBINDING_MATERIAL_DATA = 1,
    };


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
#define GFX_VIEW_SHADER_DATA_VARS \
    mat4 _cameraView; \
    mat4 _cameraProj; \
    mat4 _cameraViewProj; \
    mat4 _cameraWorld; \
    vec4 _cameraEyePos; \
    vec4 _cameraViewDir; \
    vec2 _renderTargetRcp; \
    vec2 _renderTargetSize; \
    float _cameraFov; \
    float _cameraAspect; \
    float _cameraZNear; \
    float _cameraZFar;

    typedef Matrix4 mat4;
    typedef float4_t vec4;
    typedef float3_t vec3;
    typedef float2_t vec2;

    struct GfxViewShaderData
    {
        GFX_VIEW_SHADER_DATA_VARS
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
        eATTRIB_POSITION = 0,
        eATTRIB_NORMAL = 1,
        eATTRIB_TEXCOORD0 = 2,
        eATTRIB_COLOR = 3,

        eATTRIB_COUNT,
    };
    static const char* vertexAttribName[eATTRIB_COUNT] =
    {
        "POSITION",
        "NORMAL",
        "TEXCOORD",
        "COLOR",
    };

    

    struct GfxShader
    {
        u32 _programId;

        GfxShader()
            : _programId(0)
        {}

        static GfxShaderId makeId( u32 hash )
        {
            GfxShaderId id = { hash };
            return id;
        }
    };
    struct GfxMesh
    {
        u32 _vertexArrayDesc; /// VAO
        u32 _positionStreamId;
        u32 _normalStreamId;
        u32 _texcoordStreamId;
        u32 _indexStreamId;

        u32 _vertexCount;
        u32 _indexCount;

        f32 _aabb[6];

        GfxMesh()
            : _vertexArrayDesc( 0 )
            , _positionStreamId( 0 )
            , _normalStreamId( 0 )
            , _texcoordStreamId( 0 )
            , _indexStreamId( 0 )
            , _vertexCount( 0 )
            , _indexCount( 0 )
        {
            memset( _aabb, 0x00, sizeof( _aabb ) );
        }

        static GfxMeshId makeId( u32 hash )
        {
            GfxMeshId id = { hash };
            return id;
        }
    };

    template< class Tobj, class Tid, u32 MAX_COUNT >
    struct GfxObjectTable
    {
        id_table_t< MAX_COUNT > _idTable;
        Tobj _dataArray[MAX_COUNT];

        Tid add()
        {
            id_t id = id_table::create( _idTable );
            _dataArray[id.index] = Tobj();
            return Tobj::makeId( id.hash );
        }
        void remove( Tid* id )
        {
            id_t i = make_id( id->hash );

            if( !id_table::has( _idTable, i ) )
                return;

            id_table::destroy( _idTable, i );
            id[0] = Tobj::makeId( 0 );
        }

        bool isValid( Tid id )
        {
            id_t i = make_id( id.hash);
            return id_table::has( _idTable, i );
        }

        Tobj& lookup( Tid id )
        {
            id_t i = make_id( id.hash );
            SYS_ASSERT( id_table::has( _idTable, i ) );
            return _dataArray[make_id( id.hash ).index];
        }
        const Tobj& lookup( Tid id ) const
        {
            SYS_ASSERT( id_table::has( _idTable, make_id( id.id ) ) );
            return _dataArray[make_id( id.hash ).index];
        }
    };

    typedef GfxObjectTable< GfxShader, GfxShaderId, eMAX_SHADERS >  GfxShaderContainer;
    typedef GfxObjectTable< GfxMesh, GfxMeshId, eMAX_MESHES >  GfxMeshContainer;

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
        GfxShaderContainer _shaderContainer;
        GfxMeshContainer _meshContainer;

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

        gfxViewCreate( &context->_commandQueue._view, eMAX_INSTANCES );
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
        glBindBufferBase( GL_UNIFORM_BUFFER, 0, cmdQueue->_view._viewParamsBufferId );
    }

    void gfxFrameEnd( GfxContext* ctx )
    {
        SwapBuffers( ctx->_hDC );
    }


}////

namespace bx
{
    GfxCamera::GfxCamera()
        : world( Matrix4::identity() )
        , view( Matrix4::identity() )
        , proj( Matrix4::identity() )
        , viewProj( Matrix4::identity() )

        , hAperture( 1.8f )
        , vAperture( 1.0f )
        , focalLength( 25.f )
        , zNear( 0.1f )
        , zFar( 1000.f )
        , orthoWidth( 10.f )
        , orthoHeight( 10.f )
    {}

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

    void gfxCameraSet( GfxCommandQueue* cmdQueue, const GfxCamera& camera )
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
    //////////////////////////////////////////////////////////////////////////
    ////
    static const char viewDataUniformBlock[] =
    {
        "layout (std140) uniform ViewData\n"
        "{\n"
        MAKE_STR( GFX_VIEW_SHADER_DATA_VARS )
        "};\n"
    };

    //static const char shaderSource[] =
    //{
    //    "#if defined( bx__vertex__ )\n"
    //    "in vec3 POSITION;\n"
    //    "void main()\n"
    //    "{\n"
    //    "   gl_Position = _cameraViewProj * vec4( POSITION, 1.0 );\n"
    //    "}\n"
    //    "#endif\n"

    //    "#if defined( bx__fragment__ )\n"
    //    "out vec4 _color;\n"
    //    "void main()\n"
    //    "{\n"
    //    "   _color = vec4( 1.0, 0.0, 0.0, 1.0 );\n"
    //    "}\n"
    //    "#endif\n"
    //};


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

    u32 gfxShaderCompile( GfxContext* ctx, const char* shaderSource )
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
            version, viewDataUniformBlock, vertexMacro, shaderSource,
        };
        const char* fragmentSources[] =
        {
            version, viewDataUniformBlock, fragmentMacro, shaderSource,
        };

        int nSources = sizeof( vertexSources ) / sizeof( *vertexSources );
        glShaderSource( vertexShaderId, nSources, vertexSources, NULL );

        nSources = sizeof( fragmentSources ) / sizeof( *fragmentSources );
        glShaderSource( fragmentShaderId, nSources, fragmentSources, NULL );

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
    //////////////////////////////////////////////////////////////////////////
    ////
    GfxShaderId gfxShaderCreate( GfxContext* ctx, bxResourceManager* resourceManager )
    {
        GfxShaderId id = ctx->_shaderContainer.add();
        GfxShader& shader = ctx->_shaderContainer.lookup( id );

        bxFS::File file = resourceManager->readTextFileSync( "shader/glsl/ninja.glsl" );
        const u32 progId = gfxShaderCompile( ctx, file.txt );
        file.release();

        u32 viewDataBlockIndex = glGetUniformBlockIndex( progId, "ViewData" );
        int blockSize = 0;
        glGetActiveUniformBlockiv( progId, viewDataBlockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize );
        SYS_ASSERT( blockSize == sizeof( GfxViewShaderData ) );
        glUniformBlockBinding( progId, viewDataBlockIndex, 0 );

        shader._programId = progId;

        return id;
    }
    void gfxShaderDestroy( GfxShaderId* id, GfxContext* ctx, bxResourceManager* resourceManager )
    {
        if( !ctx->_shaderContainer.isValid( id[0] ) )
            return;

        GfxShader& shader = ctx->_shaderContainer.lookup( id[0] );
        glDeleteProgram( shader._programId );
        shader._programId = 0;
        ctx->_shaderContainer.remove( id );
        id[0] = GfxShader::makeId( 0 );
    }
    void gfxShaderEnable( GfxCommandQueue* cmdQueue, GfxShaderId id )
    {
        SYS_ASSERT( cmdQueue->_ctx->_shaderContainer.isValid( id ) );

        const GfxShader& shader = cmdQueue->_ctx->_shaderContainer.lookup( id );
        glUseProgram( shader._programId );
    }

    GfxMeshId gfxMeshCreate( GfxContext* ctx )
    {
        GfxMeshId id = ctx->_meshContainer.add();
        return id;
    }
    void gfxMeshDestroy( GfxMeshId* id, GfxContext* ctx )
    {
        if( !ctx->_meshContainer.isValid( id[0] ) )
            return;

        GfxMesh& mesh = ctx->_meshContainer.lookup( id[0] );
        glDeleteBuffers( 1, &mesh._indexStreamId );
        glDeleteBuffers( 1, &mesh._texcoordStreamId );
        glDeleteBuffers( 1, &mesh._normalStreamId );
        glDeleteBuffers( 1, &mesh._positionStreamId );
        glDeleteVertexArrays( 1, &mesh._vertexArrayDesc );

        mesh = GfxMesh();

        ctx->_meshContainer.remove( id );
        id[0] = GfxMesh::makeId( 0 );
    }

    void gfxMeshLoadShape( GfxMesh* mesh, const bxPolyShape& shape )
    {
        u32 streamDescId = 0;
        u32 indexStreamId = 0;
        u32 vertexStreamId[eATTRIB_COUNT];
        memset( vertexStreamId, 0x00, sizeof( vertexStreamId ) );

        const int vertexCount = shape.num_vertices;
        const int indexCount = shape.num_indices;

        glCreateVertexArrays( 1, &streamDescId );

        glGenBuffers( 1, &indexStreamId );
        glGenBuffers( 1, &vertexStreamId[eATTRIB_POSITION] );
        glGenBuffers( 1, &vertexStreamId[eATTRIB_NORMAL] );
        glGenBuffers( 1, &vertexStreamId[eATTRIB_TEXCOORD0] );

        glBindVertexArray( streamDescId );

        {
            glBindBuffer( GL_ARRAY_BUFFER, vertexStreamId[eATTRIB_POSITION] );
            glBufferData( GL_ARRAY_BUFFER, vertexCount * 3 * sizeof( float ), shape.positions, GL_STATIC_DRAW );
            glEnableVertexAttribArray( eATTRIB_POSITION );
            glVertexAttribPointer( eATTRIB_POSITION, shape.n_elem_pos, GL_FLOAT, GL_FALSE, 0, 0 );
            gfxGLCheckError( "create vertex buffer" );
        }

        {
            glBindBuffer( GL_ARRAY_BUFFER, vertexStreamId[eATTRIB_NORMAL] );
            glBufferData( GL_ARRAY_BUFFER, vertexCount * 3 * sizeof( float ), shape.normals, GL_STATIC_DRAW );
            glEnableVertexAttribArray( eATTRIB_NORMAL );
            glVertexAttribPointer( eATTRIB_NORMAL, shape.n_elem_nrm, GL_FLOAT, GL_FALSE, 0, 0 );
            gfxGLCheckError( "create vertex buffer" );
        }

        {
            glBindBuffer( GL_ARRAY_BUFFER, vertexStreamId[eATTRIB_TEXCOORD0] );
            glBufferData( GL_ARRAY_BUFFER, vertexCount * 3 * sizeof( float ), shape.normals, GL_STATIC_DRAW );
            glEnableVertexAttribArray( eATTRIB_TEXCOORD0 );
            glVertexAttribPointer( eATTRIB_TEXCOORD0, shape.n_elem_tex, GL_FLOAT, GL_FALSE, 0, 0 );
            gfxGLCheckError( "create vertex buffer" );
        }

        {
            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indexStreamId );
            glBufferData( GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof( u32 ), shape.indices, GL_STATIC_DRAW );
            gfxGLCheckError( "create vertex buffer" );
        }

        glBindVertexArray( 0 );
        glBindBuffer( GL_ARRAY_BUFFER, 0 );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

        mesh->_vertexArrayDesc = streamDescId;
        mesh->_positionStreamId = vertexStreamId[eATTRIB_POSITION];
        mesh->_normalStreamId = vertexStreamId[eATTRIB_NORMAL];
        mesh->_texcoordStreamId = vertexStreamId[eATTRIB_TEXCOORD0];
        mesh->_indexStreamId = indexStreamId;
        mesh->_vertexCount = vertexCount;
        mesh->_indexCount = indexCount;

        mesh->_aabb[0] = -0.5f;
        mesh->_aabb[1] = -0.5f;
        mesh->_aabb[2] = -0.5f;
        mesh->_aabb[3] = 0.5f;
        mesh->_aabb[4] = 0.5f;
        mesh->_aabb[5] = 0.5f;
    }

    void gfxMeshLoadBox( GfxMeshId id, GfxContext* ctx )
    {
        if( !ctx->_meshContainer.isValid( id ) )
            return;

        bxPolyShape shape;
        bxPolyShape_createBox( &shape, 1 );

        GfxMesh& mesh = ctx->_meshContainer.lookup( id );
        gfxMeshLoadShape( &mesh, shape );

        bxPolyShape_deallocateShape( &shape );
    }

    void gfxMeshLoadSphere( GfxMeshId id, GfxContext* ctx )
    {
        if( !ctx->_meshContainer.isValid( id ) )
            return;

        bxPolyShape shape;
        bxPolyShape_createShpere( &shape, 4 );

        GfxMesh& mesh = ctx->_meshContainer.lookup( id );
        gfxMeshLoadShape( &mesh, shape );

        

        bxPolyShape_deallocateShape( &shape );
    }

    //////////////////////////////////////////////////////////////////////////
    ////
    union GfxSortKeyColor
    {
        u64 hash;
        struct  
        {
            u64 mesh   : 16;
            u64 shader : 16;
            u16 depth  : 16;
            u64 layer  : 16;
        };
    };
    union GfxSortKeyDepth
    {
        u16 hash;
        u16 depth;
    };
    typedef bxGdiSortList< GfxSortKeyColor > GfxSortListColor;
    typedef bxGdiSortList< GfxSortKeyDepth > GfxSortListDepth;


    struct GfxScene
    {
        enum
        {
            eMAX_MESH_INSTANCES = 1024 * 16,
        };

        union MeshInstanceFlag
        {
            u8 all;
            struct  
            {
                u8 worldMatricesAllocated : 1;
            };
        };
        struct MatrixArray
        {
            Matrix4* data;
            i32 count;
        };
        struct MeshData
        {
            void* memoryHandle;
            i32 size;
            i32 capacity;            
            
            GfxMeshId*          meshId;
            GfxShaderId*        shaderId;
            bxAABB*             localAABB;
            Matrix4*            worldMatrix;
            MatrixArray*        worldMatrixArray;
            MeshInstanceFlag*   flag;
        };
        id_array_t<eMAX_MESH_INSTANCES> _meshIdArray;
        MeshData   _meshData;
        bxAllocator* _multiMatrixAllocator;
        bxAllocator* _mainAllocator;

        GfxSortKeyColor _colorSortList;
        GfxSortKeyDepth _depthSortList;

        u32 _worldMatrixBufferId;
        u32 _worldMatrixITBufferId;
        
    };
    void gfxSceneAllocateMeshData( GfxScene::MeshData* data, int newcap, bxAllocator* alloc )
    {}
    void gfxSceneFreeMeshData( GfxScene::MeshData* data, bxAllocator* alloc )
    {}
    void gfxSceneCreate( GfxScene** scene, GfxContext* ctx )
    {}
    void gfxSceneDestroy( GfxScene** scene, GfxContext* ctx )
    {}

    GfxMeshInstanceId gfxMeshInstanceCreate( GfxScene* scene, GfxMeshId meshId, GfxShaderId shaderId, int nInstances )
    {
        GfxMeshInstanceId id = { 0 };
        return id;
    }
    void gfxMeshInstanceDestroy( GfxMeshInstanceId* id, GfxScene* scene )
    {}
    void gfxSceneDraw( GfxScene* scene, GfxCommandQueue* cmdQueue, const GfxCamera& camera )
    {}

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
            glEnableVertexAttribArray( eATTRIB_POSITION );
            glVertexAttribPointer( eATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 0, 0 );

            gfxGLCheckError( "create vertex buffer" );

            ldata->_vertexBufferPositionId = vertexBufferId;
        }
        {
            glGenBuffers( 1, &vertexBufferId );
            glBindBuffer( GL_ARRAY_BUFFER, vertexBufferId );
            glBufferData( GL_ARRAY_BUFFER, initialCapacity * sizeof( *data->normal ), data->normal, GL_DYNAMIC_DRAW );
            glEnableVertexAttribArray( eATTRIB_NORMAL );
            glVertexAttribPointer( eATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, 0 );

            gfxGLCheckError( "create vertex buffer" );

            ldata->_vertexBufferNormalId = vertexBufferId;
        }

        {
            glGenBuffers( 1, &vertexBufferId );
            glBindBuffer( GL_ARRAY_BUFFER, vertexBufferId );
            glBufferData( GL_ARRAY_BUFFER, initialCapacity * sizeof( *data->color ), data->color, GL_DYNAMIC_DRAW );
            glEnableVertexAttribArray( eATTRIB_COLOR );
            glVertexAttribPointer( eATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0 );

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

    void gfxLinesDataFlush( GfxCommandQueue* cmdQueue, GfxLinesData* lines )
    {
        glBindVertexArray( lines->_vertexArrayId );
        gfxGLCheckError( "bind vertex array" );

        glDrawArrays( GL_LINES, 0, lines->_data.size );
        gfxGLCheckError( "draw" );

        glBindVertexArray( 0 );
        glUseProgram( 0 );
    }

    namespace util
    {
        void gfxLinesAddBox( GfxLinesData* lines, const Matrix4& pose, const Vector3& ext, u32 color )
        {
            //Vector3 positions[24] = 
        }
        void gfxLinesAddAxes( GfxLinesData* lines, const Matrix4& pose, int colorScheme )
        {
        
        }
    }///

}////
