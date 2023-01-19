#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
layout(binding = 0, std430) buffer S_block_ssbo {
  vec4 inner[];
} S;

void min_1() {
}

void f(int i) {
  min_1();
  S.inner[min(uint(i), (uint(S.inner.length()) - 1u))] = vec4(1.0f);
}

