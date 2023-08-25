[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static int2 arr[2] = {int2(1, 1), int2(2, 2)};

void f() {
  int2 v[2] = arr;
}
