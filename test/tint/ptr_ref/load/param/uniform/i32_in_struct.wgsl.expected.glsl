#version 310 es

layout(binding = 0, std140) uniform str_ubo {
  int i;
  uint pad;
  uint pad_1;
  uint pad_2;
} S;

int func_S_i() {
  return S.i;
}

void tint_symbol() {
  int r = func_S_i();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
