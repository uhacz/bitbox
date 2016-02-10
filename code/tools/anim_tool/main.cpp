#include "anim_tool.h"
#include <iostream>

#define ANIM_TOOL_TEST

int main( int argc, char** argv )
{
    unsigned flags = 0;
#ifdef ANIM_TOOL_TEST
    //const char* input_file = "d:/tmp/bitBox/assets/.src/anim/run.bvh";// argv[1];
    //const char* output_anim = "d:/tmp/bitBox/assets/anim/run.anim"; // ( argc > 2 ) ? argv[2] : 0;
    //const char* output_skel = "d:/tmp/bitBox/assets/anim/human.skel";
    const char* input_file =  "d:/dev/code/bitBox/assets/.src/anim/jump.bvh";// argv[1];
    const char* output_anim = "d:/dev/code/bitBox/assets/anim/jump.anim"; // ( argc > 2 ) ? argv[2] : 0;
    const char* output_skel = "d:/dev/code/bitBox/assets/anim/human.skel";

    //flags |= animTool::eEXPORT_REMOVE_ROOT_TRANSLATION_X;
    //flags |= animTool::eEXPORT_REMOVE_ROOT_TRANSLATION_Z;

    int ires = 0;
    //{
    //    ires = animTool::exportSkeleton( output_skel, input_file );
    //}
    {
        ires = animTool::exportAnimation( output_anim, input_file, flags );
    }
#else    
    if( argc != 4 )
    {
        std::cerr << "invalid arguments!" << std::endl;
        std::cout << "usage: anim_tool.exe [skel|anim] [input_file] [output_file]" << std::endl;
        return -1;
    }

    const char* type = argv[1];
    const char* input_file = argv[2];
    const char* output_file = argv[3];

    int ires = 0;
    if( strcmp( type, "skel" ) == 0 )
    {
        ires = animTool::exportSkeleton( output_file, input_file );
    }
    else if( strcmp( type, "anim" ) == 0 )
    {
        ires = animTool::exportAnimation( output_file, input_file, flags );
    }
    else
    {
        std::cerr << "invalid type" << std::endl;
        ires = -1;
    }

    if( ires < 0 )
    {
        std::cerr << input_file << " compile failed!" << std::endl;
    }

#endif
    //system( "PAUSE" );

    return ires;
}