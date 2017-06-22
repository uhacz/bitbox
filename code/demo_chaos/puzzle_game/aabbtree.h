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

#pragma once

//#include "core.h"
//#include "maths.h"

#include <vector>
#include <util/vectormath/vectormath.h>

class AABBTree
{
	AABBTree(const AABBTree&);
	AABBTree& operator=(const AABBTree&);

public:

    AABBTree(const Vector3F* vertices, u32 numVerts, const u32* indices, u32 numFaces);

	bool TraceRaySlow(const Vector3F& start, const Vector3F& dir, float& outT, float& u, float& v, float& w, float& faceSign, u32& faceIndex) const;
    bool TraceRay(const Vector3F& start, const Vector3F& dir, float& outT, float& u, float& v, float& w, float& faceSign, u32& faceIndex) const;

    void DebugDraw();
    
    Vector3F GetCenter() const { return (m_nodes[0].m_minExtents+m_nodes[0].m_maxExtents)*0.5f; }
    Vector3F GetMinExtents() const { return m_nodes[0].m_minExtents; }
    Vector3F GetMaxExtents() const { return m_nodes[0].m_maxExtents; }
	
#if _WIN32
    // stats (reset each trace)
    static u32 GetTraceDepth() { return s_traceDepth; }
#endif
	
private:

    void DebugDrawRecursive(u32 nodeIndex, u32 depth);

    struct Node
    {
        Node() 	
            : m_numFaces(0)
            , m_faces(NULL)
            , m_minExtents(0.0f)
            , m_maxExtents(0.0f)
        {
        }

		union
		{
			u32 m_children;
			u32 m_numFaces;			
		};

		u32* m_faces;        
        Vector3F m_minExtents;
        Vector3F m_maxExtents;
    };


    struct Bounds
    {
        Bounds() : m_min(0.0f), m_max(0.0f)
        {
        }

        Bounds(const Vector3F& min, const Vector3F& max) : m_min(min), m_max(max)
        {
        }

        inline float GetVolume() const
        {
            Vector3F e = m_max-m_min;
            return (e.x*e.y*e.z);
        }

        inline float GetSurfaceArea() const
        {
            Vector3F e = m_max-m_min;
            return 2.0f*(e.x*e.y + e.x*e.z + e.y*e.z);
        }

        inline void Union(const Bounds& b)
        {
            m_min = minPerElem(m_min, b.m_min);
            m_max = maxPerElem(m_max, b.m_max);
        }

        Vector3F m_min;
        Vector3F m_max;
    };

    typedef std::vector<u32> IndexArray;
    typedef std::vector<Vector3F> PositionArray;
    typedef std::vector<Node> NodeArray;
    typedef std::vector<u32> FaceArray;
    typedef std::vector<Bounds> FaceBoundsArray;

	// partition the objects and return the number of objects in the lower partition
	u32 PartitionMedian(Node& n, u32* faces, u32 numFaces);
	u32 PartitionSAH(Node& n, u32* faces, u32 numFaces);

    void Build();
    void BuildRecursive(u32 nodeIndex, u32* faces, u32 numFaces);
    void TraceRecursive(u32 nodeIndex, const Vector3F& start, const Vector3F& dir, float& outT, float& u, float& v, float& w, float& faceSign, u32& faceIndex) const;
 
    void CalculateFaceBounds(u32* faces, u32 numFaces, Vector3F& outMinExtents, Vector3F& outMaxExtents);
    u32 GetNumFaces() const { return m_numFaces; }
	u32 GetNumNodes() const { return u32(m_nodes.size()); }

	// track the next free node
	u32 m_freeNode;

    const Vector3F* m_vertices;
    const u32 m_numVerts;

    const u32* m_indices;
    const u32 m_numFaces;

    FaceArray m_faces;
    NodeArray m_nodes;
    FaceBoundsArray m_faceBounds;    

    // stats
    u32 m_treeDepth;
    u32 m_innerNodes;
    u32 m_leafNodes; 
	
#if _WIN32
   _declspec (thread) static u32 s_traceDepth;
#endif
};
