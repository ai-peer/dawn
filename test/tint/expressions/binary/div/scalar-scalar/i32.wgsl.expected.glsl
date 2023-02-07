#version 310 es

int tint_div(int lhs, int rhs) {
  int rhs_or_one = (bool(uint((rhs == 0)) | uint(bool(uint((lhs == -2147483648)) & uint((rhs == -1))))) ? 1 : rhs);
  return (lhs / rhs_or_one);
}

void f() {
  int a = 1;
  int b = 2;
  int r = tint_div(a, b);
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
