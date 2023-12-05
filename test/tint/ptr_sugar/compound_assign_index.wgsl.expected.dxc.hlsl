void set_int3(inout int3 vec, int idx, int val) {
  vec = (idx.xxx == int3(0, 1, 2)) ? val.xxx : vec;
}

void deref() {
  int3 a = int3(0, 0, 0);
  const int tint_symbol_1 = 0;
  set_int3(a, tint_symbol_1, (a[tint_symbol_1] + 42));
}

void no_deref() {
  int3 a = int3(0, 0, 0);
  a[0] = (a[0] + 42);
}

[numthreads(1, 1, 1)]
void main() {
  deref();
  no_deref();
  return;
}
