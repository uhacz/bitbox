#include "SPHKernels.h"

using namespace PBD;

f32 CubicKernel::m_radius;
f32 CubicKernel::m_k;
f32 CubicKernel::m_l;
f32 CubicKernel::m_W_zero;

f32 Poly6Kernel::m_radius;
f32 Poly6Kernel::m_k;
f32 Poly6Kernel::m_l;
f32 Poly6Kernel::m_m;
f32 Poly6Kernel::m_W_zero;

f32 SpikyKernel::m_radius;
f32 SpikyKernel::m_k;
f32 SpikyKernel::m_l;
f32 SpikyKernel::m_W_zero;