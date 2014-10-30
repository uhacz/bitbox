#include <stdio.h>
#include <stdlib.h>

#include <util/memory.h>
#include <util/buffer.h>

struct Test
{
    int a;
    float b;
    double c;
};

int main( int argc, char* argv[] )
{
    buffer_t* buff = 0;

    bxMemoryStartUp();

    for( int i = 0; i < 10; ++i )
    {
        buffer_pushBack( int, buff, i, bxDefaultAllocator() );
    }

    for( int i = 0; i < 10; ++i )
    {
        int val = ( ( i + 1 ) * 10 );
        buffer_pushBack( int, buff, val, bxDefaultAllocator() );
    }

    buffer_eraseFast( int, buff, 5 );
    buffer_erase( int, buff, 2 );

    int a = buffer_vget( int, buff, 3 );
    a = buffer_vget( int, buff, 4 );
    const int n = buffer_count( int, buff );
    buffer_delete( &buff, bxDefaultAllocator() );


    bxMemoryShutDown();

    system( "PAUSE" );
    
    return 0;
}