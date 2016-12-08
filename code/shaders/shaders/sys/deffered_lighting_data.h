#ifndef DEFFERED_LIGHTING_DATA
#define DEFFERED_LIGHTING_DATA

CBUFFER MaterialData BREGISTER( SLOT_MATERIAL_DATA )
{
    matrix view_proj_inv;
    float2 render_target_size;
    float2 render_target_size_rcp;
    float4 camera_eye;
    float4 camera_dir;
    float4 sun_color;
    float4 sun_L; // L means direction TO light
    float sun_intensity;
    float sky_intensity;
};

#endif
