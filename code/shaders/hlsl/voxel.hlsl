passes:
{
    voxel1024 =
    {
        vertex = "vs_main";
        pixel = "ps_main";
    };
}; #~header


/*
przesylamy tutaj index w gridzie i kolor index: 10:10:10, color: rgba
mamy tutaj tez zbindowanego mesha (box) 

na podstawie index'u obliczamy pozycje worldSpace, transformujemy wierzcholek box'a i przepisujemy kolor :)
*/

shared cbuffer MaterialData: register(b3)
{
    
};

struct in_VS
{
    uint   instanceID : SV_InstanceID;
    float4 pos	  	  : POSITION;
    float3 normal 	  : NORMAL;
};

struct in_PS
{
    float4 h_pos	: SV_Position;
    float4 s_pos    : TEXCOORD0;
    float3 w_pos	: TEXCOORD1;
    float3 w_normal :TEXCOORD2;
};

struct out_PS
{  
    float4 rgba : SV_Target0;
};

in_PS vs_main( in_VS input )
{
    in_PS OUT = (in_PS)0;
    return OUT;
}

out_PS ps_main( in_PS input )
{
    out_PS OUT;
    OUT.rgba = float4(0.0, 0.0, 0.0, 1.0);
    return OUT;
}
