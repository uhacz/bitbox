#include "gfx_postprocess.h"
#include "gfx_lights.h"
#include "gfx.h"

#include <gdi/gdi_shader.h>
#include <gdi/gdi_context.h>

#include <util/random.h>
#include <util/time.h>

#include <resource_manager/resource_manager.h>

bxGfxPostprocess::bxGfxPostprocess()
    : _fxI_toneMapping( 0 )
{}

void bxGfxPostprocess::_Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, int fbWidth, int fbHeight )
{
    _fxI_toneMapping = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "tone_mapping" );
    _fxI_fog = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "fog" );
    _fxI_ssao = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "sao" );
    _fxI_fxaa = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "fxaa" );

    const int lumiTexSize = 1024;
    _toneMapping.adaptedLuminance[0] = dev->createTexture2D( lumiTexSize, lumiTexSize, 11, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
    _toneMapping.adaptedLuminance[1] = dev->createTexture2D( lumiTexSize, lumiTexSize, 11, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
    _toneMapping.initialLuminance = dev->createTexture2D( lumiTexSize, lumiTexSize, 1, bxGdiFormat( bxGdi::eTYPE_FLOAT, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );

    const int ssaoTexWidth = fbWidth / 2;
    const int ssaoTexHeight = fbHeight / 2;
    _ssao.outputTexture = dev->createTexture2D( ssaoTexWidth, ssaoTexHeight, 1, bxGdiFormat( bxGdi::eTYPE_UBYTE, 4, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
    _ssao.swapTexture = dev->createTexture2D( ssaoTexWidth, ssaoTexHeight, 1, bxGdiFormat( bxGdi::eTYPE_UBYTE, 4, 1 ), bxGdi::eBIND_RENDER_TARGET | bxGdi::eBIND_SHADER_RESOURCE, 0, 0 );
}

void bxGfxPostprocess::_Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
{
    dev->releaseTexture( &_ssao.swapTexture );
    dev->releaseTexture( &_ssao.outputTexture );
    dev->releaseTexture( &_toneMapping.initialLuminance );
    dev->releaseTexture( &_toneMapping.adaptedLuminance[1] );
    dev->releaseTexture( &_toneMapping.adaptedLuminance[0] );

    bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &_fxI_fxaa );
    bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &_fxI_ssao );
    bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &_fxI_fog );
    bxGdi::shaderFx_releaseWithInstance( dev, resourceManager, &_fxI_toneMapping );
}

void bxGfxPostprocess::toneMapping( bxGdiContext* ctx, bxGdiTexture outTexture, bxGdiTexture inTexture, float deltaTime )
{
    _fxI_toneMapping->setUniform( "input_size0", float2_t( (f32)inTexture.width, (f32)inTexture.height ) );
    _fxI_toneMapping->setUniform( "delta_time", deltaTime );
    _fxI_toneMapping->setUniform( "lum_tau", _toneMapping.tau );
    _fxI_toneMapping->setUniform( "auto_exposure_key_value", _toneMapping.autoExposureKeyValue );
    _fxI_toneMapping->setUniform( "camera_aperture", _toneMapping.camera_aperture );
    _fxI_toneMapping->setUniform( "camera_shutterSpeed", _toneMapping.camera_shutterSpeed );
    _fxI_toneMapping->setUniform( "camera_iso", _toneMapping.camera_iso );
    _fxI_toneMapping->setUniform( "useAutoExposure", _toneMapping.useAutoExposure );

    ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_NONE, 1 ), 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_LINEAR, bxGdi::eADDRESS_CLAMP, bxGdi::eDEPTH_CMP_NONE, 1 ), 1, bxGdi::eSTAGE_MASK_PIXEL );

    //
    //
    ctx->changeRenderTargets( &_toneMapping.initialLuminance, 1, bxGdiTexture() );
    ctx->setViewport( bxGdiViewport( 0, 0, 1024, 1024 ) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
    ctx->setTexture( inTexture, 0, bxGdi::eSTAGE_MASK_PIXEL );
    bxGfx::submitFullScreenQuad( ctx, _fxI_toneMapping, "luminance_map" );

    //
    //
    ctx->changeRenderTargets( &_toneMapping.adaptedLuminance[_toneMapping.currentLuminanceTexture], 1, bxGdiTexture() );
    ctx->setViewport( bxGdiViewport( 0, 0, 1024, 1024 ) );
    ctx->setTexture( _toneMapping.adaptedLuminance[!_toneMapping.currentLuminanceTexture], 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( _toneMapping.initialLuminance, 1, bxGdi::eSTAGE_MASK_PIXEL );
    bxGfx::submitFullScreenQuad( ctx, _fxI_toneMapping, "adapt_luminance" );
    ctx->backend()->generateMipmaps( _toneMapping.adaptedLuminance[_toneMapping.currentLuminanceTexture] );

    //
    //
    ctx->changeRenderTargets( &outTexture, 1, bxGdiTexture() );
    ctx->setViewport( bxGdiViewport( 0, 0, outTexture.width, outTexture.height ) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );

    ctx->setTexture( inTexture, 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( _toneMapping.adaptedLuminance[_toneMapping.currentLuminanceTexture], 1, bxGdi::eSTAGE_MASK_PIXEL );

    bxGfx::submitFullScreenQuad( ctx, _fxI_toneMapping, "composite" );

    _toneMapping.currentLuminanceTexture = !_toneMapping.currentLuminanceTexture;

    ctx->setTexture( bxGdiTexture(), 0, bxGdi::eSTAGE_PIXEL );
    ctx->setTexture( bxGdiTexture(), 1, bxGdi::eSTAGE_PIXEL );

    _ShowGUI();
}

void bxGfxPostprocess::sky( bxGdiContext* ctx, bxGdiTexture outTexture, const bxGfxLight_Sun& sunLight )
{
    _fxI_fog->setUniform( "_sunDir", sunLight.dir );
    _fxI_fog->setUniform( "_sunColor", sunLight.sunColor );
    _fxI_fog->setUniform( "_skyColor", sunLight.skyColor );
    _fxI_fog->setUniform( "_sunIlluminance", sunLight.sunIlluminance );
    _fxI_fog->setUniform( "_skyIlluminance", sunLight.skyIlluminance );
    _fxI_fog->setUniform( "_fallOff", _fog.fallOff );

    ctx->changeRenderTargets( &outTexture, 1 );
    ctx->setViewport( bxGdiViewport( 0, 0, outTexture.width, outTexture.height ) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
    bxGfx::submitFullScreenQuad( ctx, _fxI_fog, "sky" );
}

void bxGfxPostprocess::fog( bxGdiContext* ctx, bxGdiTexture outTexture, bxGdiTexture inTexture, bxGdiTexture depthTexture, bxGdiTexture shadowTexture, const bxGfxLight_Sun& sunLight )
{
    ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ), 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_BILINEAR ), 1, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( depthTexture, 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( inTexture, 1, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( shadowTexture, 2, bxGdi::eSTAGE_MASK_PIXEL );

    ctx->changeRenderTargets( &outTexture, 1 );
    ctx->setViewport( bxGdiViewport( 0, 0, outTexture.width, outTexture.height ) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
    bxGfx::submitFullScreenQuad( ctx, _fxI_fog, "fog" );

    ctx->setTexture( bxGdiTexture(), 0, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( bxGdiTexture(), 1, bxGdi::eSTAGE_MASK_PIXEL );
    ctx->setTexture( bxGdiTexture(), 2, bxGdi::eSTAGE_MASK_PIXEL );
}

void bxGfxPostprocess::ssao( bxGdiContext* ctx, bxGdiTexture nrmVSTexture, bxGdiTexture depthTexture )
{
    ++_ssao._frameCounter;

    bxRandomGen rnd( (u32)bxTime::us() + _ssao._frameCounter );

    _fxI_ssao->setUniform( "_radius", _ssao._radius );
    _fxI_ssao->setUniform( "_radius2", _ssao._radius*_ssao._radius );
    _fxI_ssao->setUniform( "_bias", _ssao._bias );
    _fxI_ssao->setUniform( "_intensity", _ssao._intensity );
    _fxI_ssao->setUniform( "_projScale", _ssao._projScale );
    _fxI_ssao->setUniform( "_ssaoTexSize", float2_t( _ssao.outputTexture.width, _ssao.outputTexture.height ) );
    _fxI_ssao->setUniform( "_randomRot", rnd.getf( 0.f, 100.f ) );
    _fxI_ssao->setTexture( "tex_normalsVS", nrmVSTexture );
    _fxI_ssao->setTexture( "tex_hwDepth", depthTexture );

    bxGdiTexture outTexture = _ssao.outputTexture;
    bxGdiTexture swapTexture = _ssao.swapTexture;

    ctx->changeRenderTargets( &outTexture, 1 );
    ctx->setViewport( bxGdiViewport( 0, 0, outTexture.width, outTexture.height ) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );

    bxGfx::submitFullScreenQuad( ctx, _fxI_ssao, "ssao" );

    ctx->changeRenderTargets( &swapTexture, 1 );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
    _fxI_ssao->setTexture( "tex_source", outTexture );
    bxGfx::submitFullScreenQuad( ctx, _fxI_ssao, "blurX" );

    ctx->changeRenderTargets( &outTexture, 1 );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
    _fxI_ssao->setTexture( "tex_source", swapTexture );
    bxGfx::submitFullScreenQuad( ctx, _fxI_ssao, "blurY" );
}

void bxGfxPostprocess::fxaa( bxGdiContext* ctx, bxGdiTexture outputTexture, bxGdiTexture inputTexture )
{
    _fxI_fxaa->setUniform( "_rcpFrame", float2_t( 1.f / (float)outputTexture.width, 1.f / (float)outputTexture.height ) );
    _fxI_fxaa->setTexture( "tex_source", inputTexture );
    _fxI_fxaa->setSampler( "sampl_source", bxGdiSamplerDesc( bxGdi::eFILTER_LINEAR ) );

    ctx->changeRenderTargets( &outputTexture, 1 );
    ctx->setViewport( bxGdiViewport( 0, 0, outputTexture.width, outputTexture.height ) );
    ctx->clearBuffers( 0.f, 0.f, 0.f, 0.f, 0.f, 1, 0 );
    bxGfx::submitFullScreenQuad( ctx, _fxI_fxaa, "fxaa" );
}

#include "gfx_gui.h"
void bxGfxPostprocess::_ShowGUI()
{
    ImGui::Begin( "gfx" );
    if ( ImGui::CollapsingHeader( "Postprocess" ) )
    {
        if ( ImGui::TreeNode( "ToneMapping" ) )
        {
            ImGui::InputFloat( "adaptation speed", &_toneMapping.tau );
            ///
            const char* itemName_aperture[] =
            {
                "1.4", "2.0", "2.8", "4.0", "5.6", "8.0", "11.0", "16.0",
            };
            const float itemValue_aperture[] =
            {
                1.4f, 2.0f, 2.8f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f,
            };
            static int currentItem_aperture = 7;
            if ( ImGui::Combo( "aperture", &currentItem_aperture, itemName_aperture, sizeof( itemValue_aperture ) / sizeof( *itemValue_aperture ) ) )
            {
                _toneMapping.camera_aperture = itemValue_aperture[currentItem_aperture];
            }

            ///
            const char* itemName_shutterSpeed[] =
            {
                "1/2000", "1/1000", "1/500", "1/250", "1/125", "1/100", "1/60", "1/30", "1/15", "1/8", "1/4", "1/2", "1/1",
            };
            const float itemValue_shutterSpeed[] =
            {
                1.f / 2000.f, 1.f / 1000.f, 1.f / 500.f, 1.f / 250.f, 1.f / 125.f, 1.f / 100.f, 1.f / 60.f, 1.f / 30.f, 1.f / 15.f, 1.f / 8.f, 1.f / 4.f, 1.f / 2.f, 1.f / 1.f,
            };
            static int currentItem_shutterSpeed = 5;
            if ( ImGui::Combo( "shutter speed", &currentItem_shutterSpeed, itemName_shutterSpeed, sizeof( itemValue_shutterSpeed ) / sizeof( *itemValue_shutterSpeed ) ) )
            {
                _toneMapping.camera_shutterSpeed = itemValue_shutterSpeed[currentItem_shutterSpeed];
            }

            ///
            const char* itemName_iso[] =
            {
                "100", "200", "400", "800", "1600", "3200", "6400",
            };
            const float itemValue_iso[] =
            {
                100.f, 200.f, 400.f, 800.f, 1600.f, 3200.f, 6400.f,
            };
            static int currentItem_iso = 0;
            if ( ImGui::Combo( "ISO", &currentItem_iso, itemName_iso, sizeof( itemValue_iso ) / sizeof( *itemValue_iso ) ) )
            {
                _toneMapping.camera_iso = itemValue_iso[currentItem_iso];
            }

            ImGui::Checkbox( "useAutoExposure", (bool*)&_toneMapping.useAutoExposure );

            ImGui::TreePop();
        }

        if ( ImGui::TreeNode( "Fog" ) )
        {
            ImGui::SliderFloat( "fallOff", &_fog.fallOff, 0.f, 1.f, "%.8", 4.f );
            ImGui::TreePop();
        }

        if ( ImGui::TreeNode( "SSAO" ) )
        {
            ImGui::SliderFloat( "radius", &_ssao._radius, 0.f, 10.f );
            ImGui::SliderFloat( "bias", &_ssao._bias, 0.f, 1.f );
            ImGui::SliderFloat( "intensoty", &_ssao._intensity, 0.f, 1.f );
            ImGui::InputFloat( "projScale", &_ssao._projScale );
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
    ImGui::End();
}

