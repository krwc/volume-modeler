#cmakedefine VM_CHUNK_BORDER @VM_CHUNK_BORDER@
#cmakedefine VM_CHUNK_SIZE @VM_CHUNK_SIZE@
#cmakedefine VM_VOXEL_SIZE @VM_VOXEL_SIZE@

#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

#include "media/kernels/utils.h"

float sdf_ball(float3 p, float3 scale) {
    return (length(p / scale) - 1) * min(min(scale.x, scale.y), scale.z);
}

float sdf_cube(float3 p, float3 scale) {
    p = fabs(p);
    return max(p.x - scale.x, max(p.y - scale.y, p.z - scale.z));
}

#define BRUSH_TYPE ball
#include "media/kernels/generic_sampler.cl"
#undef BRUSH_TYPE

#define BRUSH_TYPE cube
#include "media/kernels/generic_sampler.cl"
#undef BRUSH_TYPE
