#pragma once

#include "gdi_backend.h"

struct bxGdiRenderSurface
{
    u32 topology;
    u32 begin;
    u32 count;

    bxGdiRenderSurface()
        : topology(0)
        , begin(0), count(0)
    {}

    bxGdiRenderSurface( int topo, int b, int cnt )
        : topology(topo), begin(b), count(cnt)
    {}
};

struct bxGdiRenderSource
{
    bxGdiVertexBuffer* vstreams;
    bxGdiIndexBuffer istream;
    i8 numStreams;

    bxGdiRenderSource()
        : vstreams(0)
        , istream( bxGdi::nullIndexBuffer() )
        , numStreams(0)
    {}
};

struct bxGdiRenderSourceDesc
{
    bxGdiVertexStreamDesc vdescs[ bxGdi::cMAX_VERTEX_BUFFERS ];
    void* vdata[ bxGdi::cMAX_VERTEX_BUFFERS ];

    i32 indicesDataType;
    void* idata;
};