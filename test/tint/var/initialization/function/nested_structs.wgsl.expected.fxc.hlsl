struct S1 {
  int i;
};
struct S2 {
  S1 s1;
};
struct S3 {
  S2 s2;
};

void f(S3 s3) {
}

[numthreads(1, 1, 1)]
void main() {
  const S3 tint_symbol = {{{42}}};
  f(tint_symbol);
  return;
}
