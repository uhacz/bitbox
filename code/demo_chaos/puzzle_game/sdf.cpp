// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2013-2016 NVIDIA Corporation. All rights reserved.

#include "sdf.h"

#include <vector>
#include <float.h>
#include <math.h>

#include <algorithm>
#include <util/common.h>

using namespace std;



namespace
{
	inline float Sqr(float x) { return x*x; }
	inline int Clamp(int x, int lower, int upper) { return min(max(lower, x), upper); }

	uint32_t Sample(const uint32_t* image, uint32_t w, uint32_t h, int x, int y)
	{
		return image[Clamp(y, 0, h-1)*w + Clamp(x, 0, w-1)];
	}

	uint32_t Sample(const uint32_t* image, uint32_t w, uint32_t h, uint32_t d, int x, int y, int z)
	{
		return image[Clamp(z, 0, d-1)*w*h + Clamp(y, 0, h-1)*w + Clamp(x, 0, w-1)];
	}

	// returns true if point is on the surface
	bool EdgeDetect(const uint32_t* img, uint32_t w, uint32_t h, int x, int y)
	{
		bool center = Sample(img, w, h, x, y) != 0;

		for (int j=y-1; j <= y+1; ++j)
		{
			for (int i=x-1; i <= x+1; ++i)
			{
				if ((0 != Sample(img, w, h, i, j)) != center)
				{
					return true;
				}
			}
		}
		
		return false;
	}


	// returns true if point is on the surface
    //static inline bool CoordEdgeDetect( int ix, int iy, int iz, u32 w, u32 d, u32 h )
    //{
    //    bool is_on_edge = false;
    //    is_on_edge |= iz == 0 || iz >= ( d - 1 );
    //    is_on_edge |= iy == 0 || iy >= ( h - 1 );
    //    is_on_edge |= ix == 0 || ix >= ( w - 1 );

    //    return is_on_edge;
    //}

	bool EdgeDetect(const uint32_t* img, uint32_t w, uint32_t h, uint32_t d, int x, int y, int z, float& dist)
	{
		bool center = Sample(img, w, h, d, x, y, z) != 0;

		float minDist = FLT_MAX;

		for (int k=z-1; k <= z+1; ++k)
		{
			for (int j=y-1; j <= y+1; ++j)
			{
				for (int i=x-1; i <= x+1; ++i)
				{
                    const bool data_edge = ( 0 != Sample( img, w, h, d, i, j, k ) ) != center;
                    //const bool coord_edge = CoordEdgeDetect( i, j, k, w, h, d ) && center;

					if ( data_edge /*|| coord_edge*/ )
					{
						int dx = x-i;
						int dy = y-j;
						int dz = z-k;

						minDist = min(sqrtf(float(dx*dx + dy*dy + dz*dz))*0.5f, minDist);
					}
				}
			}
		}

		dist = minDist;

		return minDist != FLT_MAX;
	}
}

// 2D fast marching method (FMM J. Sethian. A fast marching level set method for monotonically advancing fronts. Proc. Natl. Acad. Sci., 93:1591–1595, 1996.)
namespace 
{
	struct Coord2D
	{
		int i, j;
		
		float d;
		int si, sj;

		bool operator < (const Coord2D& c) const { return d > c.d; }
	};
}

void MakeSDF(const uint32_t* img, uint32_t w, uint32_t h, float* output)
{	
	const float scale = 1.0f / max(w, h);

	std::vector<Coord2D> queue;

	// find surface points
	for (uint32_t y=0; y < h; ++y)
	{
		for (uint32_t x=0; x < w; ++x)
		{
			if (EdgeDetect(img, w, h, x, y))
			{
				Coord2D c = {(int)x, (int)y, 0.0f, (int)x, (int)y};
				queue.push_back(c);
			}

			output[y*w + x] = FLT_MAX;
		}
	}

	std::make_heap(queue.begin(), queue.end());

	while (!queue.empty())
	{
		std::pop_heap(queue.begin(), queue.end());

		Coord2D c = queue.back();
		queue.pop_back();

		// freeze coord if not already frozen
		if (output[c.j*w + c.i] == FLT_MAX)
		{
			output[c.j*w + c.i] = c.d;

			// update neighbours
			int xmin = max(c.i-1, 0), xmax = min(c.i+1, int(w-1));
			int ymin = max(c.j-1, 0), ymax = min(c.j+1, int(h-1));

			for (int y=ymin; y <= ymax; ++y)
			{
				for (int x=xmin; x <= xmax; ++x)
				{
					if (c.i != x || c.j != y)
					{
						int dx = x-c.si;
						int dy = y-c.sj;

						// calculate distance to source coord
						float d = sqrtf(float(dx*dx + dy*dy));

						Coord2D newc = {x, y, d, c.si, c.sj};
						queue.push_back(newc);
						std::push_heap(queue.begin(), queue.end());
					}
				}
			}	
		}
	}

	for (uint32_t y=0; y < h; ++y)
	{
		for (uint32_t x=0; x < w; ++x)
		{
			SYS_ASSERT(output[y*w + x] < FLT_MAX);

			// flip sign for interior
			output[y*w + x] *= (img[y*w + x]?-1.0f:1.0f)*scale;
		}
	}
}

