#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
struct S {
  ivec4 arr[4];
};

struct S_nested {
  int arr[4][3][2];
};

ivec4 src_private[4] = ivec4[4](ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0));
shared ivec4 src_workgroup[4];
layout(binding = 0) uniform S_1 {
  S _;
} src_uniform;

layout(binding = 1, std430) buffer S_2 {
  S _;
} src_storage;
layout(binding = 2, std430) buffer S_3 {
  S _;
} dst;
layout(binding = 3, std430) buffer S_nested_1 {
  S_nested _;
} dst_nested;
ivec4[4] ret_arr() {
  ivec4 tint_symbol_2[4] = ivec4[4](ivec4(0), ivec4(0), ivec4(0), ivec4(0));
  return tint_symbol_2;
}

S ret_struct_arr() {
  S tint_symbol_3 = S(ivec4[4](ivec4(0), ivec4(0), ivec4(0), ivec4(0)));
  return tint_symbol_3;
}

void foo(ivec4 src_param[4]) {
  ivec4 src_function[4] = ivec4[4](ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0));
  ivec4 tint_symbol_4[4] = ivec4[4](ivec4(1), ivec4(2), ivec4(3), ivec4(3));
  dst._.arr = tint_symbol_4;
  dst._.arr = src_param;
  ivec4 tint_symbol[4] = ret_arr();
  dst._.arr = tint_symbol;
  ivec4 src_let[4] = ivec4[4](ivec4(0), ivec4(0), ivec4(0), ivec4(0));
  dst._.arr = src_let;
  dst._.arr = src_function;
  dst._.arr = src_private;
  dst._.arr = src_workgroup;
  S tint_symbol_1 = ret_struct_arr();
  dst._.arr = tint_symbol_1.arr;
  dst._.arr = src_uniform._.arr;
  dst._.arr = src_storage._.arr;
  int src_nested[4][3][2] = int[4][3][2](int[3][2](int[2](0, 0), int[2](0, 0), int[2](0, 0)), int[3][2](int[2](0, 0), int[2](0, 0), int[2](0, 0)), int[3][2](int[2](0, 0), int[2](0, 0), int[2](0, 0)), int[3][2](int[2](0, 0), int[2](0, 0), int[2](0, 0)));
  dst_nested._.arr = src_nested;
}

