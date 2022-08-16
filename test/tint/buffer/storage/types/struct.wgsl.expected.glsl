#version 310 es

struct Inner {
  float f;
};

struct S {
  Inner inner;
};

layout(binding = 0, std430) buffer S_1 {
  S _;
} tint_symbol;
layout(binding = 1, std430) buffer S_2 {
  S _;
} tint_symbol_1;
void tint_symbol_2() {
  tint_symbol_1._ = tint_symbol._;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol_2();
  return;
}
