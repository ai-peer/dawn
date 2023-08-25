[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static int3 u = int3(1, 1, 1);

void f() {
  const uint3 v = uint3(u);
}
