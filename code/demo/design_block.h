#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>
//
////#include "renderer.h"
////#include "physics.h"
//

struct bxGdiDeviceBackend;
struct bxGdiShaderFx_Instance;

namespace bx
{
    struct GfxScene;
    struct PhxScene;


struct DesignBlock
{
    struct Handle
    {
        u32 h;
    };
    union Shape
    {
        enum
        {
            eSPHERE, eCAPSULE, eBOX,
        };
        vec_float4 vec4;
        struct
        {
            f32 param0, param1, param2;
            u32 type;
        };
        struct
        {
            f32 ex, ey, ez;
        };
        struct
        {
            f32 radius;
            f32 halfHeight;
        };

        Shape() : param0( -1.f ), param1( -1.f ), param2( -1.f ), type( UINT32_MAX ) {}
        explicit Shape( f32 sphere ) : radius( sphere ), param1( 0.f ), param2( 0.f ), type( eSPHERE ) {}
        explicit Shape( f32 capR, f32 capHH ) : radius( capR ), halfHeight( capHH ), param2( 0.f ), type( eCAPSULE ) {}
        explicit Shape( f32 boxHX, f32 boxHY, f32 boxHZ ) : ex( boxHX ), ey( boxHY ), ez( boxHZ ), type( eBOX ) {}
    };

    struct Desc
    {
        Shape shape;
        f32 density;
        bxGdiShaderFx_Instance* fxI;

        Desc()
            : shape( 0.5f )
            , density( 1.f )
            , fxI( nullptr )
        {}
    };

    ////
    ////
    Handle create( const char* name, const Matrix4& pose, const Desc& desc );
    void release( Handle* h );

    void cleanUp();
    void manageResources( GfxScene* gfxScene, PhxScene* phxScene );
    void tick();

    ////
    void assignTag( Handle h, u64 tag );

    ////
    ////
    const Matrix4& poseGet( Handle h ) const ;
    void poseSet( Handle h, const Matrix4& pose );
    const Shape& shapeGet( Handle h ) const;
};
//
void designBlockStartup( DesignBlock** dblock );
void designBlockShutdown( DesignBlock** dblock );
//

}///
#include <util/ascii_script.h>
namespace bx
{
struct DesignBlockSceneScriptCallback : public bxAsciiScript_Callback
{
    DesignBlockSceneScriptCallback();

    virtual void onCreate( const char* typeName, const char* objectName );
    virtual void onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData );
    virtual void onCommand( const char* cmdName, const bxAsciiScript_AttribData& args );

    DesignBlock* dblock;
    struct Desc
    {
        char name[256];
        char material[256];

        f32 density;
        DesignBlock::Shape shape;
        Matrix4 pose;
    } desc;
    void descClear();

};


}//

