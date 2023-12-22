SKIP: FAILED

RWByteAddressBuffer prevent_dce : register(u0, space2);

void pack4xU8_b70b53() {
  uint4 arg_0 = (1u).xxxx;
  uint res = uint(pack_u8(arg_0));
  prevent_dce.Store(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  pack4xU8_b70b53();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  pack4xU8_b70b53();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  pack4xU8_b70b53();
  return;
}
FXC validation failure:
D:\workspace\dawn\Shader@0x0000029BAF474CA0(5,19-32): error X3004: undeclared identifier 'pack_u8'

