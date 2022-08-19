#version 310 es

struct u_block {
  mat2 inner;
};

layout(binding = 0) uniform u_block_1 {
  u_block _;
} u;

void tint_symbol() {
  mat2 x = u._.inner;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
