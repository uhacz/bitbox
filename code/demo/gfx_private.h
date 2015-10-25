#pragma once

#include "gfx_public.h"
#include <util/vectormath/vectormath.h>
#include <util/pool_allocator.h>
#include <util/thread/mutex.h>
#include <util/handle_manager.h>
#include <util/hash.h>

#include <gdi/gdi_backend.h>
#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>
#include <gdi/gdi_sort_list.h>
#include <resource_manager/resource_manager.h>
namespace bx
{
    enum EFramebuffer
    {
        eFB_COLOR0,
        eFB_SAO,
        eFB_SHADOW,
        eFB_DEPTH,
        eFB_TEMP0,
        eFB_TEMP1,
        eFB_COUNT,
    };

    enum EResourceSlot
    {
        eRS_CBUFFER_VIEW_PARAMS = 0,
        eRS_CBUFFER_INSTANCE_OFFSET = 1,
        eRS_CBUFFER_LIGHTS = 2,
        eRS_CBUFFER_MATERIAL = 3,

        eRS_BUFFER_INSTANCE_WORLD = 0,
        eRS_BUFFER_INSTANCE_WORLD_IT = 1,
        eRS_BUFFER_LIGHT = 2,
        eRS_BUFFER_LIGHT_INDICES = 3,    

        eRS_TEXTURE_SAO = 4,
        eRS_TEXTURE_SHADOW = 5,
    };

    struct GfxSunLight;
    struct GfxActor
    {
        virtual ~GfxActor() {}

        virtual void load( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager ) {}
        virtual void unload( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager ) {}

        virtual GfxCamera*       isCamera() { return nullptr; }
        virtual GfxScene*        isScene() { return nullptr; }
        virtual GfxMeshInstance* isMeshInstance() { return nullptr; }
        virtual GfxSunLight*     isSunLight() { return nullptr; }
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

    //////////////////////////////////////////////////////////////////////////
    ////
    struct GfxSunLight : public GfxActor
    {
        float3_t _direction;
        float _angularRadius;
        float _sunIlluminanceInLux;
        float _skyIlluminanceInLux;

        GfxContext* _ctx;
        u32 _internalHandle;

        GfxSunLight();
        virtual GfxSunLight* isSunLight() { return this; }
    };

    //////////////////////////////////////////////////////////////////////////
    ////
    union GfxSortKeyColor
    {
        u64 hash;
        struct
        {
            u64 instance : 12;
            u64 mesh : 16;
            u64 shader : 32;
            u64 layer : 4;
        };
    };
    union GfxSortKeyDepth
    {
        u16 hash;
        u16 depth;
    };
    union GfxSortKeyShadow
    {
        u32 hash;
        struct
        {
            u16 depth;
            u16 cascade;
        };
    };
    template <class Tkey>
    struct GfxSortItem
    {
        Tkey key;
        i32 index;
    };
    typedef  GfxSortItem< GfxSortKeyColor > GfxSortItemColor;
    typedef  GfxSortItem< GfxSortKeyDepth > GfxSortItemDepth;
    typedef  GfxSortItem< GfxSortKeyShadow > GfxSortItemShadow;

    inline bool operator < (const GfxSortItemColor a, const GfxSortItemColor b) { return a.key.hash < b.key.hash; }
    inline bool operator < (const GfxSortItemDepth a, const GfxSortItemDepth b)   { return a.key.hash < b.key.hash; }
    inline bool operator < (const GfxSortItemShadow a, const GfxSortItemShadow b) { return a.key.hash < b.key.hash; }

    typedef bxGdiSortList< GfxSortItemColor > GfxSortListColor;
    typedef bxGdiSortList< GfxSortItemDepth > GfxSortListDepth;
    typedef bxGdiSortList< GfxSortItemShadow > GfxSortListShadow;

