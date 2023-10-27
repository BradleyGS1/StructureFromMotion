#include <jni.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <fftw3.h>

std::vector<float> pad(std::vector<float> input_array, int m, int n, int new_m, int new_n) {
	std::vector<float> padded_array(new_m * new_n, 0.0f);

	// The image matrix should be contained within the top left corner
    for (int i = 0, j = 0; i < new_m * new_n; i++) {
		if ((i % new_n >= n) || (i >= m*new_n)) {
			padded_array[i] = 0.0f;
		} else {
			padded_array[i] = input_array[j];
			j++;
		}
	}
	return padded_array;
}

std::vector<float> conv(std::vector<float> image, int m, int n, std::vector<float> kernel, int k) {
	// Pad the image and kernel matrices
	std::vector<float> image_padded = pad(image, m, n, m+1, n+1);
	std::vector<float> kernel_padded = pad(kernel, k, k, m+1, n+1);

	// Perform FFT on the padded image and kernel
	fftw_complex *image_input = new fftw_complex[(m + 1) * (n + 1)];
    fftw_complex *image_fft = new fftw_complex[(m + 1) * (n + 1)];
	fftw_complex *kernel_input = new fftw_complex[(m + 1) * (n + 1)];
    fftw_complex *kernel_fft = new fftw_complex[(m + 1) * (n + 1)];

    for (int i = 0; i < (m + 1) * (n + 1); i++) {
		image_input[i][0] = image_padded[i];
		image_input[i][1] = 0.0f;
		kernel_input[i][0] = kernel_padded[i];
		kernel_input[i][1] = 0.0f;
    }

	fftw_plan plan1 = fftw_plan_dft_2d(m+1, n+1, image_input, image_fft, FFTW_FORWARD, FFTW_ESTIMATE);
	fftw_plan plan2 = fftw_plan_dft_2d(m+1, n+1, kernel_input, kernel_fft, FFTW_FORWARD, FFTW_ESTIMATE);

	fftw_execute(plan1);
	fftw_execute(plan2);

    // Point-wise complex multiplication of fourier transformed matrices
	fftw_complex *conv_fft = new fftw_complex[(m + 1) * (n + 1)];
    fftw_complex *conv_padded = new fftw_complex[(m + 1) * (n + 1)];

	for (int i = 0; i < (m+1) * (n+1); i++) {
		conv_fft[i][0] = image_fft[i][0] * kernel_fft[i][0] - image_fft[i][1] * kernel_fft[i][1];
		conv_fft[i][1] = image_fft[i][0] * kernel_fft[i][1] + image_fft[i][1] * kernel_fft[i][0];
	}

	fftw_plan plan3 = fftw_plan_dft_2d(m+1, n+1, conv_fft, conv_padded, FFTW_BACKWARD, FFTW_ESTIMATE);

	// Perform IFFT on the fourier transformed convolution (un-normalised)
	fftw_execute(plan3);

	std::vector<float> conv_output(m * n, 0.0f);

	// Normalise and de-pad result
	for (int i = 0, j = 0; i < (m+1) * (n+1); i++) {
		if (((i % (n+1)) != 0) && (i > n+1)) {
			conv_output[j] = conv_padded[i][0] / ((m+1) * (n+1));
			j++;
		}
	}

	fftw_free(image_input);
	fftw_free(image_fft);
	fftw_free(kernel_input);
	fftw_free(kernel_fft);
	fftw_free(conv_fft);
	fftw_free(conv_padded);

	fftw_destroy_plan(plan1);
	fftw_destroy_plan(plan2);
	fftw_destroy_plan(plan3);

	return conv_output;
}

