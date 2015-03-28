#pragma once

struct bxGrid
{
    u32 width;
    u32 height;
    u32 depth;

    //
    bxGrid()
        : width(0), height(0), depth(0)
    {}

    bxGrid( unsigned w, unsigned h, unsigned d )
        : width(w), height(h), depth(d)
    {}

    //
    int numCells() const
    { 
        return width * height * depth; 
    }
    int index( unsigned x, unsigned y, unsigned z ) const
    {
        return x + y * width + z*width*height;    
    }
    void coords( unsigned xyz[3], unsigned idx ) const
    {
        const int wh = width*height;
        const int index_mod_wh = idx % wh;
        xyz[0] = index_mod_wh % width;
        xyz[1] = index_mod_wh / width;
        xyz[2] = idx / wh;
    }
};