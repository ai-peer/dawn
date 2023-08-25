[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static uint4 u = uint4(1u, 1u, 1u, 1u);

void f() {
  const int4 v = int4(u);
}
