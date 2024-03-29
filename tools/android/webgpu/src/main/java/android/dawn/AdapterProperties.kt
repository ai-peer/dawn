package android.dawn

open class AdapterProperties(
    var vendorID: Int = 0,
    var vendorName: String? = null,
    var architecture: String? = null,
    var deviceID: Int = 0,
    var name: String? = null,
    var driverDescription: String? = null,
    var adapterType: Int = 0,
    var backendType: Int = BackendType.Undefined,
    var compatibilityMode: Boolean = false
) {
}
