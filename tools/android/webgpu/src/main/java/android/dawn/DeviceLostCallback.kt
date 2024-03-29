package android.dawn

fun interface DeviceLostCallback {
    fun callback(
        reason: Int,
        message: String
    );
}
