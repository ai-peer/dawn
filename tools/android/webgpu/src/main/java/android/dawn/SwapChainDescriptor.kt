package android.dawn

class SwapChainDescriptor(
    var label: String? = null,
    var usage: Int = TextureUsage.None,
    var format: Int = TextureFormat.Undefined,
    var width: Int = 0,
    var height: Int = 0,
    var presentMode: Int = 0
) {
}
