struct S1 {
  int i;
};
struct S2 {
  S1 s1;
};
struct S3 {
  S2 s2;
};

static const S1 c_2 = {42};
static const S2 c_1 = {c_2};
static const S3 c = {c_1};
static S3 P = c;

[numthreads(1, 1, 1)]
void main() {
  return;
}
