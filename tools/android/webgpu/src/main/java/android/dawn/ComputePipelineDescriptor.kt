package android.dawn

open class ComputePipelineDescriptor(
    var label: String? = null,
    var layout: PipelineLayout? = null,
    var compute: ProgrammableStageDescriptor = ProgrammableStageDescriptor()

) {
}
