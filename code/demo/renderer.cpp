#include "renderer.h"
#include <util/id_table.h>

namespace bxRenderer
{
    enum
    {
        eMAX_MESHES = 1024,
    };
}///

struct bxRenderer_Mesh
{
    id_table_t<bxRenderer::eMAX_MESHES> idTable;
    bxGdiRenderSource* rsource[bxRenderer::eMAX_MESHES];
    bxGdiShaderFx_Instance* fxI[bxRenderer::eMAX_MESHES];
};

struct bxRenderer_Transform
{

};

struct bxRenderer_Context
{
    
    
};

namespace bxRenderer
{

}///