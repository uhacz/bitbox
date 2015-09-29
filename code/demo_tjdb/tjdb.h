#pragma once

#include <util/type.h>

struct bxWindow;
struct bxInput;
class bxResourceManager;
struct bxGdiDeviceBackend;
struct bxGdiContextBackend;
struct bxGdiContext;

namespace tjdb
{
    void startup( bxWindow* win, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

    void tick( const bxInput* input, bxGdiContext* ctx, u64 deltaTimeMS );
    void draw( bxGdiContext* ctx );

}///