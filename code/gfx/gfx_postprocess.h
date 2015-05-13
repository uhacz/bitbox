#pragma once

#include <gdi/gdi_backend.h>


struct bxGdiContext;
struct bxGfxLight_Sun;
struct bxGdiShaderFx_Instance;
class bxResourceManager;

struct bxGfxPostprocess
{
    void toneMapping( bxGdiContext* ctx, bxGdiTexture outTexture, bxGdiTexture inTexture, float deltaTime );

    void sky( bxGdiContext* ctx, bxGdiTexture outTexture, const bxGfxLight_Sun& sunLight );
    void fog( bxGdiContext* ctx, bxGdiTexture outTexture, bxGdiTexture inTexture, bxGdiTexture depthTexture, bxGdiTexture shadowTexture, const bxGfxLight_Sun& sunLight );
    void ssao( bxGdiContext* ctx, bxGdiTexture nrmVSTexture, bxGdiTexture depthTexture );
    void fxaa( bxGdiContext* ctx, bxGdiTexture outputTexture, bxGdiTexture inputTexture );


    bxGdiTexture ssaoOutput() const { return _ssao.outputTexture; }

    bxGfxPostprocess();
    void _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, int fbWidth, int fbHeight );
    void _Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

private:
    void _ShowGUI();

private:

    struct ToneMapping
    {
        u32 currentLuminanceTexture;
        f32 tau;
        f32 autoExposureKeyValue;
        f32 camera_aperture;
        f32 camera_shutterSpeed;
        f32 camera_iso;
        i32 useAutoExposure;

        bxGdiTexture adaptedLuminance[2];
        bxGdiTexture initialLuminance;

        ToneMapping()
            : currentLuminanceTexture( 0 ), tau( 5.f ), autoExposureKeyValue( 0.18f )
            , camera_aperture( 16.f ), camera_shutterSpeed( 1.f / 100.f ), camera_iso( 100.f )
            , useAutoExposure( 1 )
        {}
    } _toneMapping;

    struct Fog
    {
        f32 fallOff;
        f32 fallOffPower;

        Fog()
            : fallOff( 0.0005f )
        {}
    } _fog;

    struct SSAO
    {
        f32 _radius;
        f32 _bias;
        f32 _intensity;
        f32 _projScale;
        u32 _frameCounter;
        bxGdiTexture outputTexture;
        bxGdiTexture swapTexture;

        SSAO()
            : _radius( 1.2f )
            , _bias( 0.025f )
            , _intensity( 0.5f )
            , _projScale( 700.f )
            , _frameCounter( 0 )

        {}
    } _ssao;

    bxGdiShaderFx_Instance* _fxI_toneMapping;
    bxGdiShaderFx_Instance* _fxI_fog;
    bxGdiShaderFx_Instance* _fxI_ssao;
    bxGdiShaderFx_Instance* _fxI_fxaa;
};

