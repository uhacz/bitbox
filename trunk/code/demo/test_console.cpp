#include "test_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <util/memory.h>
#include <util/array.h>
#include <util/hashmap.h>
#include <util/common.h>

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

#include <util/vectormath/vectormath.h>

namespace brdf
{
    
    float saturate( float x )
    {
        return clamp( x, 0.f, 1.f );
    }

    float lerp( float a, float b, float x )
    {
        return a + x*(b - a);
    }

    float F_Schlick( float f0, float f90, float u )
    {
        return f0 + (f90 - f0) * pow( 1.f - u, 5.f );
    }

    float Diffuse( float NdotV, float NdotL, float LdotH, float linearRough )
    {
        float energyBias = lerp( 0, 0.5, linearRough );
        float energyFactor = lerp( 1.0, 1.f / 1.51f, linearRough );
        float fd90 = energyBias + 2.0 * LdotH * LdotH * linearRough;
        float f0 = 1.0f;
        float lightScatter = F_Schlick( f0, fd90, NdotL );
        float viewScatter = F_Schlick( f0, fd90, NdotV );

        return (lightScatter * viewScatter * energyFactor);
    }

    float V_SmithGGXCorrelated( float NdotL, float NdotV, float alphaG )
    {
        float alphaG2 = alphaG * alphaG;
        float lambda_GGXL = NdotL * sqrt( (-NdotV * alphaG2 + NdotV) * NdotV + alphaG2 );
        float lambda_GGXV = NdotV * sqrt( (-NdotL * alphaG2 + NdotL) * NdotL + alphaG2 );
        return 0.5f / (lambda_GGXL + lambda_GGXV);
    }

    float D_GGX( float NdotH, float m )
    {
        float m2 = m*m;
        float f = (NdotH * m2 - NdotH) * NdotH + 1;
        return m2 / (f*f);
    }

    float BRDF( const Vector3 L, const Vector3 V, const Vector3 N, float rough, float reflectance )
    {
        Vector3 H = normalize( L + V );

        float NdotH = saturate( dot( N, H ).getAsFloat() );
        float VdotH = saturate( dot( V, H ).getAsFloat() );
        float NdotL = saturate( dot( N, L ).getAsFloat() );
        float NdotV = abs( dot( N, V ).getAsFloat() ) + 1e-5f;
        float LdotH = saturate( dot( L, H ).getAsFloat() );

        float linearRough = sqrt( rough );

        float F = F_Schlick( reflectance, 1.0f, VdotH );
        float Vis = V_SmithGGXCorrelated( NdotV, NdotL, rough );
        float D = D_GGX( NdotH, rough );
        float Fr = D * F * Vis;

        float Fd = Diffuse( NdotV, NdotL, LdotH, linearRough );

        return ( Fd + Fr ) / PI;
    }

}///
void testBRDF()
{
    const Vector3 L = normalize( Vector3( 1.f, 1.f, 0.1f ) );
    const Vector3 N =normalize( Vector3( 1.f, 1.f, 0.1f ) );
    const Vector3 V = Vector3::zAxis();
    
    for( float rough = 0.f; rough <= 1.05f; rough += 0.1f )
    {
        printf( "rough: %f =>", rough );
        for ( float reflectance = 0.f; reflectance <= 1.05f; reflectance += 0.1f )
        {
            const float v = brdf::BRDF( L, V, N, rough, reflectance );
            printf( "%f; ", v );
        }
        printf( "\n" );
    }

}