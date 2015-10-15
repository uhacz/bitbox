#ifndef TYPES
#define TYPES

#define PI				(3.14159265f)
#define PI_DIV_2		(1.57079632f)
#define TWO_PI			(6.28318530f)
#define PI_DIV_180		(0.01745329f)
#define PI_DIV_360		(0.00872664f)
#define _180_DIV_PI		(57.2957795130f)
#define ONE_OVER_TWO_PI (0.159154943f)
#define PI_RCP (0.31830988618379067154f)
#define _E_ (2.71828182845904523536028747135266249775724709369995957f)

float4 colorU32toFloat4_RGBA( uint rgba )
{
    float r = (float)( (rgba >> 24) & 0xFF );
    float g = (float)( (rgba >> 16) & 0xFF );
    float b = (float)( (rgba >> 8 ) & 0xFF );
    float a = (float)( (rgba >> 0 ) & 0xFF );

    const float scaler = 0.00392156862745098039; // 1/255
    return float4( r, g, b, a ) * scaler;
}

float4 colorU32toFloat4_ABGR( uint abgr )
{
    float a = (float)((abgr >> 24) & 0xFF);
    float b = (float)((abgr >> 16) & 0xFF);
    float g = (float)((abgr >> 8) & 0xFF);
    float r = (float)((abgr >> 0) & 0xFF);

    const float scaler = 0.00392156862745098039; // 1/255
    return float4(r, g, b, a) * scaler;
}

#endif