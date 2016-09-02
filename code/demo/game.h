#pragma once

#include <util/vectormath/vectormath.h>
#include <util/type.h>
#include <gfx/gfx.h>
#include "physics.h"

struct bxInput;
struct bxGfxCamera;
struct bxGdiContextBackend;

namespace bx
{
    struct GameScene;

    struct Character;
    Character* character_new();
    void character_delete( Character** character );

    void characterInit( Character* character, bxGdiDeviceBackend* dev, GameScene* scene, const Matrix4& worldPose );
    void characterDeinit( Character* character, bxGdiDeviceBackend* dev );
    void characterTick( Character* character, bxGdiContextBackend* gdi, GameScene* scene, GfxCamera* camera, const bxInput& input, float deltaTime );

    Matrix4 characterPoseGet( const Character* character );
    Vector3 characterUpVectorGet( const Character* character );
};

namespace bx
{
    struct CameraController
    {
        f32 _dtAcc = 0.f;
		f32 _cameraMoved = 0.f;
        void follow( bx::GfxCamera* camera, const Vector3& characterPos, const Vector3& characterUpVector, const Vector3& characterDeltaPos, float deltaTime, int cameraMoved = 0 );
    };
}///

//namespace bxGame
//{
//    struct Flock;
//    Flock* flock_new();
//    void flock_delete( Flock** flock );
//
//    void flock_init( Flock* flock, int nBoids, const Vector3& center, float radius );
//    void flock_loadResources( Flock* flock, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
//    
//    struct Predator
//    {
//        enum { eMAX_COUNT = 4, };
//        Vector3 pos[ eMAX_COUNT ];
//        Vector3 vel[ eMAX_COUNT ];
//
//        i32 count;
//    };
//    
//    void flock_tick( Flock* flock, float deltaTime );
//}///


namespace bx
{
    struct CharacterInput
    {
        f32 analogX = 0.f;
        f32 analogY = 0.f;
        f32 jump = 0.f;
        f32 crouch = 0.f;
        f32 L2 = 0.f;
        f32 R2 = 0.f;
    };
    void characterInputCollectData( CharacterInput* charInput, const bxInput& sysInput, float deltaTime, float RC = 0.01f );

	struct CharacterController
    {
        static void create( CharacterController** cc, GameScene* scene, const Matrix4& worldPose );
        static void destroy( CharacterController** cc, GameScene* scene );
        
        CharacterController() {}
        virtual ~CharacterController() {}

        virtual void tick( GameScene* scene, GfxCamera* camera, const bxInput& input, float deltaTime ) = 0;
        virtual Matrix4 worldPose() const = 0;
		virtual Matrix4 worldPoseFoot() const = 0;
        virtual Vector3 upDirection() const = 0;
		virtual Vector3 footPosition() const = 0;
		virtual Vector3 centerPosition() const = 0;
		virtual Vector3 deltaPosition() const = 0;
        virtual Vector3 velocity() const = 0;
    };
    
	//////////////////////////////////////////////////////////////////////////
	struct CharacterAnimController;
	void charAnimControllerCreate( CharacterAnimController** canim, GameScene* scene );
	void charAnimControllerDestroy( CharacterAnimController** canim );
	void charAnimControllerTick( CharacterAnimController* canim, GameScene* scene, u64 deltaTimeUS );

    //////////////////////////////////////////////////////////////////////////
    struct CharacterAnim;
    void charAnimCreate( CharacterAnim** canim, GameScene* scene, const Matrix4& worldPose, float scale );
    void charAnimDestroy( CharacterAnim** canim, GameScene* scene );
    void charAnimTick( CharacterAnim* canim, const Matrix4& worldPose, float deltaTime );
	
    //////////////////////////////////////////////////////////////////////////
    struct CharacterGfx;
    void charGfxCreate( CharacterGfx** cgfx, GameScene* scene );
    void charGfxDestroy( CharacterGfx** cgfx, GameScene* scene );
    void charGfxTick( CharacterGfx* cgfx, GameScene* scene );

}////

