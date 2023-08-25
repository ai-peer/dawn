[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static bool2 u = bool2(true, true);

void f() {
  const int2 v = int2(u);
}
