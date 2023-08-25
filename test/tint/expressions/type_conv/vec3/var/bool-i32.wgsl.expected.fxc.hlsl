[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static bool3 u = bool3(true, true, true);

void f() {
  const int3 v = int3(u);
}
