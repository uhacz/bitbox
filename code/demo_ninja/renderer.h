#pragma once

struct bxWindow;

namespace bx
{
    void rendererStartup();
    void rendererShutdown();

    void sampleStartup( bxWindow* window );
    void sampleShutdown();
}////

