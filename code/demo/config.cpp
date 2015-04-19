#include "config.h"
#include <util/type.h>
#include <util/debug.h>
#include <util/memory.h>
#include <libconfig/libconfig.h>

struct bxConfig_Global
{
    config_t _cfg;

    bxConfig_Global(){}

    config_t* config() { return &_cfg; }
};

static bxConfig_Global* __cfg = 0;

namespace bxConfig
{
    int global_init()
    {
        SYS_ASSERT( __cfg == 0 );
        __cfg = BX_NEW( bxDefaultAllocator(), bxConfig_Global );

        config_init( __cfg->config() );
        int ierr = config_read_file( __cfg->config(), "global.cfg" );
        if( ierr == CONFIG_FALSE )
        {
            BX_DELETE0( bxDefaultAllocator(), __cfg );
            return -1;
        }
        return 0;
    }
    void global_deinit()
    {
        if ( !__cfg )
            return;

        config_destroy( __cfg->config() );
        BX_DELETE0( bxDefaultAllocator(), __cfg );
    }

    const char* global_string( const char* name )
    {
        const char* result = 0;
        config_lookup_string( __cfg->config(), name, &result );
        return result;
    }
    int global_int( const char* name, int defaultValue )
    {
        int result = defaultValue;
        config_lookup_int( __cfg->config(), name, &result );
        return result;
    }
    float global_float( const char* name, float defaultValue )
    {
        double result = defaultValue;
        config_lookup_float( __cfg->config(), name, &result );
        return (float)result;        
    }
}