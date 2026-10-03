

float ground_tactical_distance(float view_distance) {
  return smoothstep(32.0, 85.0, view_distance);
}

const float k_ground_chroma_knee = 0.44;
const float k_ground_chroma_ratio = 0.45;

vec3 ground_vegetation_chroma(vec3 color) {
  float peak = max(max(color.r, color.g), color.b);
  float floor_channel = min(min(color.r, color.g), color.b);
  float saturation = (peak - floor_channel) / max(peak, 1e-4);
  if (saturation <= k_ground_chroma_knee) {
    return color;
  }
  float target = k_ground_chroma_knee +
                 (saturation - k_ground_chroma_knee) * k_ground_chroma_ratio;
  float leafy = smoothstep(0.0, 0.06, color.g - max(color.r, color.b));
  vec3 gray = vec3(peak);
  return mix(color, mix(gray, color, target / saturation), leafy);
}
