#version 310 es
precision highp float;
precision highp int;

layout(rgba8) uniform highp writeonly image2D tex;
void fragment_main() {
  vec4 value = vec4(1.0f, 2.0f, 3.0f, 4.0f);
  imageStore(tex, ivec2(9, 8), value.bgra);
}

void main() {
  fragment_main();
  return;
}
