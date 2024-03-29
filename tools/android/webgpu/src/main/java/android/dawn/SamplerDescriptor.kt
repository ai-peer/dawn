package android.dawn

class SamplerDescriptor(
    var label: String? = null,
    var addressModeU: Int = AddressMode.ClampToEdge,
    var addressModeV: Int = AddressMode.ClampToEdge,
    var addressModeW: Int = AddressMode.ClampToEdge,
    var magFilter: Int = FilterMode.Nearest,
    var minFilter: Int = FilterMode.Nearest,
    var mipmapFilter: Int = MipmapFilterMode.Nearest,
    var lodMinClamp: Float = 0.0f,
    var lodMaxClamp: Float = 32.0f,
    var compare: Int = CompareFunction.Undefined,
    var maxAnisotropy: Short = 1
) {
}
