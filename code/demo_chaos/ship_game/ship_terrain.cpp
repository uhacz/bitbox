#include "ship_terrain.h"
#include <resource_manager/resource_manager.h>
#include <math.h>

namespace bx{ namespace ship{



    void Terrain::CreateFromFile( const char* filename )
    {
        ResourceManager* resource_manager = GResourceManager();
        bxFS::File file = resource_manager->readFileSync( filename );
        if( !file.ok() )
        {
            return;
        }

        _samples = (u8*)file.bin;
        _num_samples_x = (u32)( ::sqrt( (float)file.size ) );
        _num_samples_z = _num_samples_x;

        

        file.release();
    }

    void Terrain::Destroy()
    {

    }

}
}//