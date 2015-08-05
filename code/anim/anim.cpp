#include "anim.h"
#include <util/memory.h>

namespace bxAnim
{
bxAnim_Context* context_init( const bxAnim_Skel& skel )
{
	const u32 poseMemorySize = skel.numJoints * sizeof( bxAnim_Joint );
	
	u32 memSize = 0;
	memSize += sizeof( bxAnim_Context );
	memSize += poseMemorySize * bxAnim_Context::ePOSE_CACHE_SIZE;
	memSize += poseMemorySize * bxAnim_Context::ePOSE_STACK_SIZE;
	memSize += sizeof( bxAnim_Cmd ) * bxAnim_Context::eCMD_ARRAY_SIZE;

	u8* memory = (u8*)BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
	memset( memory, 0, memSize );

	bxAnim_Context* ctx = (bxAnim_Context*)memory;

	u8* current_pointer = memory + sizeof(bxAnim_Context);
	for( u32 i = 0; i < bxAnim_Context::ePOSE_CACHE_SIZE; ++i )
	{
		ctx->poseCache[i] = (bxAnim_Joint*)current_pointer;
		current_pointer += poseMemorySize;
	}

	for( u32 i = 0; i < bxAnim_Context::ePOSE_STACK_SIZE; ++i )
	{
		ctx->poseStack[i] = (bxAnim_Joint*)current_pointer;
		current_pointer += poseMemorySize;
	}

	ctx->cmdArray = (bxAnim_Cmd*)current_pointer;
	current_pointer += sizeof(bxAnim_Cmd) * bxAnim_Context::eCMD_ARRAY_SIZE;
	SYS_ASSERT( (iptr)current_pointer == (iptr)( memory + memSize ) );

	return ctx;
}

void context_deinit( bxAnim_Context** ctx )
{
	BX_FREE0( bxDefaultAllocator(), ctx[0] );
}

}///


namespace bxAnimExt
{
    

    bxAnim_Skel* loadSkelFromFile( bxResourceManager* resourceManager, const char* relativePath )
    {

    }

    bxAnim_Clip* loadAnimFromFile( bxResourceManager* resourceManager, const char* relativePath )
    {

    }

}///