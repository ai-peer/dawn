#version 310 es

ivec3 tint_div(ivec3 lhs, ivec3 rhs) {
  ivec3 rhs_or_one = mix(rhs, ivec3(1), bvec3(uvec3(equal(rhs, ivec3(0))) | uvec3(bvec3(uvec3(equal(lhs, ivec3(-2147483648))) & uvec3(equal(rhs, ivec3(-1)))))));
  return (lhs / rhs_or_one);
}

void f() {
  ivec3 a = ivec3(1, 2, 3);
  ivec3 b = ivec3(0, 5, 0);
  ivec3 r = tint_div(a, b);
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
