package android.dawn

class DawnExperimentalSubgroupLimits(
    var minSubgroupSize: Int = Constants.LIMIT_U32_UNDEFINED,
    var maxSubgroupSize: Int = Constants.LIMIT_U32_UNDEFINED
) : SupportedLimits() {
}
