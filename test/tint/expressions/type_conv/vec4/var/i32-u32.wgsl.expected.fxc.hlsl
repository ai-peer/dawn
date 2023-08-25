[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static int4 u = int4(1, 1, 1, 1);

void f() {
  const uint4 v = uint4(u);
}
