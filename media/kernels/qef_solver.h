/**
 * Following an excellent: "Matrix Computations 4th Edition" - G.H.Golub & C.F.
 * Van Loan.
 *
 * Chapter 8.5.3 - The Classical Jacobi Algorithm,
 */

void sym_schur2(const float A[3][3], int p, int q, float *out_c, float *out_s) {
    float c = 1.0f;
    float s = 0.0f;
    if (A[p][q] != 0) {
        float tau = (A[q][q] - A[p][p]) / (2.0f * A[p][q]);
        float t;
        if (tau >= 0) {
            t = 1.0f / (tau + hypot(1.0f, tau));
        } else {
            t = 1.0f / (tau - hypot(1.0f, tau));
        }
        c = 1.0f / hypot(1.0f, t);
        s = t * c;
    }
    *out_c = c;
    *out_s = s;
}

void jacobi_eigen(float A[3][3], float V[3][3]) {
    int sweeps_to_go = 8;
    while (sweeps_to_go-- > 0) {
        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                float c;
                float s;
                sym_schur2(A, p, q, &c, &s);

                /* A = J^T * A */
                for (int i = 0; i < 3; ++i) {
                    const float Api = A[p][i];
                    const float Aqi = A[q][i];
                    A[p][i] = c * Api - s * Aqi;
                    A[q][i] = s * Api + c * Aqi;
                }

                /* A = A * J */
                for (int i = 0; i < 3; ++i) {
                    const float Aip = A[i][p];
                    const float Aiq = A[i][q];
                    A[i][p] = c * Aip - s * Aiq;
                    A[i][q] = s * Aip + c * Aiq;
                }

                /* V = V * J */
                for (int i = 0; i < 3; ++i) {
                    const float Vip = V[i][p];
                    const float Viq = V[i][q];
                    V[i][p] = c * Vip - s * Viq;
                    V[i][q] = s * Vip + c * Viq;
                }
            }
        }
    }
}

/**
 * Computes Moore-Penrose pseudoinverse.
 *
 * NOTE 0: S is trashed.
 * NOTE 1: V is untouched.
 * NOTE 2: R is the resulting pseudo-inverse.
 */
void pseudo_inverse(const float V[3][3],
                    float S[3][3],
                    float R[3][3],
                    float threshold) {
    float inv_singular_values[3];
    /* Truncate small singular values */
    for (int i = 0; i < 3; ++i) {
        if (S[i][i] < threshold || 1.0f / S[i][i] < threshold) {
            inv_singular_values[i] = 0.0f;
        } else {
            inv_singular_values[i] = 1.0f / S[i][i];
        }
    }
    /* S = V * s */
    for (int j = 0; j < 3; ++j) {
        for (int i = 0; i < 3; ++i) {
            S[j][i] = V[j][i] * inv_singular_values[i];
        }
    }
    /* R = S * V^T */
    for (int j = 0; j < 3; ++j) {
        for (int i = 0; i < 3; ++i) {
            float value = 0.0f;
            for (int k = 0; k < 3; ++k) {
                value += S[j][k] * V[i][k];
            }
            R[j][i] = value;
        }
    }
}

void solve_lstsq(float A[3][3], float b[3], float x[3], float threshold) {
    float V[3][3] = {
        { 1, 0, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 }
    };
    float R[3][3];
    float s[3];
    jacobi_eigen(A, V);
    pseudo_inverse(V, A, R, threshold);
    for (int j = 0; j < 3; ++j) {
        float value = 0.0f;
        for (int k = 0; k < 3; ++k) {
            value += R[j][k] * b[k];
        }
        x[j] = value;
    }
}

float3 qef_solve(const float4 *points,
                 const float4 *normals,
                 uint num_points) {
    /**
     * In the original Dual Contouring paper, they perform some kind of implicit
     * QR on the data just to form C = R^T * R in the end. Maybe I'm missing
     * something, but it seems superflous, as from the numerical stanpoint
     * cond(R^T * R) >= cond(A^T * A) meaning that what is done here won't be
     * worse.
     *
     * So... one may simply compute A^T*A instead...
     */
    // clang-format off
    float AtA[3][3] = {
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 }
    };
    float masspoint[3] = {
        0, 0, 0
    };
    float b[3] = {
        0, 0, 0
    };
    // clang-format on

    for (uint i = 0; i < num_points; ++i) {
        /* Form A^T * A where A is a matrix containing normals */
        AtA[0][0] = fma(normals[i].x, normals[i].x, AtA[0][0]);
        AtA[0][1] = fma(normals[i].x, normals[i].y, AtA[0][1]);
        AtA[0][2] = fma(normals[i].x, normals[i].z, AtA[0][2]);

        AtA[1][1] = fma(normals[i].y, normals[i].y, AtA[1][1]);
        AtA[1][2] = fma(normals[i].y, normals[i].z, AtA[1][2]);
        AtA[2][2] = fma(normals[i].z, normals[i].z, AtA[2][2]);

        masspoint[0] += points[i].x;
        masspoint[1] += points[i].y;
        masspoint[2] += points[i].z;

        const float t = dot(points[i], normals[i]);
        b[0] = fma(normals[i].x, t, b[0]);
        b[1] = fma(normals[i].y, t, b[1]);
        b[2] = fma(normals[i].z, t, b[2]);
    }
    /* Let the symmetry be complete */
    AtA[1][0] = AtA[0][1];
    AtA[2][0] = AtA[0][2];
    AtA[2][1] = AtA[1][2];

    masspoint[0] /= num_points;
    masspoint[1] /= num_points;
    masspoint[2] /= num_points;

    for (int j = 0; j < 3; ++j) {
        for (int i = 0; i < 3; ++i) {
            b[j] -= AtA[j][i] * masspoint[i];
        }
    }

    float result[3];
    solve_lstsq(AtA, b, result, 0.1f);
    return (float3)(result[0] + masspoint[0],
                    result[1] + masspoint[1],
                    result[2] + masspoint[2]);
}
