[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static uint2 arr[2] = {uint2(1u, 1u), uint2(2u, 2u)};

void f() {
  uint2 v[2] = arr;
}
