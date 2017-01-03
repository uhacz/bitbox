// PixelShader:  SSAOCalculate, entry: SSAOCalculate
// PixelShader:  SSAOBlurHorizontal, entry: BlurSSAO, defines: HORIZONTAL
// PixelShader:  SSAOBlurVertical, entry: BlurSSAO

passes:
{
    ssao =
    {
        vertex = "vs_screenquad";
        pixel = "ps_SSAO";
        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
            color_mask = "RG";
        };
    };

    blurX = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_Blur";

        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
            color_mask = "RG";
        };

        define =
        {
            HORIZONTAL = 1;
        };
    };

    blurY = 
    {
        vertex = "vs_screenquad";
        pixel = "ps_Blur";

        hwstate =
        {
            depth_test = 0;
            depth_write = 0;
            color_mask = "R";
        };
    };

}; #~header

#include <sys/vs_screenquad.hlsl>
#include <sys/binding_map.h>
#include <sys/samplers.hlsl>
#include "ssao_data.h"

Texture2D<float>  InputTextureLinearDepth       : register(t0);
Texture2D<float2> InputTextureSSAO              : register(t1);
//Texture2D<float2> InputTextureMotion            : register(t2);
Texture2D<float4> InputTextureNormals           : register(t3);

/*
Source of whole algorithm: http://graphics.cs.williams.edu/papers/SAOHPG12/ plus some minor modifications (right now I removed mips)

Source licence:
\author Morgan McGuire and Michael Mara, NVIDIA Research

Reference implementation of the Scalable Ambient Obscurance (SAO) screen-space ambient obscurance algorithm.

The optimized algorithmic structure of SAO was published in McGuire, Mara, and Luebke, Scalable Ambient Obscurance,
<i>HPG</i> 2012, and was developed at NVIDIA with support from Louis Bavoil.

The mathematical ideas of AlchemyAO were first described in McGuire, Osman, Bukowski, and Hennessy, The
Alchemy Screen-Space Ambient Obscurance Algorithm, <i>HPG</i> 2011 and were developed at
Vicarious Visions.

DX11 HLSL port by Leonardo Zide of Treyarch

<hr>

Open Source under the "BSD" license: http://www.opensource.org/licenses/bsd-license.php

Copyright (c) 2011-2012, NVIDIA
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


*/

#define NUM_SAMPLES (8)

static const int ROTATIONS[] = { 1, 1, 2, 3, 2, 5, 2, 3, 2,
3, 3, 5, 5, 3, 4, 7, 5, 5, 7,
9, 8, 5, 5, 7, 7, 7, 8, 5, 8,
11, 12, 7, 10, 13, 8, 11, 8, 7, 14,
11, 11, 13, 12, 13, 19, 17, 13, 11, 18,
19, 11, 11, 14, 17, 21, 15, 16, 17, 18,
13, 17, 11, 17, 19, 18, 25, 18, 19, 19,
29, 21, 19, 27, 31, 29, 21, 18, 17, 29,
31, 31, 23, 18, 25, 26, 25, 23, 19, 34,
19, 27, 21, 25, 39, 29, 17, 21, 27 };

/** Used for preventing AO computation on the sky (at infinite depth) and defining the CS Z to bilateral depth key scaling.
This need not match the real far plane*/
#define FAR_PLANE_Z (90.0)

// This is the number of turns around the circle that the spiral pattern makes.  This should be prime to prevent
// taps from lining up.  This particular choice was tuned for NUM_SAMPLES == 9
static const int NUM_SPIRAL_TURNS = ROTATIONS[NUM_SAMPLES-1];

/** World-space AO radius in scene units (r).  e.g., 1.0m */
static const float radius = 0.7;
/** radius*radius*/
static const float radius2 = (radius*radius);

/** Bias to avoid AO in smooth corners, e.g., 0.01m */
static const float bias = 0.02f;

/** The height in pixels of a 1m object if viewed from 1m away.
You can compute it from your projection matrix.  The actual value is just
a scale factor on radius; you can simply hardcode this to a constant (~500)
and make your radius value unitless (...but resolution dependent.)  */
static const float projScale = 250.0f;

