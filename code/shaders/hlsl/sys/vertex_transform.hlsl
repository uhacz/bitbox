#ifndef VERTEX_TRANSFORM
#define VERTEX_TRANSFORM

shared cbuffer InstanceOffset : register(b1)
{
    uint _begin;
};

Buffer<float4> _instance_world : register(t0);
Buffer<float3> _instance_worldIT : register(t1);

void fetchWorld( out float4 row0, out float4 row1, out float4 row2, uint instanceID )
{
    uint row0Index = (_begin + instanceID) * 3;
    row0 = _instance_world[row0Index];
    row1 = _instance_world[row0Index + 1];
    row2 = _instance_world[row0Index + 2];
}

void fetchWorldIT( out float3 row0IT, out float3 row1IT, out float3 row2IT, uint instanceID )
{
    uint row0Index = (_begin + instanceID) * 3;
    row0IT = _instance_worldIT[row0Index];
    row1IT = _instance_worldIT[row0Index + 1];
    row2IT = _instance_worldIT[row0Index + 2];
}

float3 transformPosition( in float4 row0, in float4 row1, in float4 row2, in float4 localPos )
{
    return float3(dot( row0, localPos ), dot( row1, localPos ), dot( row2, localPos ));
}
float3 transformNormal( in float3 row0IT, in float3 row1IT, in float3 row2IT, in float3 normal )
{
    return float3(dot( row0IT, normal ), dot( row1IT, normal ), dot( row2IT, normal ));
}

#endif