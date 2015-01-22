#ifndef UTIL_HLSL
#define UTIL_HLSL

float resolveLinearDepth( float hwDepth, float zNear, float zFar )
{
    return -zFar * zNear / (hwDepth * (zFar - zNear) - zFar);
    //float c1 = znear_zfar.y / znear_zfar.x;
    //float c0 = 1.0 - c1;
    //return 1.0 / (c0 * hw_depth + c1);
}

///
// @param ab_inv : get from FrameData.proj_params.xy
float3 resolvePositionVS( float2 wpos, float linearDepth, float2 abInv )
{
    float2 xy = wpos * abInv * -linearDepth;
    return float3(xy, linearDepth);
}

#endif