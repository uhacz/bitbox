#pragma once

// The "wrap" parameters can be used to create wraparound noise that
// wraps at powers of two. The numbers MUST be powers of two. Specify
// 0 to mean "don't care". (The noise always wraps every 256 due
// details of the implementation, even if you ask for larger or no
// wrapping.)
float bxNoise_perlin(float x, float y, float z, int x_wrap = 0, int y_wrap = 0, int z_wrap = 0 );
void bxNoise_perlin( float out[4], float x, float y, float z );
void bxNoise_fbm( float out[4], float x, float y, float z, int octaves, float wstep = 0.5f, float dstep = 2.f );