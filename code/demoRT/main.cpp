#include <system/application.h>
#include <system/window.h>
#include <gdi/gdi_context.h>
#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include <resource_manager/resource_manager.h>

#include <util/vectormath/vectormath.h>
#include <util/type.h>
#include <util/memory.h>
#include <util/common.h>
#include <util/random.h>
#include <util/time.h>
#include <stdio.h>
#include "gfx/gfx_camera.h"

static const Vector4 spheres[] =
{
    Vector4(-4.f, 0.1f, 1.f, 0.5f ),    
    Vector4(-3.f, 0.2f,-2.f, 1.5f ),  
    Vector4(-2.f,-0.5f, 2.f, 1.1f ),         
    Vector4(-1.f,-0.3f, 1.f, 1.2f ), 
    Vector4( 0.f, 0.4f,-2.f, 2.3f ),         
    Vector4( 1.f,-0.5f, 1.f, 0.5f ),
    Vector4( 2.f, 0.1f,.5f , 0.72f ),         
    Vector4( 3.f,-0.2f, 0.f, 0.1f ),         
    Vector4( 0.f,-21.f, 0.f, 20.f),        
};
static const int nSpheres = sizeof( spheres ) / sizeof( *spheres );

static const Vector3 colors[] = 
{
    Vector3( 1.f, 0.f, 0.f ),
    Vector3( 0.f, 1.f, 0.f ),
    Vector3( 0.f, 0.f, 1.f ),
    Vector3( 1.f, 1.f, 0.f ),
    Vector3( 1.f, 0.f, 1.f ),
    Vector3( 0.f, 1.f, 1.f ),
    Vector3( 1.f, .5f, 0.f ),
    Vector3( 0.f, .5f, .5f ),
    Vector3( .8f, 0.f, 0.f ),
};
static const int nColors = sizeof(colors)/sizeof(*colors);
//SYS_STATIC_ASSERT( nColors == nSpheres );

static const Vector3 _sunDir = normalize( Vector3( 1.f, -1.f,-1.f ) );
static const Vector3 _sunColor = Vector3( 1.0f, 0.9f, 0.7f );

const Vector3 eye = Vector3( 0.f, 1.f, 15.f );
const Matrix3 cameraRot = inverse( Matrix4::lookAt( Point3(eye), Point3(0.f), Vector3::yAxis() ) ).getUpper3x3();

//static bxRandomGen rnd;
static int seed = 0xBAADC0DE;

namespace bxPathTracer
{
    float frand( int *seed )
    {
        union
        {
            float fres;
            unsigned ires;
        };

        seed[0] *= 16807;

        ires = ((((unsigned)seed[0])>>9 ) | 0x3f800000);
        return fres - 1.0f;
    }
    
    inline float rndf() { return frand( &seed ); }

    floatInVec interesctSphere( const Vector3& rO, const Vector3& rD, const Vector4& sph )
    {
        const Vector3 p = rO - sph.getXYZ();
        const floatInVec b = dot( p, rD );
        const floatInVec c = dot( p, p ) - sph.getW()*sph.getW();
        const floatInVec h = b*b - c;
        const floatInVec h1 = -b - floatInVec( sqrtf4( h.get128() ) );
        
        const floatInVec result = select( h, h1, h > zeroVec );
        return result;
    }
    
    Vector2 worldIntersect( const Vector3& ro, const Vector3& rd, const floatInVec& maxLen )
    {
        Vector2 result( FLT_MAX, -1.f );
        for( int isphere = 0; isphere < nSpheres; ++isphere )
        {
            const float t = interesctSphere( ro, rd, spheres[isphere] ).getAsFloat();
            if( ( t> 0.f && t < result.x ) )
            {
                result = Vector2( t, (float)isphere );
            }
        }
        return result;
    }

    Vector3 ortho( const Vector3& v ) 
    {
        //  See : http://lolengine.net/blog/2013/09/21/picking-orthogonal-vector-combing-coconuts
        return select( Vector3( zeroVec, -v.getZ(), v.getY() ), Vector3(-v.getY(), v.getX(), zeroVec ), absf4( v.getX() ) > absf4( v.getZ() ) );
        //return abs(v.x) > abs(v.z) ? vec3(-v.y, v.x, 0.0)  : vec3(0.0, -v.z, v.y);
    }

