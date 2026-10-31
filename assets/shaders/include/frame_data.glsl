layout(std140) uniform FrameData {
  mat4 u_view_proj;
  float u_frame_depth_only;
};

bool soi_depth_only_pass() {
  return u_frame_depth_only > 0.5;
}
