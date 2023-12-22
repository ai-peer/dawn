SKIP: FAILED

RWByteAddressBuffer prevent_dce : register(u0, space2);

void unpack4xI8_830900() {
  uint arg_0 = 1u;
  int4 res = unpack_s8s32(int8_t4_packed(arg_0));
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  unpack4xI8_830900();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  unpack4xI8_830900();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  unpack4xI8_830900();
  return;
}
DXC validation failure:
error: validation errors
shader.hlsl:5:14: error: Opcode Unpack4x8 not valid in shader model vs_6_0.
note: at '%2 = call %dx.types.fouri32 @dx.op.unpack4x8.i32(i32 219, i8 1, i32 1)' in block '#0' of function 'vertex_main'.
Validation failed.



error: validation errors
shader.hlsl:5:14: error: Opcode Unpack4x8 not valid in shader model ps_6_0.
note: at '%2 = call %dx.types.fouri32 @dx.op.unpack4x8.i32(i32 219, i8 1, i32 1)' in block '#0' of function 'fragment_main'.
Validation failed.



error: validation errors
shader.hlsl:5:14: error: Opcode Unpack4x8 not valid in shader model cs_6_0.
note: at '%2 = call %dx.types.fouri32 @dx.op.unpack4x8.i32(i32 219, i8 1, i32 1)' in block '#0' of function 'compute_main'.
Validation failed.



