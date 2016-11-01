passes:
{
    color =
    {
        vertex = "vs_main";
        pixel = "ps_main";
        hwstate =
        {
            depth_test = 1;
            depth_write = 1;
            //fill_mode = "WIREFRAME";
            //color_mask = "";
        };
    };
}; #~header

struct in_VS
{
    uint   instanceID : SV_InstanceID;
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct in_PS
{
    float4 h_pos : SV_Position;
    float4 s_pos : TEXCOORD0;
    float3 w_pos : TEXCOORD1;
    nointerpolation float3 w_normal : TEXCOORD2;
};

struct out_PS
{
    float4 rgba : SV_Target0;
};

#include <sys/vertex_transform.hlsl>
#include <sys/material.hlsl>

cbuffer _FrameData : register( BSLOT( SLOT_FRAME_DATA ) )
{
    matrix _view;
    matrix _viewProj;
};

cbuffer MaterialData : register( BSLOT( SLOT_MATERIAL_DATA ) )
{
    MATERIAL_VARIABLES;
};

in_PS vs_main( in_VS IN )
{
    in_PS OUT = (in_PS)0;

    float4 row0, row1, row2;
    float3 row0IT, row1IT, row2IT;

    fetchWorld( row0, row1, row2, IN.instanceID );
    fetchWorldIT( row0IT, row1IT, row2IT, IN.instanceID );

    float4 localPos = float4( IN.pos, 1.0 );

    OUT.w_pos = transformPosition( row0, row1, row2, localPos );
    OUT.w_normal = transformNormal( row0IT, row1IT, row2IT, IN.normal );
    OUT.h_pos = mul( _viewProj, float4( OUT.w_pos, 1.f ) );
    OUT.s_pos = OUT.h_pos;

    return OUT;
}

out_PS ps_main( in_PS IN )
{
    out_PS OUT;
    OUT.rgba = float4( 1, 0, 0, 1 );
    return OUT;
}