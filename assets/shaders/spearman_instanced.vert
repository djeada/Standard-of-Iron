#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

// Per-instance attributes: compressed model matrix (3x4) + color/alpha
layout(location = 3) in vec4 i_modelCol0;  // column 0 (xyz) + translation.x (w)
layout(location = 4) in vec4 i_modelCol1;  // column 1 (xyz) + translation.y (w)
layout(location = 5) in vec4 i_modelCol2;  // column 2 (xyz) + translation.z (w)
layout(location = 6) in vec4 i_colorAlpha; // rgb color + alpha

uniform mat4 u_viewProj;

out vec3 v_normal;
out vec2 v_texCoord;
out vec3 v_worldPos;
out vec3 v_color;
out float v_alpha;

void main() {
  // Reconstruct 4x4 model matrix from compressed columns
  mat4 model = mat4(
    vec4(i_modelCol0.xyz, 0.0),
    vec4(i_modelCol1.xyz, 0.0),
    vec4(i_modelCol2.xyz, 0.0),
    vec4(i_modelCol0.w, i_modelCol1.w, i_modelCol2.w, 1.0)
  );

  // Calculate normal matrix (inverse transpose of model)
  mat3 normalMatrix = mat3(transpose(inverse(model)));

  v_normal = normalMatrix * a_normal;
  v_texCoord = a_texCoord;
  v_worldPos = vec3(model * vec4(a_position, 1.0));
  v_color = i_colorAlpha.rgb;
  v_alpha = i_colorAlpha.a;

  gl_Position = u_viewProj * vec4(v_worldPos, 1.0);
}
