#version 310 es

void tint_symbol() {
  float fract_1 = 0.625f;
  int exp_1 = 1;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
