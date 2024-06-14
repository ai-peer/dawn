package net.android.dawn

import android.dawn.*
import android.dawn.helper.Util
import android.dawn.helper.createImageFromGpuTexture
import android.dawn.helper.streamToString
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.os.Environment
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import junit.framework.TestCase.assertEquals
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.runTest
import org.junit.Test
import org.junit.runner.RunWith
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileOutputStream

@RunWith(AndroidJUnit4::class)
class ImageTest {
    @Test
    fun imageCompareGreen() {
        triangleTest(Color(0.2, 0.9, 0.1, 1.0), "green.png")
    }

    @Test
    fun imageCompareRed() {
        triangleTest(Color(0.9, 0.1, 0.2, 1.0), "red.png")
    }

    private fun triangleTest(color: Color, imageName: String) {
        runTest {
            Util  // Hack to force library initialization.
            val instrumentation = InstrumentationRegistry.getInstrumentation()
            val appContext = instrumentation.targetContext

            val instance = createInstance()

            val eventProcessor = launch {
                while (true) {
                    delay(200)
                    instance.processEvents()
                }
            }

            val (_, adapter, _) = instance.requestAdapter()

            val (_, device, _) = adapter!!.requestDevice(DeviceDescriptor())

            device!!.setUncapturedErrorCallback { type, message ->
                throw Exception(message)
            }

            val shaderModule = device.createShaderModule(
                ShaderModuleDescriptor(
                    shaderModuleWGSLDescriptor = ShaderModuleWGSLDescriptor(
                        code = streamToString(appContext.assets.open("triangle/shader.wgsl"))
                    )
                )
            )

            val offScreenTexture = device.createTexture(
                TextureDescriptor(
                    size = Extent3D(256, 256),
                    format = TextureFormat.RGBA8Unorm,
                    usage = TextureUsage.CopySrc or TextureUsage.RenderAttachment
                )
            )

            with(device.queue) {
                submit(device.createCommandEncoder().use {
                    with(
                        it.beginRenderPass(
                            RenderPassDescriptor(
                                colorAttachments = arrayOf(
                                    RenderPassColorAttachment(
                                        loadOp = LoadOp.Clear,
                                        storeOp = StoreOp.Store,
                                        clearValue = color,
                                        view = offScreenTexture.createView()
                                    )
                                )
                            )
                        )
                    ) {
                        setPipeline(
                            device.createRenderPipeline(
                                RenderPipelineDescriptor(
                                    vertex = VertexState(module = shaderModule),
                                    primitive = PrimitiveState(
                                        topology = PrimitiveTopology.TriangleList
                                    ),
                                    fragment = FragmentState(
                                        module = shaderModule,
                                        targets = arrayOf(
                                            ColorTargetState(
                                                format = TextureFormat.RGBA8Unorm
                                            )
                                        )
                                    )
                                )
                            )
                        )
                        draw(3)
                        end()
                    }

                    arrayOf(it.finish())
                })
            }

            val bitmap = createImageFromGpuTexture(device, offScreenTexture)

            if (false) {
                writeReferenceImage(bitmap)
            }

            val testAssets = instrumentation.context.assets
            val matched = testAssets.list("compare")!!.filter {
                imageSimilarity(
                    bitmap,
                    BitmapFactory.decodeStream(testAssets.open("compare/$it"))
                ) > 0.99
            }

            assertEquals(listOf(imageName), matched)

            device.close()
            device.destroy()
            adapter.close()

            eventProcessor.cancel()
            eventProcessor.join()

            instance.close()
        }
    }

    private fun writeReferenceImage(bitmap: Bitmap) {
        val path = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
        val file = File("${path}${File.separator}${"reference.png"}")
        BufferedOutputStream(FileOutputStream(file)).use {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, it)
            it.close()
        }
    }
}