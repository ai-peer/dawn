package android.dawn

open class DeviceDescriptor(
    var label: String? = null,
    var requiredFeatures: IntArray? = null,
    var requiredLimits: RequiredLimits? = null,
    var defaultQueue: QueueDescriptor = QueueDescriptor(),
    var deviceLostCallback: DeviceLostCallback? = null,
    var deviceLostUserdata: ByteArray? = null
) {
}
