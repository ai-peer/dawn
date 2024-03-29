package android.dawn

class SurfaceDescriptorFromXlibWindow(
    var display: Array<Byte> = arrayOf(),
    var window: Long = 0
) : SurfaceDescriptor() {
}
