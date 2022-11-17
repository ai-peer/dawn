#version 310 es

void tint_symbol() {
  vec4 a = vec4(0.0f);
  vec4 b = vec4(57.295776367f);
  vec4 c = vec4(57.295776367f, 114.591552734f, 171.887329102f, 229.183105469f);
  vec3 d = vec3(0.0f);
  vec3 e = vec3(57.295776367f);
  vec3 f = vec3(57.295776367f, 114.591552734f, 171.887329102f);
  vec2 g = vec2(0.0f);
  vec2 h = vec2(57.295776367f);
  vec2 i = vec2(57.295776367f, 114.591552734f);
  float j = 57.295780182f;
  float k = 114.591560364f;
  float l = 171.88734436f;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
