RWByteAddressBuffer b : register(u0, space0);

[numthreads(1, 1, 1)]
void main() {
  uint i = 0u;
  [loop] while (true) {
    if ((i >= b.Load(0u))) {
      break;
    }
    const uint ptr_index_save = i;
    const uint p_save = ptr_index_save;
    if (((i % 2u) == 0u)) {
      {
        b.Store((4u + (4u * ptr_index_save)), asuint((b.Load((4u + (4u * ptr_index_save))) * 2u)));
        i = (i + 1u);
      }
      continue;
    }
    b.Store((4u + (4u * ptr_index_save)), asuint(0u));
    {
      b.Store((4u + (4u * ptr_index_save)), asuint((b.Load((4u + (4u * ptr_index_save))) * 2u)));
      i = (i + 1u);
    }
  }
  return;
}
