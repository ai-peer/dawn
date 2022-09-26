RWByteAddressBuffer b : register(u0, space0);

[numthreads(1, 1, 1)]
void main() {
  uint i = 0u;
  [loop] while (true) {
    if ((i >= b.Load(0u))) {
      break;
    }
    const uint tint_symbol = i;
    const uint p_save = tint_symbol;
    if (((i % 2u) == 0u)) {
      {
        b.Store((4u + (4u * tint_symbol)), asuint((b.Load((4u + (4u * tint_symbol))) * 2u)));
        i = (i + 1u);
      }
      continue;
    }
    b.Store((4u + (4u * tint_symbol)), asuint(0u));
    {
      b.Store((4u + (4u * tint_symbol)), asuint((b.Load((4u + (4u * tint_symbol))) * 2u)));
      i = (i + 1u);
    }
  }
  return;
}
