package android.dawn

class RequestAdapterOptions(
    var compatibleSurface: Surface? = null,
    var powerPreference: Int = PowerPreference.Undefined,
    var backendType: Int = BackendType.Undefined,
    var forceFallbackAdapter: Boolean = false,
    var compatibilityMode: Boolean = false
) {
}
