struct S1 {
    i : i32,
}
struct S2 {
    s1 : S1,
}
struct S3 {
    s2 : S2,
}

fn f(s3 : S3) {}

@compute @workgroup_size(1)
fn main() {
    const C = 42;
    f(S3(S2(S1(C))));
}
