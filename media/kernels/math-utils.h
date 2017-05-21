#ifndef MATH_UTILS_H
#define MATH_UTILS_H

typedef struct {
    float3 row0;
    float3 row1;
    float3 row2;
} mat3;

typedef struct {
    float4 row0;
    float4 row1;
    float4 row2;
    float4 row3;
} mat4;

float3 mul_mat3_vec3(mat3 A, float3 v) {
    return (float3)(dot(A.row0, v), dot(A.row1, v), dot(A.row2, v));
}

float4 mul_mat4_vec4(mat4 A, float4 v) {
    return (float4)(dot(A.row0, v), dot(A.row1, v), dot(A.row2, v), dot(A.row3, v));
}

#endif // MATH_UTILS_H
