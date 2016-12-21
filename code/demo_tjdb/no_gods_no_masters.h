#pragma once
#include <system\application.h>

#include <resource_manager/resource_manager.h>
#include <rdi/rdi.h>

using namespace bx;

namespace tjdb
{
class NoGodsNoMasters : public bxApplication
{
public:
    bool startup( int argc, const char** argv ) override;
    void shutdown() override;
    bool update( u64 deltaTimeUS ) override;


private:
    rdi::RenderSource _fullscreen_quad = BX_RDI_NULL_HANDLE;
    rdi::TextureRO _kozak_texture = {};

    rdi::Pipeline _pipeline_blit = BX_RDI_NULL_HANDLE;

    struct
    {
        rdi::ResourceDescriptor resource_desc = BX_RDI_NULL_HANDLE;
        rdi::Sampler point = {};
        rdi::Sampler linear = {};
        rdi::Sampler bilinear = {};
        rdi::Sampler trilinear = {};
    } _samplers;
    
};


}//