std::vector<float> compute_eigenvalues(std::vector<float> sobel_x, std::vector<float> sobel_y, int m, int n) {
	
	std::vector<float> eigenvalues(m * n, 0.0f);
    for (int i = 0; i < m * n; i++) {
        int s = int(i / n);
        int t = i % n;
        std::vector<float> structure_matrix(4, 0.0f);
        for (int u = s-1; u < s+2; u++) {
            for (int v = t-1; v < t+2; v++) {
				if (0 <= u && u < m && 0 <= v && v < n) {
					int idx = u * n + v;
					structure_matrix[0] += sobel_x[idx] * sobel_x[idx];
					structure_matrix[1] += sobel_x[idx] * sobel_y[idx];
					structure_matrix[3] += sobel_y[idx] * sobel_y[idx];
				}
            }
        }

        float determinant = structure_matrix[0] * structure_matrix[3] - structure_matrix[1] * structure_matrix[1];
        float trace = structure_matrix[0] + structure_matrix[3];

        eigenvalues[i] = determinant / trace;
    }

	return eigenvalues;
}

std::vector<int> sort_indices(const std::vector<float> &vec) {
    std::vector<int> indices(vec.size());
    for (int i = 0; i < indices.size(); i++) {
        indices[i] = i;
    }

    std::sort(indices.begin(), indices.end(), [&vec](int i, int j) {
        return vec[i] > vec[j];
    });

    return indices;
}

std::vector<int> harris(std::vector<float> image, int m, int n, float quantile) {

	// Create the finite difference kernels in the x and y direction
	std::vector<float> kernel_x {1.0f, 0.0f, -1.0f, 2.0f, 0.0f, -2.0f, 1.0f, 0.0f, -1.0f};
	std::vector<float> kernel_y {1.0f, 2.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, -2.0f, -1.0f};

	// Perform a 2d convolution on the image using the two kernels
	std::vector<float> sobel_x = conv(image, m, n, kernel_x, 3);
	std::vector<float> sobel_y = conv(image, m, n, kernel_y, 3);

	// Compute the array of eigenvalues
	std::vector<float> eigenvalues = compute_eigenvalues(sobel_x, sobel_y, m, n);

	// Get the indices of the sorted eigenvalues in descending order
	std::vector<int> sorted_eigenvalues = sort_indices(eigenvalues);

	// Get the upper quantile of indices (these are our corner indices)
	int num_corners = int(m * n * quantile);
	std::vector<int> corners(2 * num_corners);

	int corner_count = 0;
	int block_size = 4;
	for (int i = 0; i < sorted_eigenvalues.size(); i++) {
		int idx = sorted_eigenvalues[i];
		int u = int(idx / n);
		int v = idx % n;

		// Check if pixel is only one selected in this block
		bool select_pixel = true;
		for (int j = 0; j < corner_count; j++) {
			int selected_u = corners[2 * j];
			int selected_v = corners[2 * j + 1];

			// Check if the pixel is within the block surrounding the selected pixel
			if (abs(selected_u - u) < block_size && abs(selected_v - v) < block_size) {
				select_pixel = false;
				break;
			}
		}

		// Store pixel in corners array
		if (select_pixel) {
			corners[2*corner_count] = u;
			corners[2*corner_count+1] = v;
			corner_count++;

			// Early stop if we have enough elements in the corners array
			if (corner_count >= num_corners) {
				break;
			}
		}
	}

	return corners;
}

extern "C" {
    JNIEXPORT jintArray JNICALL Java_com_example_cornerdetector_MainActivity_findCorners(JNIEnv *env, jobject thiz, jfloatArray image, jint m, jint n, jfloat quantile) {
        jfloat *imageArray = env -> GetFloatArrayElements(image, nullptr);
        jsize imageArrayLength = env -> GetArrayLength(image);

        // Convert the input imageArray to a std::vector<float>
        std::vector<float> imageVector(imageArray, imageArray + imageArrayLength);

        // Get result from native function
        std::vector<int> corners = harris(imageVector, m, n, quantile);

        // Convert output to java int array
        jintArray result = env -> NewIntArray(corners.size());
        env -> SetIntArrayRegion(result, 0, corners.size(), corners.data());

		// Release imageArray elements
        env -> ReleaseFloatArrayElements(image, imageArray, 0);
        return result;
    }
}