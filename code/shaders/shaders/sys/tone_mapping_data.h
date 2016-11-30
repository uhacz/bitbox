#ifndef TONE_MAPPING_DATA
#define TONE_MAPPING_DATA

CBUFFER MaterialData BREGISTER( SLOT_MATERIAL_DATA )
{
    float2 input_size0;
    float delta_time;

    float bloom_thresh;
    float bloom_blur_sigma;
    float bloom_magnitude;

    float lum_tau;
    float auto_exposure_key_value;
    int use_auto_exposure;

    float camera_aperture;
    float camera_shutterSpeed;
    float camera_iso;
};

#endif