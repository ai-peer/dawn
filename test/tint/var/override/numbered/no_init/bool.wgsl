// flags: --backend-overrides WGSL_SPEC_CONSTANT_1234=0 --overrides 1234=0
@id(1234) override o : bool;

@compute @workgroup_size(1)
fn main() {
    if o {
        _ = o;
    }
}
