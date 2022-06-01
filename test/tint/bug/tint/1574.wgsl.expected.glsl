#version 310 es

struct atomic_compare_exchange_resultu32 {
  uint old_value;
  bool exchanged;
};


struct a_block {
  uint inner;
};

layout(binding = 0, std430) buffer a_block_1 {
  uint inner;
} a;
void tint_symbol() {
  uint value = 42u;
  atomic_compare_exchange_resultu32 atomic_compare_result;
  atomic_compare_result.old_value = atomicCompSwap(a.inner, 0u, value);
  atomic_compare_result.exchanged = atomic_compare_result.old_value == 0u;
  atomic_compare_exchange_resultu32 r1 = atomic_compare_result;
  atomic_compare_exchange_resultu32 atomic_compare_result_1;
  atomic_compare_result_1.old_value = atomicCompSwap(a.inner, 0u, value);
  atomic_compare_result_1.exchanged = atomic_compare_result_1.old_value == 0u;
  atomic_compare_exchange_resultu32 r2 = atomic_compare_result_1;
  atomic_compare_exchange_resultu32 atomic_compare_result_2;
  atomic_compare_result_2.old_value = atomicCompSwap(a.inner, 0u, value);
  atomic_compare_result_2.exchanged = atomic_compare_result_2.old_value == 0u;
  atomic_compare_exchange_resultu32 r3 = atomic_compare_result_2;
}

layout(local_size_x = 16, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
