#version 330 core

// Mesh vertex attributes
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

// Per-instance attributes (model matrix as 3 columns + color/alpha)
layout(location = 3) in vec4 i_modelCol0;  // First column of model matrix
layout(location = 4) in vec4 i_modelCol1;  // Second column of model matrix
layout(location = 5) in vec4 i_modelCol2;  // Third column of model matrix
layout(location = 6) in vec4 i_colorAlpha; // RGB color + alpha

uniform mat4 u_viewProj;
uniform vec3 u_lightDir;

out vec3 v_worldPos;
out vec3 v_normal;
out vec3 v_color;
out float v_alpha;

void main() {
  // Reconstruct model matrix from columns
  // The 4th column (translation) is in the w components
  mat4 modelMatrix =
      mat4(vec4(i_modelCol0.xyz, 0.0), vec4(i_modelCol1.xyz, 0.0),
           vec4(i_modelCol2.xyz, 0.0),
           vec4(i_modelCol0.w, i_modelCol1.w, i_modelCol2.w, 1.0));

  // Transform position
  vec4 worldPos4 = modelMatrix * vec4(a_position, 1.0);
  v_worldPos = worldPos4.xyz;

  // Transform normal (using transpose of inverse for non-uniform scaling)
  mat3 normalMatrix = mat3(modelMatrix);
  // For uniform scaling, we can simplify
  v_normal = normalize(normalMatrix * a_normal);

  // Pass through color and alpha
  v_color = i_colorAlpha.rgb;
  v_alpha = i_colorAlpha.a;

  gl_Position = u_viewProj * worldPos4;
}
