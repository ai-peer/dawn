#version 310 es

layout(binding = 0, std430) buffer str_ssbo {
  int arr[4];
} S;

void func_S_arr() {
  int tint_symbol_1[4] = int[4](0, 0, 0, 0);
  S.arr = tint_symbol_1;
}

void tint_symbol() {
  func_S_arr();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