/** Reconstruct camera-space P.xyz from screen-space S = (x, y) in
pixels and camera-space z < 0.  Assumes that the upper-left pixel center
is at (0.5, 0.5) [but that need not be the location at which the sample tap
was placed!]
*/
float3 reconstructCSPosition(float2 S, float z)
{
    return float3((S * g_ReprojectInfoHalfResFromInt.xy + g_ReprojectInfoHalfResFromInt.zw)*z, z);
}

/** Reconstructs screen-space unit normal from screen-space position */
float3 reconstructCSFaceNormal(float3 C)
{
    return normalize(cross(ddy_fine(C), ddx_fine(C)));
}

/** Returns a unit vector and a screen-space radius for the tap on a unit disk (the caller should scale by the actual disk radius) */
float2 tapLocation(int sampleNumber, float spinAngle, out float ssR)
{
    // Radius relative to ssR
    float alpha = float(sampleNumber + 0.5) * (1.0 / NUM_SAMPLES);
    float angle = alpha * (NUM_SPIRAL_TURNS * 6.28) + spinAngle;

    ssR = alpha;
    float sin_v, cos_v;
    sincos(angle, sin_v, cos_v);
    return float2(cos_v, sin_v);
}

/** Read the camera-space position of the point at screen-space pixel ssP */
float3 getPosition(int2 ssP)
{
    float3 P;

    P.z = InputTextureLinearDepth.Load(int3(ssP, 0)).r;

    // Offset to pixel center
    P = reconstructCSPosition(float2(ssP), P.z);
    return P;
}

/** Read the camera-space position of the point at screen-space pixel ssP + unitOffset * ssR.  Assumes length(unitOffset) == 1 */
float3 getOffsetPosition(int2 ssC, float2 unitOffset, float ssR)
{
    int2 ssP = int2(ssR*unitOffset) + ssC;

    float3 P;

    // Divide coordinate by 2^mipLevel
    P.z = InputTextureLinearDepth.Load(int3(ssP, 0)).r;

    // Offset to pixel center
    P = reconstructCSPosition(float2(ssP), P.z);

    return P;
}


/** Compute the occlusion due to sample with index \a i about the pixel at \a ssC that corresponds
to camera-space point \a C with unit normal \a n_C, using maximum screen-space sampling radius \a ssDiskRadius */
float sampleAO(in int2 ssC, in float3 C, in float3 n_C, in float ssDiskRadius, in int tapIndex, in float randomPatternRotationAngle)
{
    // Offset on the unit disk, spun for this pixel
    float ssR;
    float2 unitOffset = tapLocation(tapIndex, randomPatternRotationAngle, ssR);
    ssR *= ssDiskRadius;

    // The occluding point in camera space
    float3 Q = getOffsetPosition(ssC, unitOffset, ssR);

    float3 v = Q - C;

    float vv = dot(v, v);
    float vn = dot(v, n_C);

    const float epsilon = 0.02f;
    float f = max(radius2 - vv, 0.0);
    return f * f * f * max((vn - bias) * rcp(epsilon + vv), 0.0);
}

