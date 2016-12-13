#ifndef UTIL_HLSL
#define UTIL_HLSL

float cameraDepth( in float3 worldPosition, in float3 cameraEyePos, in float3 cameraDir )
{
    const float3 inCameraSpace = worldPosition - cameraEyePos;
    return dot( cameraDir, inCameraSpace );
}

float resolveLinearDepth( float hwDepth, float2 reprojectDepthInfo )
{
    return rcp( hwDepth * reprojectDepthInfo.x + reprojectDepthInfo.y );
    //return -_camera_zFar * _camera_zNear / (hwDepth * (_camera_zFar - _camera_zNear) - _camera_zFar);
    //float c1 = _camera_zFar / _camera_zNear;
    //float c0 = 1.0 - c1;
    //return 1.0 / (c0 * hwDepth + c1);
}

///

float3 resolvePositionVS( float2 screenPos, float linearDepth, float2 reprojectInfo )
{
    //return float3((screenPos * _reprojectInfo.xy + _reprojectInfo.zw) * -linearDepth, linearDepth );
    return float3( screenPos * reprojectInfo * -linearDepth, linearDepth );
}

#endif