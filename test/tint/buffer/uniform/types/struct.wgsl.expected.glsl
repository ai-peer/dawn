#version 310 es

struct Inner {
  float f;
};

struct S {
  Inner inner;
};

struct u_block {
  S inner;
};

layout(binding = 0) uniform u_block_1 {
  S inner;
} u;

void tint_symbol() {
  S x = u.inner;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
