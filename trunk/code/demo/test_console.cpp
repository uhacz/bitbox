#include "test_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <util/memory.h>
#include <util/array.h>
#include <util/hashmap.h>

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

int testMain()
{
    array_t<Test> test;
    hashmap_t hmap;

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

    for( int i = 0; i < array::size( test ); ++i )
    {
        hashmap_t::cell_t* cell = hashmap::insert( hmap, (size_t)&test[i] );
        cell->value = test[i].a;
    }

    {
        printf( "\n================================\nhashmap:\n" );

        size_t a = hashmap::lookup( hmap, (size_t)&test[0] )->value;
        size_t b = hashmap::lookup( hmap, (size_t)&test[5] )->value;
        size_t c = hashmap::lookup( hmap, (size_t)&test[12] )->value;
        size_t d = hashmap::lookup( hmap, (size_t)&test[18] )->value;
        size_t e = hashmap::lookup( hmap, (size_t)&test[8] )->value;
        int aa = 0;

        hashmap::erase( hmap, (size_t)&test[13] );
        hashmap::erase( hmap, (size_t)&test[19] );

        hashmap::iterator iter( hmap );
        while( iter.next() )
        {
            int value = (int)iter->value;
            printf( "hmap: %d\n", value );
        }

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

    return 0;
}