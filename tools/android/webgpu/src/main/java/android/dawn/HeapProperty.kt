package android.dawn

object HeapProperty {
    const val Undefined = 0x00000000;
    const val DeviceLocal = 0x00000001;
    const val HostVisible = 0x00000002;
    const val HostCoherent = 0x00000004;
    const val HostUncached = 0x00000008;
    const val HostCached = 0x00000010;
}
