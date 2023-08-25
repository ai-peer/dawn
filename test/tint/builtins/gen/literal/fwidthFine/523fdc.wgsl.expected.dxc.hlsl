RWByteAddressBuffer prevent_dce : register(u0, space2);

void fwidthFine_523fdc() {
  float3 res = fwidth(float3(1.0f, 1.0f, 1.0f));
  prevent_dce.Store3(0u, asuint(res));
}

void fragment_main() {
  fwidthFine_523fdc();
  return;
}
