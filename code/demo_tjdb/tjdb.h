#pragma once

#include <util/type.h>

struct bxWindow;
class bxResourceManager;
struct bxGdiDeviceBackend;
struct bxGdiContextBackend;
struct bxGdiContext;

namespace tjdb
{
    void startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

    void tick( u64 deltaTimeMS );
    void draw( bxGdiContext* ctx );

}///