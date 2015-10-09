#pragma once

#include "gfx_public.h"
#include <util/vectormath/vectormath.h>
#include <util/pool_allocator.h>
#include <util/thread/mutex.h>
#include <util/handle_manager.h>

#include <gdi/gdi_backend.h>
#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include <resource_manager/resource_manager.h>
namespace bx
{
    enum EFramebuffer
    {
        eFB_COLOR0,
        eFB_DEPTH,
        eFB_COUNT,
    };

    struct GfxActor
    {
        virtual ~GfxActor() {}

        virtual void load( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager ) {}
        virtual void unload( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager ) {}

        virtual GfxCamera*       isCamera() { return nullptr; }
        virtual GfxScene*        isScene() { return nullptr; }
        virtual GfxMeshInstance* isMeshInstance() { return nullptr; }
    };

    struct GfxCamera : public GfxActor
    {
        Matrix4 world;
        Matrix4 view;
        Matrix4 proj;
        Matrix4 viewProj;

        f32 hAperture;
        f32 vAperture;
        f32 focalLength;
        f32 zNear;
        f32 zFar;
        f32 orthoWidth;
        f32 orthoHeight;

        GfxContext* _ctx;
        u32 _internalHandle;

        GfxCamera();

        virtual GfxCamera* isCamera() { return this; }
    };

    struct GfxInstanceData;
    struct GfxScene : public GfxActor
    {
        struct Data
        {
            void* memoryHandle;
            i32 size;
            i32 capacity;

            u32* meshHandle;
            bxGdiRenderSource** _rsource;
            bxGdiShaderFx_Instance** _fxInstance;
            bxAABB* _bbox;
            GfxInstanceData* _idata;
        };

        Data _data;

        GfxContext* _ctx;
        u32 _internalHandle;

        virtual GfxScene* isScene() { return this; }
    };

    struct GfxInstanceData
    {
        Matrix4* pose;
        i32 count;

        GfxInstanceData()
            : pose( nullptr )
            , count( 1 )
        {}
    };

    struct GfxMeshInstance : public GfxActor
    {
        GfxContext* _ctx;
        GfxScene* _scene;
        u32 _internalHandle;

        bxGdiRenderSource* _rsource;
        bxGdiShaderFx_Instance* _fxI;
        GfxInstanceData _idata;
        float localAABB[6];

        GfxMeshInstance();

        virtual GfxMeshInstance* isMeshInstance() { return this; }
    };

    struct GfxViewFrameParams
    {
        Matrix4 _camera_view;
        Matrix4 _camera_proj;
        Matrix4 _camera_viewProj;
        Matrix4 _camera_world;
        float4_t _camera_eyePos;
        float4_t _camera_viewDir;
        //float4_t _camera_projParams;
        float4_t _reprojectInfo;
        float4_t _reprojectInfoFromInt;
        float2_t _renderTarget_rcp;
        float2_t _renderTarget_size;
        float _camera_fov;
        float _camera_aspect;
        float _camera_zNear;
        float _camera_zFar;
        float _reprojectDepthScale; // (g_zFar - g_zNear) / (-g_zFar * g_zNear)
        float _reprojectDepthBias; // g_zFar / (g_zFar * g_zNear)
    };
    void gfxViewFrameParamsFill( GfxViewFrameParams* fparams, const GfxCamera* camera, int rtWidth, int rtHeight );

    struct GfxView
    {
        bxGdiBuffer _viewParamsBuffer;
        bxGdiBuffer _instanceWorldBuffer;
        bxGdiBuffer _instanceWorldITBuffer;

        i32 _maxInstances;

        GfxView();
    };
    void gfxViewCreate( GfxView* view, bxGdiDeviceBackend* dev, int maxInstances );
    void gfxViewDestroy( GfxView* view, bxGdiDeviceBackend* dev );

    struct GfxContext;
    struct GfxCommandQueue
    {
        GfxView _view;
        u32 _acquireCounter;
        GfxContext* _ctx;
        bxGdiDeviceBackend* _device;

        GfxCommandQueue();
    };

    typedef bxHandleManager< GfxActor* > ActorHandleManager;

    template< class Talloc, class Tlock >
    struct AllocatorThreadSafe : public Talloc
    {
        virtual ~AllocatorThreadSafe() {}
        virtual void* alloc( size_t size, size_t align )
        {
            _lock.lock();
            void* ptr = Talloc::alloc( size, align );
            _lock.unlock();
            return ptr;
        }
        virtual void  free( void* ptr )
        {
            _lock.lock();
            Talloc::free( ptr );
            _lock.unlock();
        }

        Tlock _lock;
    };
    typedef AllocatorThreadSafe< bxDynamicPoolAllocator, bxBenaphore > DynamicPoolAllocatorThreadSafe;

    struct GfxContext
    {
        GfxCommandQueue _cmdQueue;

        bxGdiTexture _framebuffer[eFB_COUNT];

        bxAllocator* _allocMesh;
        bxAllocator* _allocCamera;
        bxAllocator* _allocScene;
        bxAllocator* _allocIDataMulti;
        bxAllocator* _allocIDataSingle;

        ActorHandleManager _handles;
        bxRecursiveBenaphore _lockHandles;

        bxBenaphore _lockActorsToRelease;
        array_t< GfxActor* > _actorsToRelease;

        GfxContext();
    };
}///