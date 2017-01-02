#ifndef NO_GODS_NO_MASTERS_DATA
#define NO_GODS_NO_MASTERS_DATA

CBUFFER MaterialData BREGISTER( SLOT_MATERIAL_DATA )
{
    float2 resolution;
    float2 resolutionRcp;
    float time;
    uint currentSong;
};

#endif