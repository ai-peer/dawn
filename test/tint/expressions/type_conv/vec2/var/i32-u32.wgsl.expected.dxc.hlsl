[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static int2 u = int2(1, 1);

void f() {
  const uint2 v = uint2(u);
}
