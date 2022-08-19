#version 310 es

struct Constants {
  uint zero;
};

layout(binding = 0) uniform Constants_1 {
  Constants _;
} constants;

struct Result {
  uint value;
};

layout(binding = 1, std430) buffer Result_1 {
  Result _;
} result;
struct TestData {
  int data[3];
};

layout(binding = 0, std430) buffer TestData_1 {
  TestData _;
} s;
int runTest() {
  return atomicOr(s._.data[(0u + uint(constants._.zero))], 0);
}

void tint_symbol() {
  int tint_symbol_1 = runTest();
  uint tint_symbol_2 = uint(tint_symbol_1);
  result._.value = tint_symbol_2;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
