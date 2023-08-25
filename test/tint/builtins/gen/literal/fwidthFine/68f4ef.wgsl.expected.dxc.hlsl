RWByteAddressBuffer prevent_dce : register(u0, space2);

void fwidthFine_68f4ef() {
  float4 res = fwidth(float4(1.0f, 1.0f, 1.0f, 1.0f));
  prevent_dce.Store4(0u, asuint(res));
}

void fragment_main() {
  fwidthFine_68f4ef();
  return;
}
