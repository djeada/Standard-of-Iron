#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

// Instance attributes (model matrix packed as 3 vec4s + translation in w components)
layout(location = 3) in vec4 i_model_col0;  // col0.xyz + translation.x
layout(location = 4) in vec4 i_model_col1;  // col1.xyz + translation.y
layout(location = 5) in vec4 i_model_col2;  // col2.xyz + translation.z
layout(location = 6) in vec4 i_color_alpha; // rgb + alpha

uniform mat4 u_viewProj;

out vec3 v_normal;
out vec2 v_texCoord;
out vec3 v_worldPos;
out vec3 v_color;
out float v_alpha;

void main() {
  // Reconstruct model matrix from instance data
  mat4 model = mat4(
    vec4(i_model_col0.xyz, 0.0),
    vec4(i_model_col1.xyz, 0.0),
    vec4(i_model_col2.xyz, 0.0),
    vec4(i_model_col0.w, i_model_col1.w, i_model_col2.w, 1.0)
  );

  // For normal transformation, extract 3x3 rotation/scale part
  // For uniform scaling, we can just use the upper 3x3 directly
  // This is faster than transpose(inverse(model)) and correct for most cases
  mat3 normalMatrix = mat3(i_model_col0.xyz, i_model_col1.xyz, i_model_col2.xyz);
  v_normal = normalize(normalMatrix * a_normal);
  
  v_texCoord = a_texCoord;
  v_worldPos = vec3(model * vec4(a_position, 1.0));
  v_color = i_color_alpha.rgb;
  v_alpha = i_color_alpha.a;
  
  gl_Position = u_viewProj * model * vec4(a_position, 1.0);
}
