#version 310 es

void deref() {
  ivec3 a = ivec3(0, 0, 0);
  int tint_symbol_2 = 0;
  a[tint_symbol_2] = (a[tint_symbol_2] + 42);
}

void no_deref() {
  ivec3 a = ivec3(0, 0, 0);
  a[0] = (a[0] + 42);
}

void tint_symbol() {
  deref();
  no_deref();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
