[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static int2 u = int2(1, 1);

void f() {
  const bool2 v = bool2(u);
}
