[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static int4 u = int4(1, 1, 1, 1);

void f() {
  const vector<float16_t, 4> v = vector<float16_t, 4>(u);
}
