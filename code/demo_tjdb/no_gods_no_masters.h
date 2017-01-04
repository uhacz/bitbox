#pragma once
#include <system\application.h>

#include <resource_manager/resource_manager.h>
#include <rdi/rdi.h>
#include <bass/bass.h>
#include <shaders/shaders/sys/binding_map.h>

using namespace bx;

namespace tjdb
{
    class PostProcessPass
    {
    public:
        static void _StartUp( PostProcessPass* pass );
        static void _ShutDown( PostProcessPass* pass );

        void DoToneMapping( rdi::CommandQueue* cmdq, rdi::TextureRW outTexture, rdi::TextureRW inTexture, float deltaTime );


        struct ToneMapping
        {
        #include <shaders/shaders/tone_mapping_data.h>
            MaterialData data;

            rdi::TextureRW adapted_luminance[2] = {};
            rdi::TextureRW initial_luminance = {};
            rdi::Pipeline pipeline_luminance_map = BX_RDI_NULL_HANDLE;
            rdi::Pipeline pipeline_adapt_luminance = BX_RDI_NULL_HANDLE;
            rdi::Pipeline pipeline_composite = BX_RDI_NULL_HANDLE;
            rdi::ConstantBuffer cbuffer_data = {};
            u32 current_luminance_texture = 0;

            rdi::TextureRW CurrLuminanceTexture() { return adapted_luminance[current_luminance_texture]; }
            rdi::TextureRW PrevLuminanceTexture() { return adapted_luminance[!current_luminance_texture]; }

        }_tm;

    };


class NoGodsNoMasters : public bxApplication
{
public:
    bool startup( int argc, const char** argv ) override;
    void shutdown() override;
    bool update( u64 deltaTimeUS ) override;

    void DrawFullScreenQuad( rdi::CommandQueue* cmdq );

private:
    void _StopCurrentSong();
private:
    rdi::RenderSource _fullscreen_quad = BX_RDI_NULL_HANDLE;
    
    rdi::TextureRO _bg_texture = {};
    rdi::TextureRO _bg_mask = {};
    rdi::TextureRO _logo_mask = {};

    //rdi::TextureRO _logo_texture = {};
    rdi::TextureRO _spis_texture = {};
    rdi::TextureRW _fft_texture = {};
    rdi::TextureRW _color_texture = {};
    rdi::TextureRW _swap_texture = {};

    rdi::ConstantBuffer _cbuffer_mdata = {};

    rdi::Pipeline _pipeline_blit = BX_RDI_NULL_HANDLE;
    rdi::Pipeline _pipeline_main = BX_RDI_NULL_HANDLE;

    struct
    {
        rdi::ResourceDescriptor resource_desc = BX_RDI_NULL_HANDLE;
        rdi::Sampler point = {};
        rdi::Sampler linear = {};
        rdi::Sampler bilinear = {};
        rdi::Sampler trilinear = {};
    } _samplers;
    
    PostProcessPass _post_process = {};

    static const unsigned FFT_BINS = 256;
    static const unsigned NUM_SONGS = 7;
    HSTREAM _song_streams[NUM_SONGS] = {};
    bxFS::File _song_files[NUM_SONGS] = {};

    f32 _fft_data_prev[FFT_BINS] = {};
    f32 _fft_data_curr[FFT_BINS] = {};
    
    u32 _current_song = UINT32_MAX;
    u32 _next_song = UINT32_MAX;
    bool _stop_request = false;

    float _timeS = 0.f;

#include <shaders/shaders/tjdb_no_gods_no_masters_data.h>
};
extern NoGodsNoMasters gTJDB;

}//


