#pragma once

#include "../vectormath/vectormath.h"
#include "../type.h"
#include "../bbox.h"

struct bxPolyShape
{
    bxPolyShape( int pNElem = 3, int nNElem = 3, int tNElem = 2 )
        : positions(0)
        , normals(0)
        , tangents(0)
        , bitangents(0)
        , texcoords(0)
        , indices(0)
        , num_vertices(0)
        , num_indices(0)
        , n_elem_pos(pNElem)
        , n_elem_nrm(nNElem)
        , n_elem_tex(tNElem)
    {}

    float* positions;
    float* normals;
    float* tangents;
    float* bitangents;
    float* texcoords;
    u32* indices;

    i32 num_vertices;
    i32 num_indices;

    const int n_elem_pos;
    const int n_elem_nrm;
    const int n_elem_tex;

    inline float* position     ( int index ) const { return positions  + index*n_elem_pos; }
    inline float* normal       ( int index ) const { return normals    + index*n_elem_nrm; }
    inline float* tangent      ( int index ) const { return tangents   + index*n_elem_nrm; }
    inline float* bitangent    ( int index ) const { return bitangents + index*n_elem_nrm; }
    inline float* texcoord     ( int index ) const { return texcoords  + index*n_elem_tex; }
    inline u32*   triangle     ( int index ) const { return indices    + index*3; }
    inline int    ntriangles()               const { return num_indices/3; }   
    inline int    nvertices ()               const { return num_vertices; }   
        
private:
    bxPolyShape& operator = ( const bxPolyShape& ) { return *this; }
};

int bxPolyShape_computeNumPoints( const int iterations[6] );
int bxPolyShape_computeNumTriangles( const int iterations[6] );

int bxPolyShape_computeNumPointsPerSide( int iterations );
int bxPolyShape_computeNumTrianglesPerSide( int iterations );

void bxPolyShape_allocateShape( bxPolyShape* shape, int numVertices, int numIndices );
void bxPolyShape_deallocateShape( bxPolyShape* shape );
void bxPolyShape_copy( bxPolyShape* dst, const bxPolyShape& src );

void bxPolyShape_allocateCube( bxPolyShape* shape, const int iterations[6] );
void bxPolyShape_createCube( bxPolyShape* shape, const int iterations[6], float extent = 1.0f );
void bxPolyShape_normalize( bxPolyShape* shape, int begin, int count, float tolerance = FLT_EPSILON );
void bxPolyShape_transform( bxPolyShape* shape, int begin, int count, const Transform3& tr );
void bxPolyShape_generateSphereUV( bxPolyShape* shape, int firstPoint, int count );
void bxPolyShape_generateFlatNormals( bxPolyShape* shape, int firstTriangle, int count );
void bxPolyShape_generateSmoothNormals( bxPolyShape* shape, int firstTriangle, int count );
void bxPolyShape_generateSmoothNormals1( bxPolyShape* shape, int firstPoint, int count );
void bxPolyShape_computeTangentSpace( bxPolyShape* shape );

//////////////////////////////////////////////////////////////////////////
void bxPolyShape_createBox( bxPolyShape* shape, int iterations );
void bxPolyShape_createShpere( bxPolyShape* shape, int iterations );
void bxPolyShape_createCapsule( bxPolyShape* shape, int surfaces[6], int vertexCount[3], int iterations );


#include "par_shapes.h"
void ParShapesMeshMakeSymetric( par_shapes_mesh* mesh );



//struct Grid3D
//{
//    i32 width;
//    i32 height;
//    i32 depth;
//
//    Grid3D()
//        :width(0), height(0), depth(0)
//    {}
//
//    Grid3D( int w, int h, int d )
//        : width(w), height(h), depth(d)
//    {}
//
//    //
//    int num_cells() 
//    { 
//        return width * height * depth; 
//    }
//    int index( int x, int y, int z )
//    {
//        return x + y * width + z*width*height;    
//    }
//    void coords( int xyz[3], int index )
//    {
//        const int wh = width*height;
//        const int index_mod_wh = index % wh;
//        xyz[0] = index_mod_wh % width;
//        xyz[1] = index_mod_wh / width;
//        xyz[2] = index / wh;
//    }
//};
//struct ScalarField
//{
//    f32* samples;
//    int n_samples;
//    Grid3D grid;
//    f32 min_value;
//    f32 max_value;
//    f32 cell_size[3];
//    bxAABB bbox;
//};
//typedef float normalize_func( float a, float b, float x );
//
//void scalar_field_create( ScalarField* sfield, const Vector3* points, int n_points, int n_cells_x, int n_cells_y, int n_cells_z );
//void scalar_field_release( ScalarField* sfield );
//void scalar_field_normalize( ScalarField* sfield, normalize_func* nrm_function, float minimum = 0.f, float maximum = 1.f );
//
//struct IsoSurface
//{
//	f32* points;
//	f32* normals;
//	u32* indices;
//
//	i32 n_points;
//	i32 n_indices;
//};
//void iso_surface_create( IsoSurface* isosurf, const ScalarField& sfield, float iso_level );
//void iso_surface_release( IsoSurface* isosurf );

