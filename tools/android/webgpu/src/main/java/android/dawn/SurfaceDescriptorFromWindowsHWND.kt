package android.dawn

class SurfaceDescriptorFromWindowsHWND(
    var hinstance: Array<Byte> = arrayOf(),
    var hwnd: Array<Byte> = arrayOf()

) : SurfaceDescriptor() {
}
