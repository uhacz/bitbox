#include "simple_scene.h"
#include <gfx/gfx_render_list.h>
#include <util/random.h>
#include <util/color.h>
#include <util/perlin_noise.h>

#include "demo_engine.h"
#include <util/time.h>

bxDemoSimpleScene* __simpleScene = 0;

bxDemoSimpleScene::bxDemoSimpleScene()
    : rsource(0)
    , rList(0)
    , currentCamera(0)
{}

bxDemoSimpleScene::~bxDemoSimpleScene()
{
}
void bxDemoSimpleScene_startUp(bxDemoEngine* engine, bxDemoSimpleScene* scene)
{
    scene->componentMesh._Startup();
    engine->entityManager.register_releaseCallback( bxMeshComponent_Manager::_Callback_releaseEntity, &scene->componentMesh );

    {
        const u32 colors[] =
        {
            0xFF0000FF, 0x00FF00FF, 0x0000FFFF,
            0xFFFF00FF, 0x00FFFFFF, 0xFFFFFFFF,
        };
        const int nColors = sizeof( colors ) / sizeof( *colors );

        const int gridX = 5;
        const int gridZ = 5;
        const float cellSize = 5.f;
        const float y = 1.5f;
        int counter = 0;
        for ( int iz = 0; iz < gridZ; ++iz )
        {
            const float z = -gridZ * cellSize * 0.5f + iz*cellSize;

            for ( int ix = 0; ix < gridX; ++ix )
            {
                const float x = -gridX * cellSize * 0.5f + ix*cellSize;
                const Vector3 pos = Vector3( x, y, z );
                bxGfxLight_Point l;
                m128_to_xyz( l.position.xyz, pos.get128() );
                l.radius = 50.f;
                bxColor::u32ToFloat3( colors[counter%nColors], l.color.xyz );
                l.intensity = 100000.f;

                //pointLights[counter] = _gfxLights->lightManager.createPointLight( l );
                //++nPointLights;
                ++counter;
            }
        }
    }

    scene->rsource = bxGfxContext::shared()->rsource.sphere;

    scene->rList = bxGfx::renderList_new( 1024 * 6, 1024 * 8, bxDefaultAllocator() );

    bxGfxCamera& camera = scene->camera;
    bxGfxCamera& camera1 = scene->camera1;

    camera.matrix.world = Matrix4::translation( Vector3( 0.f, 2.f, 10.f ) );
    camera1.matrix.world = Matrix4( Matrix3::rotationZYX( Vector3( 0.f, 0.f, 0.0f ) ), Vector3( 0.f, 0.f, 35.f ) ); //Matrix4::translation( Vector3( 0.f ) );
    camera1.params = camera.params;

    bxGfx::cameraMatrix_compute( &camera1.matrix, camera1.params, camera1.matrix.world, engine->gfxContext->framebufferWidth(), engine->gfxContext->framebufferHeight() );

    scene->currentCamera = &camera;


    bxGdiRenderSource* rsources[] =
    {
        engine->gfxContext->shared()->rsource.box,
        engine->gfxContext->shared()->rsource.sphere,
    };
    bxGdiShaderFx_Instance* materials[] =
    {
        engine->gfxMaterials->findMaterial( "red" ),
        engine->gfxMaterials->findMaterial( "green" ),
        engine->gfxMaterials->findMaterial( "blue" ),
        engine->gfxMaterials->findMaterial( "white" ),
    };

    const int gridX = 16;
    const int gridY = 16;
    const int gridZ = 16;

    const float cellSize = 1.5f;
    const float yOffset = 4.f;
    const float zOffset = -0.f;

    int counter = 0;
    for ( int iz = 0; iz < gridZ; ++iz )
    {
        const float z = -gridZ * cellSize * 0.5f + iz*cellSize + zOffset;
        for ( int iy = 0; iy < gridY; ++iy )
        {
            const float y = -gridY * cellSize * 0.5f + iy*cellSize + yOffset;
            for ( int ix = 0; ix < gridX; ++ix )
            {
                const float x = -gridX * cellSize * 0.5f + ix*cellSize;
                const Vector3 rndOffset = Vector3( bxNoise_perlin( x, y, z ) );
                const Vector3 rndScale( 1.f );
                const Matrix4 pose = appendScale( Matrix4( Matrix3::identity(), Vector3( x, y, z ) + rndOffset ), rndScale );


                bxEntity e = engine->entityManager.create();
                bxMeshComponent_Instance cMeshI = scene->componentMesh.create( e, 1 );

                bxMeshComponent_Data cMeshData = scene->componentMesh.mesh( cMeshI );
                cMeshData.rsource = rsources[array::size( scene->entities ) % 2];
                cMeshData.shader = materials[array::size( scene->entities ) % 3];
                cMeshData.surface = bxGdi::renderSource_surface( cMeshData.rsource, bxGdi::eTRIANGLES );
                cMeshData.passIndex = 0;
                scene->componentMesh.setMesh( cMeshI, cMeshData );

                bxComponent_Matrix mx = scene->componentMesh.matrix( cMeshI );
                mx.pose[0] = pose;

                array::push_back( scene->entities, e );
            }
        }
    }

        {
            const Matrix4 world = appendScale( Matrix4( Matrix3::identity(), Vector3( 0.f, -3.f, 0.0f ) ), Vector3( 100.f, 0.1f, 100.f ) );
            bxGdiRenderSource* box = engine->gfxContext->shared()->rsource.box;

            bxGdiShaderFx_Instance* fxI = engine->gfxMaterials->findMaterial( "blue" );

            bxEntity e = engine->entityManager.create();
            bxMeshComponent_Instance cMeshI = scene->componentMesh.create( e, 1 );

            bxMeshComponent_Data cMeshData = scene->componentMesh.mesh( cMeshI );
            cMeshData.rsource = box;
            cMeshData.shader = fxI;
            cMeshData.surface = bxGdi::renderSource_surface( cMeshData.rsource, bxGdi::eTRIANGLES );
            cMeshData.passIndex = 0;
            scene->componentMesh.setMesh( cMeshI, cMeshData );

            bxComponent_Matrix mx = scene->componentMesh.matrix( cMeshI );
            mx.pose[0] = world;

            array::push_back( scene->entities, e );
        }

        //{
        //    Matrix4 world = appendScale( Matrix4( Matrix3::identity(), Vector3( -3.f,-1.f, 0.0f ) ), Vector3( 5.f, 5.f, 5.f ) );
        //    bxGdiRenderSource* rsource = _gfxContext->shared()->rsource.sphere;

        //    bxGdiShaderFx_Instance* fxI = _gfxMaterials->findMaterial( "red" );
        //    bxGfxRenderList_ItemDesc itemDesc( rsource, fxI, 0, bxAABB( Vector3(-0.5f), Vector3(0.5f) ) );
        //    bxGfx::renderList_pushBack( rList, &itemDesc, bxGdi::eTRIANGLES, world );

        //    world *= Matrix4::translation( Vector3( 0.f, 0.f, 3.f ) );
        //    itemDesc.setShader( _gfxMaterials->findMaterial( "blue" ), 0 );
        //    itemDesc.setRenderSource( _gfxContext->shared()->rsource.box );
        //    bxGfx::renderList_pushBack( rList, &itemDesc, bxGdi::eTRIANGLES, world );
        //}
}
void bxDemoSimpleScene_shutdown( bxDemoEngine* engine, bxDemoSimpleScene* scene )
{
    for ( int ie = 0; ie < array::size( scene->entities ); ++ie )
    {
        engine->entityManager.release( &scene->entities[ie] );
    }
    array::clear( scene->entities );
    scene->componentMesh._Shutdown();

    bxGfx::renderList_delete( &scene->rList, bxDefaultAllocator() );
    scene->rsource = 0;
    {
        for ( int ilight = 0; ilight < array::size( scene->pointLights ); ++ilight )
        {
            engine->gfxLights->lightManager.releaseLight( scene->pointLights[ilight] );
        }
    }
    array::clear( scene->pointLights );
}
void bxDemoSimpleScene_frame( bxWindow* win, bxDemoEngine* engine, bxDemoSimpleScene* scene, u64 deltaTimeUS)
{
    Matrix4* cameraMatrix = &scene->camera.matrix.world;
    bxGfxCamera& camera = scene->camera;
    bxGfxCamera& camera1 = scene->camera1;
    if ( bxInput_isKeyPressedOnce( &win->input.kbd, bxInput::eKEY_LSHIFT ) )
    {
        scene->currentCamera = (scene->currentCamera == &camera) ? &camera1 : &camera;
    }
    bxGfxCamera* currentCamera = scene->currentCamera;
    //cameraMatrix = ( cameraMatrix == &camera.matrix.world ) ? &camera1.matrix.world : &camera.matrix.world;

    const double deltaTimeS = bxTime::toSeconds( deltaTimeUS );
    const float deltaTime = (float)deltaTimeS;

    bxGfx::cameraUtil_updateInput( &scene->cameraInputCtx, &win->input, 1.f, deltaTime );

    bxGfxCamera_InputContext& cameraInputCtx = scene->cameraInputCtx;
    currentCamera->matrix.world = bxGfx::cameraUtil_movement( currentCamera->matrix.world
                                                               , cameraInputCtx.leftInputX * 0.25f
                                                               , cameraInputCtx.leftInputY * 0.25f
                                                               , cameraInputCtx.rightInputX * deltaTime * 5.f
                                                               , cameraInputCtx.rightInputY * deltaTime * 5.f
                                                               , cameraInputCtx.upDown * 0.25f );

    const int fbWidth = engine->gfxContext->framebufferWidth();
    const int fbHeight = engine->gfxContext->framebufferHeight();
    bxGfx::cameraMatrix_compute( &camera.matrix, camera.params, camera.matrix.world, fbWidth, fbHeight );
    bxGfx::cameraMatrix_compute( &camera1.matrix, camera1.params, camera1.matrix.world, fbWidth, fbHeight );

    scene->rList->clear();
    bxComponent::mesh_createRenderList( scene->rList, scene->componentMesh );
}

