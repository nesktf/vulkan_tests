#version 450

layout(location = 0) in vec2 att_coords;
layout(location = 1) in vec3 att_color;

layout(location = 0) out vec3 frag_color;

// vec2 positions[3] = vec2[](
//   vec2(.0, -.5),
//   vec2(.5, .5),
//   vec2(-.5, .5)
// );
//
// vec3 colors[3] = vec3[](
//   vec3(1., 0., 0.),
//   vec3(0., 1., 0.),
//   vec3(0., 0., 1.)
// );

void main() {
  // gl_Position = vec4(positions[gl_VertexIndex].xy, .0, 1.);
  // frag_color = colors[gl_VertexIndex];
  gl_Position = vec4(att_coords, 0.f, 1.f);
  frag_color = att_color;
}
