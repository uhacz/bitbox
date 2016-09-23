#pragma once


#include <resource_manager/resource_manager.h>
#include <gdi/gdi_context.h>
#include <phx/phx.h>
#include <gfx/gfx.h>

#include <util/ascii_script.h>
#include <util/vectormath/vectormath.h>

#include "profiler.h"
#include "camera_manager.h"
#include "scene_graph.h"


struct bxInput;

namespace bx
{
    struct GraphSceneScriptCallback;
    struct Engine
    {
        bxResourceManager*    resource_manager = nullptr;

        bxGdiDeviceBackend*   gdi_device = nullptr;
        bxGdiContext*         gdi_context = nullptr;

        CameraManager*        camera_manager = nullptr;
        GfxContext*       gfx_context = nullptr;

        PhxContext*       phx_context = nullptr;
        
        //// tool for profiling
        Remotery* _remotery = nullptr;

        ////
        GraphSceneScriptCallback*         _graph_script_callback = nullptr;
        CameraManagerSceneScriptCallback* _camera_script_callback = nullptr;

        
        struct StartupInfo
        {
            const char* cfg_filename = nullptr;
            bool start_gdi = true;
            bool start_gfx = true;
            bool start_physics = true;
            bool start_graph = true;
        };
        static void startup( Engine* e, const StartupInfo& info );
        static void shutdown( Engine* e );
    };
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    struct Scene
    {
        bx::GfxScene* gfx = nullptr;
        bx::PhxScene* phx = nullptr;
        SceneGraph* scene_graph = nullptr;

        static void startup( Scene* scene, Engine* engine );
        static void shutdown( Scene* scene, Engine* engine );
    };
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    struct DevCamera
    {
        GfxCamera* camera = nullptr;
        gfx::CameraInputContext input_ctx;

        void tick( const bxInput* input, float deltaTime );

        static void startup( DevCamera* devCamera, Scene* scene, Engine* engine );
        static void shutdown( DevCamera* devCamera, Engine* engine );
    };
}///

