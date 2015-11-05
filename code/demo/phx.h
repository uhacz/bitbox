#pragma once

namespace bx
{
    struct PhxContext;
    struct PhxScene;
    typedef void PhxActor;
    typedef void PhxShape;

    void phxContextStartup( PhxContext** phx, int maxThreads );
    void phxContextShutdown( PhxContext** phx );

    void phxSceneCreate( PhxScene** scene, PhxContext* ctx );
    void phxSceneDestroy( PhxScene** scene );

    void phxSceneSimulate( PhxScene* scene, float deltaTime );
    void phxSceneFetchResults( PhxScene* scene );

}////
