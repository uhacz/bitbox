#include "ship_terrain.h"
#include <resource_manager/resource_manager.h>
#include <math.h>

namespace bx{ namespace ship{



    void Terrain::CreateFromFile( const char* filename )
    {
        ResourceManager* resource_manager = GResourceManager();
        _file = resource_manager->readFileSync( filename );
        if( _file.ok() )
        {
            return;
        }

        _samples = (f32*)_file.bin;
        _num_samples_x = (u32)( ::sqrt( (float)_file.size ) ) / sizeof( *_samples );
        _num_samples_z = _num_samples_x;



    }

    void Terrain::Destroy()
    {
        _file.release();
    }

}
}//