vec3 perturb_normal(vec3 n, vec3 world_pos, float height, float strength) {
  vec3 dpdx = dFdx(world_pos);
  vec3 dpdy = dFdy(world_pos);
  vec3 r1 = cross(dpdy, n);
  vec3 r2 = cross(n, dpdx);
  float det = dot(dpdx, r1);
  vec3 gradient = sign(det) * (dFdx(height) * r1 + dFdy(height) * r2);
  return normalize(abs(det) * n - strength * gradient);
}

vec3 lobe_normal(vec3 n,
                 vec3 local_pos,
                 float frequency,
                 float strength,
                 vec3 seed,
                 out float height) {
  const vec3 k0 = vec3(0.86, 0.32, 0.40);
  const vec3 k1 = vec3(-0.44, 0.78, 0.44);
  const vec3 k2 = vec3(0.30, -0.52, 0.80);
  const vec3 k3 = vec3(-0.70, -0.60, -0.38);
  const vec3 k4 = vec3(0.55, 0.62, -0.56);
  vec3 p = local_pos * frequency;
  float a0 = dot(p, k0) + seed.x;
  float a1 = dot(p, k1) * 1.31 + seed.y;
  float a2 = dot(p, k2) * 1.73 + seed.z;
  float a3 = dot(p, k3) * 2.21 + seed.x * 1.7;
  float a4 = dot(p, k4) * 2.87 + seed.y * 2.3;
  height = (sin(a0) * 1.00 + sin(a1) * 0.85 + sin(a2) * 0.70 + sin(a3) * 0.55 +
            sin(a4) * 0.45) /
           3.55;
  vec3 gradient = k0 * cos(a0) * 1.00 + k1 * cos(a1) * 0.85 + k2 * cos(a2) * 0.70 +
                  k3 * cos(a3) * 0.55 + k4 * cos(a4) * 0.45;
  vec3 tangent_gradient = gradient - n * dot(gradient, n);
  return normalize(n - tangent_gradient * strength);
}
