[numthreads(1, 1, 1)]
void f() {
  const int a = 1;
  int b = 0;
  const int c = (a / (b == 0 ? 1 : b));
  return;
}
