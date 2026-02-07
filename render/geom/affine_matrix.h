#pragma once

#include <QMatrix4x4>

namespace Render::Geom {

// Fast affine multiply for transforms where both matrices have
// bottom row [0, 0, 0, 1].
inline auto multiply_affine(const QMatrix4x4 &a, const QMatrix4x4 &b)
    -> QMatrix4x4 {
  const float *A = a.constData();
  const float *B = b.constData();

  QMatrix4x4 out;
  float *O = out.data();

  auto mul_col = [&](int b_col_offset, float bw, int o_col_offset) {
    const float bx = B[b_col_offset + 0];
    const float by = B[b_col_offset + 1];
    const float bz = B[b_col_offset + 2];
    O[o_col_offset + 0] = A[0] * bx + A[4] * by + A[8] * bz + A[12] * bw;
    O[o_col_offset + 1] = A[1] * bx + A[5] * by + A[9] * bz + A[13] * bw;
    O[o_col_offset + 2] = A[2] * bx + A[6] * by + A[10] * bz + A[14] * bw;
    O[o_col_offset + 3] = bw;
  };

  mul_col(0, 0.0F, 0);
  mul_col(4, 0.0F, 4);
  mul_col(8, 0.0F, 8);
  mul_col(12, 1.0F, 12);
  O[15] = 1.0F;

  return out;
}

} // namespace Render::Geom
