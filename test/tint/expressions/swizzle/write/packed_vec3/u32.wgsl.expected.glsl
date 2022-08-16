#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
struct S {
  uvec3 v;
};

layout(binding = 0, std430) buffer S_1 {
  S _;
} U;
void f() {
  U._.v = uvec3(1u, 2u, 3u);
  U._.v.x = 1u;
  U._.v.y = 2u;
  U._.v.z = 3u;
}