// 3D fast marching method (FMM J. Sethian. A fast marching level set method for monotonically advancing fronts. Proc. Natl. Acad. Sci., 93:1591–1595, 1996.)
namespace 
{
	struct Coord3D
	{
		int i, j, k;
		
		float d;
		int si, sj, sk;

		bool operator < (const Coord3D& c) const { return d > c.d; }
	};
}

void MakeSDF(const uint32_t* img, uint32_t w, uint32_t h, uint32_t d, float* output)
{	
	const float scale = 1.0f / max(max(w, h), d);

	std::vector<Coord3D> queue;
    queue.reserve( w*h*d );
	// find surface points
	for (uint32_t z=0; z < d; ++z)
	{
		for (uint32_t y=0; y < h; ++y)
		{
			for (uint32_t x=0; x < w; ++x)
			{
				float dist;
				if (EdgeDetect(img, w, h, d, x, y, z, dist))
				{
					Coord3D c = {(int)x, (int)y, (int)z, dist, (int)x, (int)y, (int)z};
					queue.push_back(c);
				}

				output[z*w*h + y*w + x] = FLT_MAX;
			}
		}
	}

	// no occupied voxels so quit
	if (queue.empty())
		return;

	std::make_heap(queue.begin(), queue.end());

	while (!queue.empty())
	{
		std::pop_heap(queue.begin(), queue.end());

		Coord3D c = queue.back();
		queue.pop_back();

		// freeze coord if not already frozen
		if (output[c.k*w*h + c.j*w + c.i] == FLT_MAX)
		{
			output[c.k*w*h + c.j*w + c.i] = c.d;

			// update neighbours
			int xmin = max(c.i-1, 0), xmax = min(c.i+1, int(w-1));
			int ymin = max(c.j-1, 0), ymax = min(c.j+1, int(h-1));
			int zmin = max(c.k-1, 0), zmax = min(c.k+1, int(d-1));

			for (int z=zmin; z <= zmax; ++z)
			{
				for (int y=ymin; y <= ymax; ++y)
				{
					for (int x=xmin; x <= xmax; ++x)
					{
						if ((c.i != x || c.j != y || c.k != z) && output[z*w*h + y*w + x] == FLT_MAX)
						{
							int dx = x-c.si;
							int dy = y-c.sj;
							int dz = z-c.sk;

							// calculate distance to source coord
							float d = sqrtf(float(dx*dx + dy*dy + dz*dz)) + output[c.sk*w*h + c.sj*w + c.si];

                            SYS_ASSERT(d > 0.0f);

							Coord3D newc = {x, y, z, d, c.si, c.sj, c.sk};

							queue.push_back(newc);
							std::push_heap(queue.begin(), queue.end());
						}
					}
				}	
			}
		}
	}

	for (uint32_t z=0; z < d; ++z)
	{
		for (uint32_t y=0; y < h; ++y)
		{
			for (uint32_t x=0; x < w; ++x)
			{
                SYS_ASSERT(output[z*w*h + y*w + x] < FLT_MAX);

				// flip sign for interior
				output[z*w*h + y*w + x] *= (img[z*w*h + y*w + x]?-1.0f:1.0f)*scale;
			}
		}
	}
}