    struct GfxInstanceData;
    struct GfxScene : public GfxActor
    {
        struct Data
        {
            bxAllocator* alloc;
            void* memoryHandle;
            i32 size;
            i32 capacity;

            u32* meshHandle;
            bxGdiRenderSource** rsource;
            bxGdiShaderFx_Instance** fxInstance;
            bxAABB* bbox;
            GfxInstanceData* idata;
        };
        bxAABB _aabb;
        Data _data;
        i32 _instancesCount;


        GfxContext* _ctx;
        u32 _internalHandle;

        struct Cmd
        {
            enum
            {
                eOP_ADD,
                eOP_REMOVE,
                eOP_REFRESH,
            };
            u32 handle;
            u32 op;
        };
        array_t< Cmd > _cmd;
        bxRecursiveBenaphore _lockCmd;

        GfxSortListColor* _sListColor;
        GfxSortListDepth* _sListDepth;

        GfxScene();    

        virtual GfxScene* isScene() { return this; }
    };
    void gfxSceneDataAllocate( GfxScene::Data* data, int newsize, bxAllocator* alloc );
    void gfxSceneDataFree( GfxScene::Data* data );
    int  gfxSceneDataAdd( GfxScene::Data* data, GfxMeshInstance* meshInstance );
    void gfxSceneDataRemove( GfxScene::Data* data, int index );
    int  gfxSceneDataRefresh( GfxScene::Data* data, GfxContext* ctx, int index );
    void gfxSceneDataRefresh( GfxScene* scene, u32 meshHandle );
    int  gfxSceneDataFind( const GfxScene::Data& data, u32 meshHandle );
    
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
        float _localAABB[6];

        GfxMeshInstance();

        virtual GfxMeshInstance* isMeshInstance() { return this; }
    };

    //////////////////////////////////////////////////////////////////////////
    ///
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
        bxGdiBuffer _instanceOffsetBuffer;

        array_t<u32> _instanceOffsetArray;
        i32 _maxInstances;

