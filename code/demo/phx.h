#pragma once

namespace bx
{
    struct PhxContext;
    struct PhxScene;
    typedef void PhxActor;
    typedef void PhxShape;

    bool phxContextStartup( PhxContext** phx, int maxThreads );
    void phxContextShutdown( PhxContext** phx );

    bool phxSceneCreate( PhxScene** scene, PhxContext* ctx );
    void phxSceneDestroy( PhxScene** scene );

    void phxSceneSimulate( PhxScene* scene, float deltaTime );
    void phxSceneSync( PhxScene* scene );

}////
