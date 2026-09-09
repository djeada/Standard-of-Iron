

float ground_tactical_distance(float view_distance) {
  return smoothstep(32.0, 85.0, view_distance);
}
