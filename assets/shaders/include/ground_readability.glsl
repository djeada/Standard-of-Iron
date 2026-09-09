// Keep small ground features subordinate to units at normal gameplay distances.
// Shared by terrain and scatter so their detail transitions stay in step.
float ground_tactical_distance(float view_distance) {
  return smoothstep(32.0, 85.0, view_distance);
}
