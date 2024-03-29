package android.dawn

fun interface RequestDeviceCallback {
    fun callback(
        status: Int,
        device: Device?,
        message: String?
    );
}
