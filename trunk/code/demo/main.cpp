#include <stdio.h>
#include <stdlib.h>

#include <util\memory.h>

struct Test
{
    int a;
    float b;
    double c;
};

int main( int argc, char* argv[] )
{
    printf( "H\n" );

    bxMemoryStartUp();

    Test* t = BX_ALLOCATE( bxDefaultAllocator(), Test );

    BX_FREE0( bxDefaultAllocator(), t );

    bxMemoryShutDown();

    system( "PAUSE" );
    
    return 0;
}