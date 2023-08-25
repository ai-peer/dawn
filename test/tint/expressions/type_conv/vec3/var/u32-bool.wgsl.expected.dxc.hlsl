[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static uint3 u = uint3(1u, 1u, 1u);

void f() {
  const bool3 v = bool3(u);
}
