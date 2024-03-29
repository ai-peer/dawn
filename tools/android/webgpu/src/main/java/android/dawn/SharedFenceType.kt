package android.dawn

object SharedFenceType {
    const val Undefined = 0x00000000;
    const val VkSemaphoreOpaqueFD = 0x00000001;
    const val VkSemaphoreSyncFD = 0x00000002;
    const val VkSemaphoreZirconHandle = 0x00000003;
    const val DXGISharedHandle = 0x00000004;
    const val MTLSharedEvent = 0x00000005;
}
