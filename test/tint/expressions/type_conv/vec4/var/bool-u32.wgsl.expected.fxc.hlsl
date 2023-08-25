[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static bool4 u = bool4(true, true, true, true);

void f() {
  const uint4 v = uint4(u);
}