float SampleSDF( const float* sdf, int dim, int x, int y, int z )
{
    SYS_ASSERT( x < dim && x >= 0 );
    SYS_ASSERT( y < dim && y >= 0 );
    SYS_ASSERT( z < dim && z >= 0 );

    return sdf[z*dim*dim + y*dim + x];
}

//float SampleSDF( const float* sdf, int dimX, int dimY, int dimZ, int x, int y, int z )
//{
//    SYS_ASSERT( x < dimX && x >= 0 );
//    SYS_ASSERT( y < dimY && y >= 0 );
//    SYS_ASSERT( z < dimZ && z >= 0 );
//    
//    return sdf[z*dimX*dimY + y*dimX + x];
//}

void SampleSDFGrad( float grad[3], const float* sdf, int dim, int x, int y, int z )
{
    int x0 = max( x - 1, 0 );
    int x1 = min( x + 1, dim - 1 );

    int y0 = max( y - 1, 0 );
    int y1 = min( y + 1, dim - 1 );

    int z0 = max( z - 1, 0 );
    int z1 = min( z + 1, dim - 1 );

    grad[0] = ( SampleSDF( sdf, dim, x1, y, z ) - SampleSDF( sdf, dim, x0, y, z ) )*( dim*0.5f );
    grad[1] = ( SampleSDF( sdf, dim, x, y1, z ) - SampleSDF( sdf, dim, x, y0, z ) )*( dim*0.5f );
    grad[2] = ( SampleSDF( sdf, dim, x, y, z1 ) - SampleSDF( sdf, dim, x, y, z0 ) )*( dim*0.5f );
}

//void SampleSDFGrad( float grad[3], const float* sdf, int dimX, int dimY, int dimZ, int x, int y, int z )
//{
//    int x0 = max( x - 1, 0 );
//    int x1 = min( x + 1, dimX - 1 );
//
//    int y0 = max( y - 1, 0 );
//    int y1 = min( y + 1, dimY - 1 );
//
//    int z0 = max( z - 1, 0 );
//    int z1 = min( z + 1, dimZ - 1 );
//
//    grad[0] = ( SampleSDF( sdf, dimX, dimY, dimZ, x1, y, z ) - SampleSDF( sdf, dimX, dimY, dimZ, x0, y, z ) )*( dimX*0.5f );
//    grad[1] = ( SampleSDF( sdf, dimX, dimY, dimZ, x, y1, z ) - SampleSDF( sdf, dimX, dimY, dimZ, x, y0, z ) )*( dimY*0.5f );
//    grad[2] = ( SampleSDF( sdf, dimX, dimY, dimZ, x, y, z1 ) - SampleSDF( sdf, dimX, dimY, dimZ, x, y, z0 ) )*( dimZ*0.5f );
//}

/*
// Brute-force 2D SDF generation

void FindNeighbour(const uint32_t* image, uint32_t w, uint32_t h, uint32_t cx, uint32_t cy, uint32_t& i, uint32_t& j, float& d)
	{
		float minDistSq=FLT_MAX;

		float fx = float(cx);
		float fy = float(cy);

		for (uint32_t y=0; y < h; ++y)
		{
			for (uint32_t x=0; x < w; ++x)
			{
				if ((x != cx || y != cy) && image[y*w + x])
				{
					float dSq = Sqr(fx-float(x)) + Sqr(fy-float(y));
					if (dSq < minDistSq)
					{
						minDistSq = dSq;
						i = x;
						j = y;
					}
				}
			}	
		}

		d = sqrtf(minDistSq);
	}	


// brute force
void MakeSDF(const uint32_t* img, uint32_t w, uint32_t h, float* output)
{
	// find surface points
	vector<uint32_t> surface(w*h);

	for (uint32_t y=0; y < h; ++y)
	{
		for (uint32_t x=0; x < w; ++x)
		{
			if (EdgeDetect(img, w, h, x, y))
			{
				surface[y*w + x] = 1;
			}
		}
	}

	// brute force search
	for (uint32_t y=0; y < h; ++y)
	{
		for (uint32_t x=0; x < w; ++x)
		{
			uint32_t i, j;
			float d;
			FindNeighbour(&surface[0], w, h, x, y, i, j, d);
		
			// flip sign for pixels inside the shape	
			float sign = (img[y*w + x])?-1.0f:1.0f; 

			output[y*w + x] = d*sign/w;
		}	
	}
}
*/
