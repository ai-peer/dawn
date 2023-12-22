SKIP: FAILED

RWByteAddressBuffer prevent_dce : register(u0, space2);

void pack4xI8_bfce01() {
  int4 arg_0 = (1).xxxx;
  uint res = uint(pack_s8(arg_0));
  prevent_dce.Store(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  pack4xI8_bfce01();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  pack4xI8_bfce01();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  pack4xI8_bfce01();
  return;
}
FXC validation failure:
D:\workspace\dawn\Shader@0x000002B62EB8A5B0(5,19-32): error X3004: undeclared identifier 'pack_s8'

