package android.dawn

object StencilOperation {
    const val Undefined = 0x00000000;
    const val Keep = 0x00000001;
    const val Zero = 0x00000002;
    const val Replace = 0x00000003;
    const val Invert = 0x00000004;
    const val IncrementClamp = 0x00000005;
    const val DecrementClamp = 0x00000006;
    const val IncrementWrap = 0x00000007;
    const val DecrementWrap = 0x00000008;
}
