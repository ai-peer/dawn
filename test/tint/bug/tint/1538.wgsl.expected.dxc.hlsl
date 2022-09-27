bug/tint/1538.wgsl:18:2 warning: use of deprecated language feature: remove stage and use @compute
@stage(compute)
 ^^^^^

RWByteAddressBuffer buf : register(u1, space0);

int g() {
  return 0;
}

int f() {
  [loop] while (true) {
    g();
    break;
  }
  const int o = g();
  return 0;
}

[numthreads(1, 1, 1)]
void main() {
  [loop] while (true) {
    if ((buf.Load(0u) == 0u)) {
      break;
    }
    int s = f();
    buf.Store(0u, asuint(0u));
  }
  return;
}
