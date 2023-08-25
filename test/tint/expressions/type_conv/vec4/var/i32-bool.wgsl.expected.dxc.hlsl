[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static int4 u = int4(1, 1, 1, 1);

void f() {
  const bool4 v = bool4(u);
}