    Vector3 getSampleBiased( const Vector3& dir, float power  ) 
    {
        const Vector3 dirN = normalize(dir);
        const Vector3 o1 = normalize(ortho(dirN));
        const Vector3 o2 = normalize(cross(dirN, o1));
        Vector2 r = Vector2( rndf(), rndf() );
        r.x = r.x*2.f*PI;
        r.y = powf(r.y,1.0f/(power+1.0f));
        float oneminus = sqrt(1.0f-r.y*r.y);
        return o1 * cos(r.x)*oneminus + sin(r.x)*oneminus*o2 + r.y*dirN;
    }

    Vector3 cosWeightedRandomHemisphereDirection2( const Vector3& n )
    {
        const Vector3 xi( rndf(), rndf(), 0.f );

        const floatInVec theta1( acosf4( sqrtf4( ( oneVec - xi.getX() ).get128() ) ) );
        const floatInVec phi1( floatInVec( PI2 ) * xi.getY() );

        Vector4 sThetaPhi, cThetaPhi, sPhi, cPhi;
        const Vector4 args( theta1, phi1, zeroVec, zeroVec );
        sincosf4( args.get128(), (vec_float4*)&sThetaPhi, (vec_float4*)&cThetaPhi );

        const floatInVec xs = sThetaPhi.getX() * cThetaPhi.getY();
        const floatInVec ys = cThetaPhi.getX();
        const floatInVec zs = sThetaPhi.getX() * sThetaPhi.getY();

        //float Xi1 = rndf(); //(float)rand()/(float)RAND_MAX;
        //float Xi2 = rndf(); //(float)rand()/(float)RAND_MAX;

        //float  theta = acos( sqrt( 1.0f-Xi1 ) );
        //float  phi = PI2 * Xi2;

        //float xs = sinf(theta) * cosf(phi);
        //float ys = cosf(theta);
        //float zs = sinf(theta) * sinf(phi);

        Vector3 y = n;
        Vector3 h = ortho( y );


        //const Vector3 hAbs = absPerElem( h );

        //if( ( hAbs.getX() <= hAbs.getY() & hAbs.getX()<=hAbs.getZ() ).getAsBool() )
        //{
        //    h.setX( oneVec );
        //}
        //else if( ( hAbs.getY()<=hAbs.getX() & hAbs.getY()<=hAbs.getZ() ).getAsBool() )
        //{
        //    h.setY( oneVec );
        //}
        //else
        //{
        //    h.setZ( oneVec );
        //}
        //if (fabs(h.x)<=fabs(h.y) && fabs(h.x)<=fabs(h.z))
        //{
        //    h.x= 1.0;
        //}
        //else if (fabs(h.y)<=fabs(h.x) && fabs(h.y)<=fabs(h.z))
        //{
        //    h.y= 1.0;
        //}
        //else
        //{
        //    h.z= 1.0;
        //}
        const Vector3 x = normalize( cross( h, y) );
        const Vector3 z = normalize( cross( x, y) );
        const Vector3 direction = xs * x + ys * y + zs * z;
        return normalize( direction );
    }

    Vector3 cosineSampleHemisphere(float u1, float u2)
    {
        const float r = sqrt(u1);
        const float theta = PI2 * u2;

        const float x = r * cos(theta);
        const float y = r * sin(theta);

        return normalize( Vector3(x, y, sqrt(maxOfPair(0.0f, 1.f - u1))) );
    }

    Vector3 getCosineWeightedSample( const Vector3& dir ) 
    {
        return cosWeightedRandomHemisphereDirection2( dir );
        //return cosineSampleHemisphere( rnd.getf(0.f,1.f), rnd.getf(0.f,1.f) );
        //return getSampleBiased( dir, 1.0f, rnd );
    }
    floatInVec worldShadow( const Vector3& ro, const Vector3& rd, const floatInVec& maxLen )
    {
        const Vector2 tres = worldIntersect( ro, rd, floatInVec( 1000.f ) );
        return (tres.y < 0.f) ? oneVec : zeroVec;
    }

    Vector3 worldGetNormal( const Vector3& po, float objectID )
    {
        const Vector4& sph = spheres[(int)objectID];
        return normalize( po - sph.getXYZ() );

    }
    Vector3 worldGetColor( const Vector3& po, const Vector3& no, float objectID )
    {
        return colors[ (int)objectID ];

    }
    Vector3 worldGetBackgound( const Vector3& rd ) { (void)rd; return Vector3( 0.5f, 0.6f, 0.7f ); }

