[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static bool4 u = bool4(true, true, true, true);

void f() {
  const int4 v = int4(u);
}
