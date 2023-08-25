RWByteAddressBuffer prevent_dce : register(u0, space2);

void fwidth_d2ab9a() {
  float4 arg_0 = float4(1.0f, 1.0f, 1.0f, 1.0f);
  float4 res = fwidth(arg_0);
  prevent_dce.Store4(0u, asuint(res));
}

void fragment_main() {
  fwidth_d2ab9a();
  return;
}
