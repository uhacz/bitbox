#include <stdio.h>
#include <stdlib.h>

#include <util/memory.h>
#include <util/array.h>

struct Test
{
    Test( int i = 0 )
        : a(i)
        , b(i*10.f)
        , c(i*100.0)
    {}
    int a;
    float b;
    double c;
};

int main( int argc, char* argv[] )
{
    //buffer_t* buff = 0;

    
    array_t<Test> test;

    for( int i = 0; i < 10; ++i )
    {
    //    buffer_pushBack( int, buff, i, bxDefaultAllocator() );
        array::push_back( test, Test(i) );
    }

    for( int i = 10; i < 20; ++i )
    {
        //int val = ( ( i + 1 ) * 10 );
        //buffer_pushBack( int, buff, val, bxDefaultAllocator() );

        array::push_back( test, Test(i) );
    }

    array::erase_swap( test, 5 );
    array::erase( test, 2 );

    Test a = test[3];
    a = test[4];

    array::pop_back( test );

    Test* b = array::begin( test );
    const Test* e = array::end( test );
    while( b != e )
    {
        printf( "%f, ", b->b );
        ++b;
    }
    printf( "\n" );

    //buffer_eraseFast( int, buff, 5 );
    //buffer_erase( int, buff, 2 );

    //int a = buffer_vget( int, buff, 3 );
    //a = buffer_vget( int, buff, 4 );
    //const int n = buffer_count( int, buff );
    //buffer_delete( &buff, bxDefaultAllocator() );

    system( "PAUSE" );
    
    return 0;
}