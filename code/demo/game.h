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
    
    void character_init( Character* character, const Matrix4& worldPose );
    void character_tick( Character* character, const bxGfxCamera& camera, const bxInput& input, float deltaTime );

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

    virtual Handle create( const char* name ) = 0;
    virtual void release( Handle* h ) = 0;
    virtual void cleanUp( bxPhx_CollisionSpace* cs, bxGfx_HWorld gfxWorld ) = 0;
    
    virtual void assignTag( Handle h, u64 tag ) = 0;
    virtual void assignMesh( Handle h, bxGfx_HMeshInstance meshi ) = 0;
    virtual void assignCollisionShape( Handle h, bxPhx_HShape hshape ) = 0;

    virtual void tick( bxPhx_CollisionSpace* cs ) = 0;
};
bxDesignBlock* bxDesignBlock_new();
void bxDesignBlock_delete( bxDesignBlock** dblock );

//struct bxDesignBlock_HBlock;
//namespace bxDesignBlock
//{
//    bxDesignBlock_HBlock block_create();
//    void block_release( bxDesignBlock_HBlock* h );
//
//    void assignTag( bxDesignBlock_HBlock h, u64 tag );
//    void assignMesh( bxDesignBlock_HBlock h, bxGfx_HInstanceBuffer hinstance );
//    void assignCollisionShape( bxDesignBlock_HBlock h, bxPhysics_HBoxShape hc );
//    void assignCollisionShape( bxDesignBlock_HBlock h, bxPhysics_HSphereShape hc );
//    void assignCollisionShape( bxDesignBlock_HBlock h, bxPhysics_HCapsuleShape hc );
//    void assignCollisionShape( bxDesignBlock_HBlock h, bxPhysics_HPlaneShape hc );
//
//
//
//}//

