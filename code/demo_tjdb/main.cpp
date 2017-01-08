#include "no_gods_no_masters.h"
#include "system/window.h"

#define FINAL 1

int main( int argc, const char** argv )
{
#if FINAL == 1
    unsigned w = 1920;
    unsigned h = 1080;
    bool fs = true;
#else
    unsigned w = 960;
    unsigned h = 540;
    bool fs = false;
#endif
    bxWindow* window = bxWindow_create( "tjdb", w, h, fs, 0 );
    if ( window )
    {
        tjdb::NoGodsNoMasters* app = &tjdb::gTJDB;
        if ( bxApplication_startup( app, argc, argv ) )
        {
            bxApplication_run( app );
        }

        bxApplication_shutdown( app );
        bxWindow_release();
    }

    return 0;
}
