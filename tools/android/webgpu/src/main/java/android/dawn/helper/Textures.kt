package android.dawn.helper

import android.dawn.*
import android.graphics.Bitmap
import java.nio.ByteBuffer

fun createGpuTextureFromImage(device: Device, bitmap: Bitmap): android.dawn.Texture {
    val size = Extent3D(width = bitmap.width, height = bitmap.height)
    return device.createTexture(
        TextureDescriptor(
            size = size,
            format = TextureFormat.RGBA8Unorm,
            usage = TextureUsage.TextureBinding or TextureUsage.CopyDst or
                    TextureUsage.RenderAttachment
        )
    ).also { texture ->
        ByteBuffer.allocateDirect(bitmap.height * bitmap.width * Int.SIZE_BYTES).let { pixels ->
            bitmap.copyPixelsToBuffer(pixels)
            device.queue.writeTexture(
                dataLayout = TextureDataLayout(

                    bytesPerRow = bitmap.width * Int.SIZE_BYTES,
                    rowsPerImage = bitmap.height
                ),
                data = pixels,
                destination = ImageCopyTexture(texture = texture),
                writeSize = size
            )
        }
    }
}

suspend fun createImageFromGpuTexture(device: Device, texture: android.dawn.Texture): Bitmap {
    val useWidth = texture.width.roundDown(64)

    val size1 = useWidth * texture.height * Int.SIZE_BYTES
    val readbackBuffer = device.createBuffer(
        BufferDescriptor(
            size = size1.toLong(),
            usage = BufferUsage.CopyDst or BufferUsage.MapRead
        )
    )
    device.queue.submit(arrayOf(device.createCommandEncoder().run {
        copyTextureToBuffer(
            source = ImageCopyTexture(texture = texture),
            destination = ImageCopyBuffer(
                layout = TextureDataLayout(
                    offset = 0,
                    bytesPerRow = useWidth * Int.SIZE_BYTES,
                    rowsPerImage = texture.height
                ), buffer = readbackBuffer
            ),
            copySize = Extent3D(width = useWidth, height = texture.height)
        )
        finish()
    }))

    readbackBuffer.mapAsync(MapMode.Read, 0, size1.toLong())

    return Bitmap.createBitmap(useWidth, texture.height, Bitmap.Config.ARGB_8888).apply {
        copyPixelsFromBuffer(readbackBuffer.getConstMappedRange(size = readbackBuffer.size))
        readbackBuffer.unmap()
    }
}