    Vector3 worldApplyLighting( const Vector3& pos, const Vector3& nor )
    {
        const Vector3 L = normalize( -_sunDir *1000.f - pos );
        const floatInVec sh = worldShadow( pos, L, floatInVec( 1000.f ) );

        return _sunColor * clampf4( dot( nor, -_sunDir ), zeroVec, oneVec ) * sh;
    }
    Vector3 worldGetBRDFRay( const Vector3& pos, const Vector3& nor, const Vector3& eye, float materialID )
    {
        return getCosineWeightedSample( nor );
    }
    Vector3 reflect( const Vector3& N, const Vector3& L )
    {
        return twoVec * dot( N, L ) * N - L;
    }
    Vector3 renderCalculateColor( const Vector3& rayOrig, const Vector3& rayDir, int nLevels )
    {
        Vector3 tcol(0.f);
        Vector3 fcol(1.f);

        Vector3 ro = rayOrig;
        Vector3 rd = rayDir;

        const float strengthScale = 1.f / nLevels;
        float strength = 1.f;
        

        for( int ilevel = 0; ilevel < nLevels; ++ilevel )
        {
            const Vector2 tres = worldIntersect( ro, rd, floatInVec( 1000.f ) );
            if ( tres.y < 0.0f )
            {
                if( ilevel == 0 ) return worldGetBackgound( rd );    
                else break;
            }

            const Vector3 wPos = ro + rd * tres.x;
            const Vector3 wNrm = worldGetNormal( wPos, tres.y );
            const Vector3 matCol = worldGetColor( wPos, wNrm, tres.y );
            const Vector3 litCol = mulPerElem( worldApplyLighting( wPos, wNrm ), matCol ) * PI_INV;

            ro = wPos;
            //rd = normalize( reflect( wNrm, -rd ) );//worldGetBRDFRay( wPos, wNrm, rd, tres.y, rnd );    
            rd = worldGetBRDFRay( wPos, wNrm, rd, tres.y );
            
            fcol = mulPerElem( fcol, matCol ); // * strength;
            //if( ilevel > 0 )
            tcol += mulPerElem( fcol, litCol );

            strength -= strengthScale;
        }
        
        return tcol;
    }

    Vector3 calculatePixelColor( const Vector3& pixel, const Vector3& resolution, int nSamples, int nLevels )
    {
        Vector3 color(0.f);

        

        for( int isample = 0; isample < nSamples; ++isample )
        {
            const Vector3 shift = Vector3( rndf(), rndf(), 0.f );//bxRand::randomVector2( rnd, Vector3(0.f), Vector3(1.f) );
            //const Vector3 p = ( ( pixel ) * resolution.getX() / resolution.getY() ) * twoVec - Vector3( oneVec );
            const Vector3 p = ( -resolution + ( pixel + shift ) * twoVec ) / resolution.getY();
            
            const Vector3 rayDir = normalize( cameraRot.getCol0() * p.getX() + cameraRot.getCol1() * p.getY() - cameraRot.getCol2() * 2.5f );
            const Vector3 rayOrg = eye;

            color += renderCalculateColor( rayOrg, rayDir, nLevels );
        }

        color *= 1.f / float( nSamples );

        return color;
    }
    
    union Vec3
    {
        f32 xyz[3];
        f32 rgb[3];
        struct
        {
            f32 r, g, b;
        };
        struct
        {
            f32 x, y, z;
        };

        explicit Vec3( f32 v ): r(v), g(v), b(v) {}
        Vec3( f32 v0, f32 v1, f32 v2 ): r(v0), g(v1), b(v2) {}
    };
    

    inline int toInt( float x ){ return int( powf( clamp( x, 0.f, 1.f ), 0.454545455f )*255.0f + .5f ); }

