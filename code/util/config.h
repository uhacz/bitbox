#pragma once

namespace bxConfig
{
    int global_init( const char* cfgFilename );
    void global_deinit();

    const char* global_string( const char* name );
    int   global_int( const char* name, int defaultValue = -1 );
    float global_float( const char* name, float defaultValue = 0.f );
};
