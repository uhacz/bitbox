#include "voxel.h"

#include <gdi/gdi_backend.h>
#include <gdi/gdi_context.h>
#include <gdi/gdi_render_source.h>
#include <gdi/gdi_shader.h>

#include <gfx/gfx_camera.h>
#include "voxel_gfx.h"
#include "voxel_container.h"

namespace bxVoxel
{
    void _Startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
		gfx_startup(dev, resourceManager);
    }

    void _Shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager )
    {
		gfx_shutdown(dev, resourceManager);
    }

  //  void gfx_draw( bxGdiContext* ctx, bxVoxel_Container* cnt, const bxGfxCamera& camera )
  //  {
  //      //bxVoxel_Container* menago = &vctx->menago;
  //      const bxVoxel_Container::Data& data = cnt->_data;
  //      const int numObjects = data.size;

  //      if( !numObjects )
  //          return;

  //      const bxGdiBuffer* vxDataGpu = data.voxelDataGpu;
  //      const bxVoxel_ObjectData* vxDataObj = data.voxelDataObj;
  //      const Matrix4* worldMatrices = data.worldPose;

		//bxVoxel_Gfx* vgfx = bxVoxel_Gfx::instance();

  //      bxGdiShaderFx_Instance* fxI = vgfx->fxI;
  //      bxGdiRenderSource* rsource = vgfx->rsource;

  //      fxI->setUniform( "_viewProj", camera.matrix.viewProj );
  //      
  //      bxGdi::shaderFx_enable( ctx, fxI, 0 );
  //      bxGdi::renderSource_enable( ctx, rsource );
  //      const bxGdiRenderSurface surf = bxGdi::renderSource_surface( rsource, bxGdi::eTRIANGLES );
  //      ctx->setSampler( bxGdiSamplerDesc( bxGdi::eFILTER_NEAREST ), 0, bxGdi::eSTAGE_MASK_PIXEL );

  //      for( int iobj = 0; iobj < numObjects; ++iobj )
  //      {
  //          const bxVoxel_ObjectData& objData = vxDataObj[iobj];
  //          const int nInstancesToDraw = objData.numShellVoxels;
  //          if( nInstancesToDraw <= 0 )
  //              continue;

  //          fxI->setUniform( "_world", worldMatrices[iobj] );
  //          fxI->uploadCBuffers( ctx->backend() );

  //          ctx->setBufferRO( vxDataGpu[iobj], 0, bxGdi::eSTAGE_MASK_VERTEX );
		//	ctx->setTexture( vgfx->colorPalette_texture[objData.colorPalette], 0, bxGdi::eSTAGE_MASK_PIXEL);

  //          bxGdi::renderSurface_drawIndexedInstanced( ctx, surf, nInstancesToDraw );
  //      }
  //  }
}