float2 ps_SSAO( out_VS_screenquad i) : SV_Target
{
    //float2 vPos = i.position.xy;
    float2 vPos = i.uv.xy * g_renderTargetSize;
    
    // Pixel being shaded 
    int2 ssC = vPos;

    float2 output = float2(1,1);

    // World space point being shaded
    float3 C = getPosition(ssC);

    output.g = C.z;

    bool earlyOut = C.z > FAR_PLANE_Z || C.z < 0.4f || any(ssC < 8) || any(ssC >= (g_renderTargetSize.xy - 8));
    [branch]
    if(earlyOut)
    {
        return output;
    }

    //float2 diffVector = InputTextureMotion.SampleLevel(linearSampler, i.uv, 0);
    //float2 prevFrame = InputTextureSSAO.SampleLevel(linearSampler, i.uv+diffVector, 0);
    //float keyPrevFrame = prevFrame.g;
    //float aoPrevFrame = prevFrame.r;

    // Hash function used in the HPG12 AlchemyAO paper
    float randomPatternRotationAngle = (3 * ssC.x ^ ssC.y + ssC.x * ssC.y) * 10 + g_SSAOPhase;

    float3 normalsWS = InputTextureNormals.Load(int3(ssC, 0)).xyz * 2.0f - 1.0f;
    float3 n_C = mul(g_ViewMatrix, float4(normalsWS, 0.0f)).xyz * float3(-1,1,1);

    // Choose the screen-space sample radius
    float ssDiskRadius = projScale * radius / max(C.z,0.1f);

    float sum = 0.0;

    [unroll]
    for (int i = 0; i < NUM_SAMPLES; ++i) 
    {
         sum += sampleAO(ssC, C, n_C, ssDiskRadius, i, randomPatternRotationAngle);
    }

    const float temp = radius2 * radius;
    sum /= temp * temp;

    float A = max(0.0f, 1.0f - sum * 1.0f * (4.0f / NUM_SAMPLES));

    // bilat filter 1-pixel wide for "free"
	if (abs(ddx_fine(C.z)) < 0.2f) 
    {
		A -= ddx_fine(A) * ((ssC.x & 1) - 0.5);
	}
    if (abs(ddx_fine(C.z)) < 0.2f)
    {
		A -= ddy_fine(A) * ((ssC.y & 1) - 0.5);
	}
    output.r = lerp(A, 1.0f, 1.0f - saturate(0.5f * C.z)); // this algorithm has problems with near surfaces... lerp it out smoothly

    //float difference = saturate(1.0f - 5 * abs(C.z - keyPrevFrame));
    //output.r = lerp(output.r, aoPrevFrame, 0.95f*difference);

    return output;
}

/** Increase to make edges crisper. Decrease to reduce temporal flicker. */
#define EDGE_SHARPNESS     (1.0)

/** Step in 2-pixel intervals since we already blurred against neighbors in the
first AO pass.  This constant can be increased while R decreases to improve
performance at the expense of some dithering artifacts.

Morgan found that a scale of 3 left a 1-pixel checkerboard grid that was
unobjectionable after shading was applied but eliminated most temporal incoherence
from using small numbers of sample taps.
*/
#define SCALE               (2)

/** Filter radius in pixels. This will be multiplied by SCALE. */
#define R                   (2)



//////////////////////////////////////////////////////////////////////////////////////////////

// Gaussian coefficients
static const float gaussian[] =
//	{ 0.356642, 0.239400, 0.072410, 0.009869 };
//	{ 0.398943, 0.241971, 0.053991, 0.004432, 0.000134 };  // stddev = 1.0
  { 0.153170, 0.144893, 0.122649, 0.092902, 0.062970 };  // stddev = 2.0
//  { 0.111220, 0.107798, 0.098151, 0.083953, 0.067458, 0.050920, 0.036108 }; // stddev = 3.0


float2 ps_Blur( out_VS_screenquad i) : SV_Target
{
    //float2 vPos = i.position.xy;
    float2 vPos = i.uv.xy * g_renderTargetSize;

    // Pixel being shaded 
    int2 ssC = vPos;

    float2 output = 1.0f;

    float2 temp = InputTextureSSAO.Load(int3(ssC, 0));

#ifdef HORIZONTAL
    output.g = temp.g;
#endif

    float key = temp.g;

    float sum = temp.r;

    [branch]
    if (key == 1.0)
    {
        // Sky pixel (if you aren't using depth keying, disable this test)
        output.r = sum;
        return output;
    }

    float BASE = gaussian[0];
    float totalWeight = BASE;
    sum *= totalWeight;

    [unroll]
    for (int r = -R; r <= R; ++r)
    {
        // We already handled the zero case above.  This loop should be unrolled and the branch discarded
        if (r != 0)
        {
#ifdef HORIZONTAL
            float2 axis = float2(1, 0);
#else
            float2 axis = float2(0, 1);
#endif
            temp = InputTextureSSAO.Load(int3(ssC + axis * (r * SCALE), 0));
            float tapKey = temp.g;
            float value = temp.r;

            // spatial domain: offset gaussian tap
            float weight = gaussian[abs(r)];

            // range domain (the "bilateral" weight). As depth difference increases, decrease weight.
            weight *= max(0.0, 1.0 - (20.0 * EDGE_SHARPNESS) * abs(tapKey - key));

            sum += value * weight;
            totalWeight += weight;
        }
    }

    const float epsilon = 0.0001;
    output.r = sum / (totalWeight + epsilon);

    return output;
}