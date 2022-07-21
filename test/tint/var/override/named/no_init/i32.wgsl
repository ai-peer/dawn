// flags: --backend-overrides WGSL_SPEC_CONSTANT_0=0 --overrides o=0
override o : i32;

@compute @workgroup_size(1)
fn main() {
    if o == 1 {
        _ = o;
    }
}