    void doPathTracing( int w, int h, int samps, int bounces )
    {
        //rnd = bxRandomGen( (u32)bxTime::ms() );
        seed = (u32)bxTime::ms();
        const Vector3 resolution = Vector3( (float)w, (float)h, 1.f );
        Vector3* c = (Vector3*)BX_MALLOC( bxDefaultAllocator(), w*h *sizeof( Vector3 ), 4 );
        memset( c, 0x00, w*h*sizeof(Vector3) );
        //int i = 0;
        for ( unsigned short y = 0; y<h; y+=1 )
        {            // Loop over image rows
            for ( unsigned short x = 0; x<w; x+=1 )   // Loop cols
            {
                fprintf( stderr, "\rRendering (%d samples/pp) %5.2f%%", samps, 100.*y / (h - 1) );
                const Vector3 pixel = Vector3( (float)x, (float)( w - y ), 0.f );
                const Vector3 col = calculatePixelColor( pixel, resolution, samps, bounces );
               
                const int i = (y) * w + x;
                c[i] = col;

                //for( int yy = -2; yy <= 2; ++yy )
                //{
                //    for( int xx = -2; xx <= 2; ++xx )
                //    {
                //        const int i = clamp( (y+yy) * w + (x+xx), 0, w*h );
                //        const float w = weights[yy+2][xx+2];
                //        c[i] += col;

                //        //m128_to_xyz( c[i].xyz, col.get128() );
                //    }
                //}
                
                
                
                //i+=4;
                //c[i++] = Vec3(1.f);
            }
        }
        FILE *f = NULL;
        fopen_s( &f, "image.ppm", "w" );         // Write image to PPM file.
        fprintf( f, "P3\n%d %d\n%d\n", w, h, 255 );
        for ( int i = 0; i<w*h; i++ )
        {
            Vec3 tmp(0.f);
            m128_to_xyz( tmp.xyz, c[i].get128() );
            fprintf( f, "%d %d %d ", toInt( tmp.x ), toInt( tmp.y ), toInt( tmp.z ) );
        }

        fclose( f );

        BX_FREE0( bxDefaultAllocator(), c );
    }
}///

class bxDemoRT : public bxApplication
{
public:
    bxDemoRT()
        : _timeUS(0)
    {}
    virtual ~bxDemoRT() {}

