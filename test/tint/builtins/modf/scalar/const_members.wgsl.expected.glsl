#version 310 es

void tint_symbol() {
  float fract_1 = 0.25f;
  float whole = 1.0f;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
