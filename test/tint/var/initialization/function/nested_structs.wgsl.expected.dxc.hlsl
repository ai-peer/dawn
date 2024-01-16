struct S1 {
  int i;
};
struct S2 {
  S1 s1;
};
struct S3 {
  S2 s2;
};

[numthreads(1, 1, 1)]
void main() {
  S3 P = {{{42}}};
  return;
}
