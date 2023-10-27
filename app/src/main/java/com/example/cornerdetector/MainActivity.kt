package com.example.cornerdetector

import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis.STRATEGY_BLOCK_PRODUCER
import androidx.camera.core.ImageProxy
import androidx.camera.view.LifecycleCameraController
import androidx.camera.view.PreviewView
import androidx.core.content.ContextCompat
import androidx.core.graphics.createBitmap
import com.example.cornerdetector.databinding.ActivityMainBinding

class MainActivity : ComponentActivity() {
    private lateinit var viewBinding: ActivityMainBinding
    private lateinit var cameraController: LifecycleCameraController
    private external fun findCorners(imageArray: FloatArray, m: Int, n: Int, quantile: Float): IntArray

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        System.loadLibrary("CornerDetector")


        viewBinding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(viewBinding.root)

        if (!hasPermissions(baseContext)) {
            // Request camera-related permissions
            activityResultLauncher.launch(REQUIRED_PERMISSIONS)
        } else {
            // Start the camera preview and also initialises overlay
            Log.d(TAG, "camera view on")
            cameraController = startCamera()

            // Toggle the analysis overlay when button clicked
            var toggleAnalysis = true
            viewBinding.imageAnalyseButton.setOnClickListener {
                Log.d(TAG, "click")
                Log.d(TAG, "${cameraController.cameraInfo}")

                if (toggleAnalysis) {
                    viewBinding.overlayView.visibility = View.VISIBLE
                    cameraController.setImageAnalysisAnalyzer(
                        ContextCompat.getMainExecutor(this)
                    ) { imageProxy ->
                        analyze(imageProxy)
                    }
                } else {
                    cameraController.clearImageAnalysisAnalyzer()
                    viewBinding.overlayView.visibility = View.GONE
                }

                toggleAnalysis = !toggleAnalysis
            }
        }
    }

    companion object {
        private const val TAG = "CornerDetector"
        private const val FILENAME_FORMAT = "yyyy-MM-dd-HH-mm-ss"
        private val REQUIRED_PERMISSIONS =
            mutableListOf(
                android.Manifest.permission.CAMERA
            ).toTypedArray()
        fun hasPermissions(context: Context) = REQUIRED_PERMISSIONS.all {
            ContextCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED
        }
    }


    private fun startCamera(): LifecycleCameraController {
        val cameraView: PreviewView = viewBinding.cameraView
        cameraController = LifecycleCameraController(baseContext)
        cameraController.bindToLifecycle(this)
        cameraController.cameraSelector = CameraSelector.DEFAULT_BACK_CAMERA
        cameraView.controller = cameraController

        return cameraController
    }

    private fun analyze(imageProxy: ImageProxy) {

        // Perform image analysis here
        // Planes in YUV format
        val imageArray = imageProxy.planes
        val yArray = imageArray[0] // Y plane

        val yData = ByteArray(yArray.buffer.remaining())
        yArray.buffer.get(yData)

        // Downscales the image captured for analysis
        val downscale_factor = 2
        val m = imageProxy.height.floorDiv(downscale_factor)
        val n = imageProxy.width.floorDiv(downscale_factor)

        val floatArray = FloatArray(m * n)
        var j = 0
        for (i in 0 until imageProxy.height * imageProxy.width) {
            if (i.mod(downscale_factor) == 0 && i.floorDiv(imageProxy.width).mod(downscale_factor) == 0) {
                floatArray[j] = (yData[i].toFloat() + 127) / 255
                j += 1
            }
        }

        // Compute the corner locations using native c++ function
        val quantile = 0.005f
        val corners = findCorners(floatArray, m, n, quantile)

        // Get the bitmap overlay
        val bitmap = createBitmap(m, n, Bitmap.Config.ARGB_8888)
        for (i in 0 until m * n) {
            val x = i.floorDiv(n)
            val y = i.mod(n)
            bitmap.setPixel(x, y, Color.argb(60, 180, 180, 180))
        }
        for (i in 0 until corners.size / 2) {
            val x = corners[2*i]
            val y = corners[2*i+1]
            bitmap.setPixel(x, y, Color.GREEN)
        }

        val overlayView = viewBinding.overlayView
        overlayView.setImageBitmap(bitmap)

        Log.d(TAG, "running analysis")
        imageProxy.close()
    }

    private val activityResultLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
                permissions ->
            var permissionGranted = true
            permissions.entries.forEach {
                if (it.key in REQUIRED_PERMISSIONS && it.value == false)
                    permissionGranted = false
            }
            if (!permissionGranted) {
                // Put code for what happens with no permissions here
                Toast.makeText(this, "Permission request denied", Toast.LENGTH_LONG).show()
            } else {
                startCamera()
            }
        }
}

/*
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
        binding.sampleText.text = stringFromJNI()
    }

    /**
     * A native method that is implemented by the 'cornerdetector' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    companion object {
        // Used to load the 'cornerdetector' library on application startup.
        init {
            System.loadLibrary("cornerdetector")
        }
    }
}
*/