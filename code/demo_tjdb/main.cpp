#include "no_gods_no_masters.h"
#include "system/window.h"

#define FINAL 0

int main( int argc, const char** argv )
{
#if FINAL == 1
    unsigned w = 1920;
    unsigned h = 1080;
    bool fs = true;
#else
    unsigned w = 1280;
    unsigned h = 720;
    bool fs = false;
#endif
    bxWindow* window = bxWindow_create( "tjdb", w, h, fs, 0 );
    if ( window )
    {
        tjdb::NoGodsNoMasters app;
        if ( bxApplication_startup( &app, argc, argv ) )
        {
            bxApplication_run( &app );
        }

        bxApplication_shutdown( &app );
        bxWindow_release();
    }

    return 0;
}
