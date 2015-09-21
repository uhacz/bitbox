#pragma once

struct bxWindow;
class bxResourceManager;
struct bxGdiDeviceBackend;
struct bxGdiContextBackend;
struct bxGdiContext;

namespace tjdb
{
    void startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

    void tick();
    void draw( bxGdiContext* ctx );

}///