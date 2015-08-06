#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>
#include "renderer.h"
#include "physics.h"

struct bxInput;
struct bxGfxCamera;

namespace bxGame
{
    struct Character;

    Character* character_new();
    void character_delete( Character** character );
    
    void character_init( Character* character, bxResourceManager* resourceManager, const Matrix4& worldPose );
    void character_deinit( Character* character, bxResourceManager* resourceManager );
    void character_tick( Character* character, bxPhx_CollisionSpace* cspace, const bxGfxCamera& camera, const bxInput& input, float deltaTime );

    Matrix4 character_pose( const Character* character );
    Vector3 character_upVector( const Character* character );
}///

namespace bxGame
{
    void characterCamera_follow( bxGfxCamera* camera, const Character* character, float deltaTime, int cameraMoved = 0 );
}///

namespace bxGame
{
    struct Flock;
    Flock* flock_new();
    void flock_delete( Flock** flock );

    void flock_init( Flock* flock, int nBoids, const Vector3& center, float radius );
    void flock_loadResources( Flock* flock, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxGfx_HWorld gfxWorld );
    
    struct Predator
    {
        enum { eMAX_COUNT = 4, };
        Vector3 pos[ eMAX_COUNT ];
        Vector3 vel[ eMAX_COUNT ];

        i32 count;
    };
    
    void flock_tick( Flock* flock, float deltaTime );
}///


struct bxDesignBlock
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

        Shape(): param0(-1.f), param1(-1.f), param2(-1.f), type( UINT32_MAX ) {}
        explicit Shape( f32 sphere ): radius( sphere ), param1(0.f), param2(0.f), type( eSPHERE ) {}
        explicit Shape( f32 capR, f32 capHH ) : radius( capR), halfHeight( capHH ), param2( 0.f ), type( eCAPSULE ) {}
        explicit Shape( f32 boxHX, f32 boxHY, f32 boxHZ ) : ex( boxHX ), ey( boxHY), ez( boxHZ ), type( eBOX ) {}
    };

    ////
    ////
    virtual Handle create( const char* name, const Matrix4& pose, const Shape& shape, const char* material = "green" ) = 0;
    virtual void release( Handle* h ) = 0;
    
    virtual void cleanUp() = 0;
    virtual void manageResources( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxPhx_CollisionSpace* cs, bxGfx_HWorld gfxWorld ) = 0;
    virtual void tick( bxPhx_CollisionSpace* cs, bxGfx_HWorld gfxWorld ) = 0;

    virtual void assignTag( Handle h, u64 tag ) = 0;
    //virtual void assignMesh( Handle h, bxGfx_HMeshInstance meshi ) = 0;
    //virtual void assignCollisionShape( Handle h, bxPhx_HShape hshape ) = 0;

    ////
    ////
    static Matrix4 poseGet( bxDesignBlock* dblock, Handle h );
    static void poseSet( bxDesignBlock* dblock, Handle h, const Matrix4& pose );

    static Shape shapeGet( bxDesignBlock* dblock, Handle h );
    //static void shapeSet( bxDesignBlock* dblock, Handle h, const Shape& shape );
};

bxDesignBlock* bxDesignBlock_new();
void bxDesignBlock_delete( bxDesignBlock** dblock );

#include <util/ascii_script.h>
struct bxDesignBlock_SceneScriptCallback : public bxAsciiScript_Callback
{
    bxDesignBlock_SceneScriptCallback();

    virtual void onCreate( const char* typeName, const char* objectName );
    virtual void onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData );
    virtual void onCommand( const char* cmdName, const bxAsciiScript_AttribData& args );

    bxDesignBlock* dblock;
    struct Desc
    {
        char name[256];
        char material[256];

        bxDesignBlock::Shape shape;
        Matrix4 pose;
    } desc;
};

