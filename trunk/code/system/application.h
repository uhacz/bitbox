#pragma once

struct bxWindow;
class bxApplication
{
public:
    virtual ~bxApplication() {}

    virtual bool startup( int argc, const char** argv ) = 0;
    virtual void shutdown() = 0;
    virtual bool update() = 0;
    
};

bool bxApplication_startup( bxApplication* app, int argc, const char** argv );
void bxApplication_shutdown( bxApplication* app );
int bxApplication_run( bxApplication* app );