        GfxView();
    };
    void gfxViewCreate( GfxView* view, bxGdiDeviceBackend* dev, int maxInstances );
    void gfxViewDestroy( GfxView* view, bxGdiDeviceBackend* dev );
    void gfxViewCameraSet( bxGdiContext* gdi, const GfxView* view, const GfxCamera* camera, int rtw, int rth );
    void gfxViewEnable( bxGdiContext* gdi, const GfxView* view );

    //////////////////////////////////////////////////////////////////////////
    ///
    struct GfxLights;
    struct GfxLighningData
    {
        u32 _numTilesXY[2];
        u32 _numTiles;
        u32 _tileSize;
        u32 _maxLights;
        f32 _tileSizeRcp;

        f32 _sunAngularRadius;
        f32 _sunIlluminanceInLux;
        f32 _skyIlluminanceInLux;

        float3_t _sunDirection;
        //float3 _sunColor;
        //float3 _skyColor;
    };
    void gfxLightningDataFill( GfxLighningData* ldata, const GfxLights* lights, const GfxSunLight* sunLight );

    struct GfxLights
    {
        bxGdiBuffer _lightnigParamsBuffer;
        //bxGdiBuffer _lightsDataBuffer;
        //bxGdiBuffer _lightsIndicesBuffer;

        GfxLights();
    };
    void gfxLightsCreate( GfxLights* lights, bxGdiDeviceBackend* dev );
    void gfxLightsDestroy( GfxLights* lights, bxGdiDeviceBackend* dev );
    void gfxLightsUploadData( bxGdiContext* ctx, const GfxLights* lights, const GfxSunLight* sunLight );
    void gfxLightsEnable( bxGdiContext* ctx, const GfxLights* lights );
    void gfxSunLightCreate( GfxSunLight** sunLight, GfxContext* ctx );
    void gfxSunLightDestroy( GfxSunLight** sunLight );
    void gfxSunLightDirectionSet( GfxSunLight* sunLight, const Vector3& direction );
    Vector3 gfxSunLightDirectionGet( GfxSunLight* sunLight );
    //////////////////////////////////////////////////////////////////////////
    ///
    struct GfxContext;
    struct GfxCommandQueue
    {
        GfxView _view;
        u32 _acquireCounter;
        GfxContext* _ctx;
        bxGdiContext* _gdiContext;

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

    //////////////////////////////////////////////////////////////////////////
    ///
    struct GfxMaterialManager
    {
        typedef float3_t float3;
        #include <shaders/hlsl/sys/material.hlsl>

        typedef hashmap_t MaterialMap;
        MaterialMap _map;

        bxGdiShaderFx* _nativeFx;
    };
    void gfxMaterialManagerStartup( GfxMaterialManager** materialManager, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* nativeShaderName = "native1" );
    void gfxMaterialManagerShutdown( GfxMaterialManager** materialManager, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    bxGdiShaderFx_Instance* gfxMaterialManagerCreateMaterial( GfxMaterialManager* materialManager, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* name, const GfxMaterialManager::Material& params );
    inline u64 gfxMaterialManagerCreateNameHash( const char* name )
    {
        const u32 hashedName0 = simple_hash( name );
        const u32 hashedName1 = murmur3_hash32( name, (u32)strlen( name ), hashedName0 );
        const u64 key = u64( hashedName1 ) << 32 | u64( hashedName0 );
        return key;
    }

    //////////////////////////////////////////////////////////////////////////
    ///
    struct GfxShadow
    {
        Matrix4 _lightWorld;
        Matrix4 _lightView;
        Matrix4 _lightProj;
        
        bxGdiTexture _texDepth;
        GfxSortListShadow* _sortList;

        GfxShadow();
    };
    void gfxShadowCreate( GfxShadow* shd, bxGdiDeviceBackend* dev, int shadowMapSize );
    void gfxShadowDestroy( GfxShadow* shd, bxGdiDeviceBackend* dev );
    void gfxShadowComputeMatrices( GfxShadow* shd, const Vector3 wsCorners[8], const Vector3& lightDirection );
    void gfxShadowSortListBuild( GfxShadow* shd, bxChunk* chunk, const GfxScene* scene );
    void gfxShadowSortListSubmit( bxGdiContext* gdi, const GfxView& view, const GfxScene::Data& data, const GfxSortListShadow* sList, int begin, int end );
    void gfxShadowDraw( GfxCommandQueue* cmdq, GfxShadow* shd, const GfxScene* scene, const GfxCamera* mainCamera, const Vector3& lightDirection );
    void gfxShadowResolve( GfxCommandQueue* cmdq, bxGdiTexture output, const bxGdiTexture sceneHwDepth, const GfxShadow* shd, const GfxCamera* mainCamera );


    //////////////////////////////////////////////////////////////////////////
    ///
    struct GfxContext
    {
        GfxCommandQueue _cmdQueue;
        
        GfxLights _lights;
        GfxSunLight* _sunLight;

        GfxShadow _shadow;


        bxGdiTexture _framebuffer[eFB_COUNT];
        bxGdiShaderFx_Instance* _fxISky;
        bxGdiShaderFx_Instance* _fxISao;
        bxGdiShaderFx_Instance* _fxIShadow;

        bxAllocator* _allocMesh;
        bxAllocator* _allocCamera;
        bxAllocator* _allocScene;
        bxAllocator* _allocIDataMulti;
        bxAllocator* _allocIDataSingle;

        ActorHandleManager _handles;
        bxRecursiveBenaphore _lockHandles;

        bxBenaphore _lockActorsToRelease;
        array_t< GfxActor* > _actorsToRelease;

        static GfxGlobalResources* _globalResources;
        static GfxMaterialManager* _materialManager;

        GfxContext();
    };
    ////
    //
    inline u32 gfxContextHandleAdd( GfxContext* ctx, GfxActor* actor )
    {
        bxScopeRecursiveBenaphore lock( ctx->_lockHandles );
        return ctx->_handles.add( actor ).asU32();
    }
    inline void gfxContextHandleRemove( GfxContext* ctx, u32 handle )
    {
        bxScopeRecursiveBenaphore lock( ctx->_lockHandles );
        ctx->_handles.remove( ActorHandleManager::Handle( handle ) );
    }
    inline int gfxContextHandleValid( GfxContext* ctx, u32 handle )
    {
        bxScopeRecursiveBenaphore lock( ctx->_lockHandles );
        return ctx->_handles.isValid( ActorHandleManager::Handle( handle ) );
    }
    inline int gfxContextHandleActorGet_lock( GfxActor** actor, GfxContext* ctx, u32 handle )
    {
        bxScopeRecursiveBenaphore lock( ctx->_lockHandles );
        return ctx->_handles.get( ActorHandleManager::Handle( handle ), actor );
    }
    inline int gfxContextHandleActorGet_noLock( GfxActor** actor, GfxContext* ctx, u32 handle )
    {
        return ctx->_handles.get( ActorHandleManager::Handle( handle ), actor );
    }
    inline void gfxContextActorRelease( GfxContext* ctx, GfxActor* actor )
    {
        bxScopeBenaphore lock( ctx->_lockActorsToRelease );
        array::push_back( ctx->_actorsToRelease, actor );
    }
    void gfxGlobalResourcesStartup ( GfxGlobalResources** globalResources, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void gfxGlobalResourcesShutdown( GfxGlobalResources** globalResources, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
}///


namespace bx
{
    template< class Tlist >
    void gfxViewUploadInstanceData( bxGdiContext* gdi, GfxView* view, const GfxScene::Data& data, const Tlist* sList, int begin, int end )
    {
        float4_t* dataWorld = (float4_t*)bxGdi::buffer_map( gdi->backend(), view->_instanceWorldBuffer, 0, view->_maxInstances );
        float3_t* dataWorldIT = (float3_t*)bxGdi::buffer_map( gdi->backend(), view->_instanceWorldITBuffer, 0, view->_maxInstances );

        array::clear( view->_instanceOffsetArray );
        u32 currentOffset = 0;
        u32 instanceCounter = 0;
        for ( int i = begin; i < end; ++i )
        {
            const Tlist::ItemType& item = sList->items[i];
            const GfxInstanceData idata = data.idata[item.index];

            for ( int imatrix = 0; imatrix < idata.count; ++imatrix, ++instanceCounter )
            {
                SYS_ASSERT( instanceCounter < (u32)view->_maxInstances );

                const u32 dataOffset = (currentOffset + imatrix) * 3;
                const Matrix4 worldRows = transpose( idata.pose[imatrix] );
                const Matrix3 worldITRows = inverse( idata.pose[imatrix].getUpper3x3() );

                const float4_t* worldRowsPtr = (float4_t*)&worldRows;
                memcpy( dataWorld + dataOffset, worldRowsPtr, sizeof( float4_t ) * 3 );

                const float4_t* worldITRowsPtr = (float4_t*)&worldITRows;
                memcpy( dataWorldIT + dataOffset, worldITRowsPtr, sizeof( float3_t ) );
                memcpy( dataWorldIT + dataOffset + 1, worldITRowsPtr + 1, sizeof( float3_t ) );
                memcpy( dataWorldIT + dataOffset + 2, worldITRowsPtr + 2, sizeof( float3_t ) );
            }

            array::push_back( view->_instanceOffsetArray, currentOffset );
            currentOffset += idata.count;
        }
        array::push_back( view->_instanceOffsetArray, currentOffset );

        gdi->backend()->unmap( view->_instanceWorldITBuffer.rs );
        gdi->backend()->unmap( view->_instanceWorldBuffer.rs );

        SYS_ASSERT( array::size( view->_instanceOffsetArray ) == end + 1 );

    }
}///