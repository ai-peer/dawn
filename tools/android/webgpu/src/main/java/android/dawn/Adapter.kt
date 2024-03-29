package android.dawn

class Adapter(val handle: Long) {

    external fun createDevice(descriptor: DeviceDescriptor?): Device
    val instance: Instance
        external get

    external fun hasFeature(feature: Int): Boolean
    external fun requestDevice(descriptor: DeviceDescriptor?, callback: RequestDeviceCallback): Unit
}
