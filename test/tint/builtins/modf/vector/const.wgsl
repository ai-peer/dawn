@compute @workgroup_size(1)
fn main() {
    let res = modf(vec2(1.23, 3.45));
    let fract : vec2<f32> = res.fract;
    let whole : vec2<f32> = res.whole;
}
