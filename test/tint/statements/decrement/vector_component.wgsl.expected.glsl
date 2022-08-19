#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
struct a_block {
  uvec4 inner;
};

layout(binding = 0, std430) buffer a_block_1 {
  a_block _;
} a;
void tint_symbol() {
  int tint_symbol_2 = 1;
  a._.inner[tint_symbol_2] = (a._.inner[tint_symbol_2] - 1u);
  a._.inner.z = (a._.inner.z - 1u);
}

