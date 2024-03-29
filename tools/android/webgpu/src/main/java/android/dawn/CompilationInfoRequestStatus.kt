package android.dawn

object CompilationInfoRequestStatus {
    const val Success = 0x00000000;
    const val InstanceDropped = 0x00000001;
    const val Error = 0x00000002;
    const val DeviceLost = 0x00000003;
    const val Unknown = 0x00000004;
}
