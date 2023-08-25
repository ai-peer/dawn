RWByteAddressBuffer prevent_dce : register(u0, space2);

void dpdyFine_d0a648() {
  float4 arg_0 = float4(1.0f, 1.0f, 1.0f, 1.0f);
  float4 res = ddy_fine(arg_0);
  prevent_dce.Store4(0u, asuint(res));
}

void fragment_main() {
  dpdyFine_d0a648();
  return;
}
