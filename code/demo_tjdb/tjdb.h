#pragma once

struct bxWindow;
class bxResourceManager;

namespace tjdb
{
    void startup( bxWindow* win, bxResourceManager* resourceManager );
    void shutdown();


    void draw( bxWindow* win );

}///