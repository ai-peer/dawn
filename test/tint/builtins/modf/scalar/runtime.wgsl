@compute @workgroup_size(1)
fn main() {
    let in = 1.23;
    let res = modf(1.23);
    let fract : f32 = res.fract;
    let whole : f32 = res.whole;
}
