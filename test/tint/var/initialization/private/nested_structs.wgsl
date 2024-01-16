struct S1 {
    i : i32,
}
struct S2 {
    s1 : S1,
}
struct S3 {
    s2 : S2,
}

var<private> P = S3(S2(S1(42)));

@compute @workgroup_size(1)
fn main() {
    _ = P.s2.s1.i;
}
