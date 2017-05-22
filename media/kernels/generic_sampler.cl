#include "media/kernels/utils.h"

kernel void MAKE_IMPL(sample, BRUSH_TYPE) (read_only image3d_t samples_in,
                                           write_only image3d_t samples_out,
                                           int operation_type,
                                           float3 chunk_origin,
                                           float3 brush_origin,
                                           float3 brush_scale,
                                           mat3 brush_rotation) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);

    const float old_sample =
            read_imagef(samples_in, nearest_sampler, (int4)(x, y, z, 0)).x;
    const float3 p = vertex_at(x, y, z, chunk_origin) - brush_origin;
    const float3 v = mul_mat3_float3(brush_rotation, p);

    float new_sample = MAKE_IMPL(sdf, BRUSH_TYPE) (v, brush_scale);
    if (operation_type == 0) {
        new_sample = min(new_sample, old_sample);
    } else {
        new_sample = max(-new_sample, old_sample);
    }
    write_imagef(samples_out, (int4)(x, y, z, 0),
                 (float4)(new_sample, 0, 0, 0));
}
