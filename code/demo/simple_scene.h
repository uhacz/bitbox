#pragma once

#include <gfx/gfx_camera.h>
#include <gfx/gfx_lights.h>
#include "entity.h"
#include <util/containers.h>

struct bxDemoEngine;
struct bxWindow;

struct bxDemoSimpleScene
{
    bxDemoSimpleScene();
    ~bxDemoSimpleScene();

    bxMeshComponent_Manager componentMesh;

    bxGdiRenderSource* rsource;
    bxGfxRenderList* rList;
    bxGfxCamera camera;
    bxGfxCamera camera1;
    bxGfxCamera_InputContext cameraInputCtx;

    array_t<bxGfxLightManager::PointInstance> pointLights;
    array_t<bxEntity> entities;
    
    bxGfxCamera* currentCamera;
};

extern bxDemoSimpleScene* __simpleScene;

void bxDemoSimpleScene_startUp( bxDemoEngine* engine, bxDemoSimpleScene* scene );
void bxDemoSimpleScene_shutdown( bxDemoEngine* engine, bxDemoSimpleScene* scene );
void bxDemoSimpleScene_frame( bxWindow* win, bxDemoEngine* engine, bxDemoSimpleScene* scene, u64 deltaTimeUS );
