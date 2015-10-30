passes:
{
    zPrepassDepthOnly =
    {
        vertex = "vs_main";
        pixel = "ps_main";
        hwstate =
        {
            depth_test = 1;
            depth_write = 1;
            //color_mask = "";
        };
    };

}; #~header

#include <sys/frame_data.hlsl>

shared cbuffer MaterialData : register(b3)
{
};


#ifdef zPrepassDepthOnly

#include <sys/vertex_transform.hlsl>

struct in_VS
{
    uint   instanceID : SV_InstanceID;
    float4 pos	  	  : POSITION;
};

struct in_PS
{
    float4 hpos	: SV_Position;
};

struct out_PS
{};
in_PS vs_main( in_VS IN )
{
    in_PS OUT = (in_PS)0;

    float4 row0, row1, row2;
    float3 row0IT, row1IT, row2IT;

    fetchWorld( row0, row1, row2, IN.instanceID );
    fetchWorldIT( row0IT, row1IT, row2IT, IN.instanceID );

    float3 wpos = transformPosition( row0, row1, row2, IN.pos );
    OUT.hpos = mul( _camera_viewProj, float4(wpos, 1.f) );
    return OUT;
}

[earlydepthstencil]
out_PS ps_main( in_PS input )
{
    out_PS OUT;
    return OUT;
}
#endif
