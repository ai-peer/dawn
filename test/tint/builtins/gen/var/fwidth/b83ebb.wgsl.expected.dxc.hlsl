RWByteAddressBuffer prevent_dce : register(u0, space2);

void fwidth_b83ebb() {
  float2 arg_0 = float2(1.0f, 1.0f);
  float2 res = fwidth(arg_0);
  prevent_dce.Store2(0u, asuint(res));
}

void fragment_main() {
  fwidth_b83ebb();
  return;
}