    virtual bool startup( int argc, const char** argv )
    {
        bxWindow* win = bxWindow_get();
        //_resourceManager = bxResourceManager::startup( "d:/dev/code/bitBox/assets/" );
        _resourceManager = bxResourceManager::startup( "d:/tmp/bitBox/assets/" );
        bxGdi::backendStartup( &_gdiDevice, (uptr)win->hwnd, win->width, win->height, win->full_screen );

        _gdiContext = BX_NEW( bxDefaultAllocator(), bxGdiContext );
        _gdiContext->_Startup( _gdiDevice );

        {//// fullScreenQuad
            const float vertices[] =
            {
                -1.f, -1.f, 0.f, 0.f, 0.f,
                1.f, -1.f, 0.f, 1.f, 0.f,
                1.f, 1.f, 0.f, 1.f, 1.f,

                -1.f, -1.f, 0.f, 0.f, 0.f,
                1.f, 1.f, 0.f, 1.f, 1.f,
                -1.f, 1.f, 0.f, 0.f, 1.f,
            };
            bxGdiVertexStreamDesc vsDesc;
            vsDesc.addBlock( bxGdi::eSLOT_POSITION, bxGdi::eTYPE_FLOAT, 3 );
            vsDesc.addBlock( bxGdi::eSLOT_TEXCOORD0, bxGdi::eTYPE_FLOAT, 2 );
            bxGdiVertexBuffer vBuffer = _gdiDevice->createVertexBuffer( vsDesc, 6, vertices );

            _fullScreenQuad = bxGdi::renderSource_new( 1 );
            bxGdi::renderSource_setVertexBuffer( _fullScreenQuad, vBuffer, 0 );
        }
        {
            _fxI = bxGdi::shaderFx_createWithInstance( _gdiDevice, _resourceManager, "pathtracer" );
        }
        {
            _spheresBuffer = _gdiDevice->createBuffer( 1024, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
            _colorsBuffer  = _gdiDevice->createBuffer( 1024, bxGdiFormat( bxGdi::eTYPE_FLOAT, 3 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );

        }

        const Vector3 eye = Vector3( 2.f, 5.f, 25.f );
        _camera_worldMatrix = inverse( Matrix4::lookAt( Point3(eye), Point3(0.f), Vector3::yAxis() ) );
        
        
        return true;

    }
    virtual void shutdown()
    {
        _gdiDevice->releaseBuffer( &_colorsBuffer );
        _gdiDevice->releaseBuffer( &_spheresBuffer );

        bxGdi::shaderFx_releaseWithInstance( _gdiDevice, &_fxI );
        bxGdi::renderSource_releaseAndFree( _gdiDevice, &_fullScreenQuad );

        _gdiContext->_Shutdown();
        BX_DELETE0( bxDefaultAllocator(), _gdiContext );

        bxGdi::backendShutdown( &_gdiDevice );
        bxResourceManager::shutdown( &_resourceManager );
    }
    virtual bool update( u64 deltaTimeUS )
    {
        bxWindow* win = bxWindow_get();
        if ( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_ESC ) )
        {
            return false;
        }

        const float deltaTime = (float)bxTime::toSeconds( deltaTimeUS );

        bxGfx::cameraUtil_updateInput( &_camera_inputCtx, &win->input, 1.f, deltaTime );
        _camera_worldMatrix = bxGfx::cameraUtil_movement( _camera_worldMatrix
            , _camera_inputCtx.leftInputX * 0.25f 
            , _camera_inputCtx.leftInputY * 0.25f 
            , _camera_inputCtx.rightInputX * deltaTime * 5.f
            , _camera_inputCtx.rightInputY * deltaTime * 5.f
            , _camera_inputCtx.upDown * 0.25f );

        const float2_t resolution = float2_t(( float)win->width, (float)win->height );
        const float aspect = resolution.x / resolution.y;

        _fxI->setUniform( "_cameraX", _camera_worldMatrix.getCol0() );
        _fxI->setUniform( "_cameraY", _camera_worldMatrix.getCol1() );
        _fxI->setUniform( "_cameraZ", _camera_worldMatrix.getCol2() );
        _fxI->setUniform( "_cameraEye", _camera_worldMatrix.getTranslation() );
        _fxI->setUniform( "_sunDir", _sunDir );
        _fxI->setUniform( "_sunColor", _sunColor );
        _fxI->setUniform( "_resolution", resolution );
        _fxI->setUniform( "_aspect", aspect );
        _fxI->setUniform( "_time", bxTime::toSeconds( _timeUS ) );
        _fxI->setUniform( "_numSpheres", nSpheres );
        {
            u8* dstData = _gdiContext->backend()->map( _spheresBuffer.rs, 0 );
            memcpy( dstData, spheres, sizeof( spheres ) );
            _gdiContext->backend()->unmap( _spheresBuffer.rs );
        }
        {
            u8* dstData = _gdiContext->backend()->map( _colorsBuffer.rs, 0 );
            memcpy( dstData, colors, sizeof( colors ) );
            _gdiContext->backend()->unmap( _colorsBuffer.rs );
        }
        
        _gdiContext->changeToMainFramebuffer();
        _gdiContext->clearBuffers( 0.f, 0.f, 0.f, 1.f, 1.f, 1, 0 );
        _gdiContext->setBufferRO( _spheresBuffer, 0, bxGdi::eSTAGE_MASK_PIXEL );
        _gdiContext->setBufferRO( _colorsBuffer, 1, bxGdi::eSTAGE_MASK_PIXEL );

        bxGdi::renderSource_enable( _gdiContext, _fullScreenQuad );
        bxGdi::shaderFx_enable( _gdiContext, _fxI, 0 );

        bxGdi::renderSurface_draw( _gdiContext, bxGdi::renderSource_surface( _fullScreenQuad, bxGdi::eTRIANGLES ) );

        _gdiContext->backend()->swap();

        _timeUS += deltaTimeUS;

        return true;

    }

    bxGdiDeviceBackend* _gdiDevice;
    bxGdiContext*       _gdiContext;
    bxResourceManager* _resourceManager;

    bxGdiShaderFx_Instance* _fxI;
    bxGdiRenderSource* _fullScreenQuad;

    bxGdiBuffer _spheresBuffer;
    bxGdiBuffer _colorsBuffer;

    bxGfxCamera_InputContext _camera_inputCtx;
    Matrix4 _camera_worldMatrix;

    u64 _timeUS;
};


int main( int argc, const char* argv[] )
{
    //bxPathTracer::doPathTracing( 1024, 1024, 2048, 16 );

    bxWindow* window = bxWindow_create( "demo", 640, 640, false, 0 );
    if ( window )
    {
        bxDemoRT app;
        if ( bxApplication_startup( &app, argc, argv ) )
        {
            bxApplication_run( &app );
        }

        bxApplication_shutdown( &app );
        bxWindow_release();
    }

    return 0;
}