#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fftw3.h>
#include <GL/glfw.h>
#include <rtl-sdr.h>
#include <unistd.h>

const int FFT_SIZE = 2048;

const int OUT_BLOCK_SIZE = 16 * 16384;

rtlsdr_dev_t *sdrDevice;

void GLFWCALL windowSizeCallback(int width, int height) {
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, width, 0, height);
	glMatrixMode(GL_MODELVIEW);
}

void printHelp(const char *name) {
	printf("usage: %s [-s sample_rate] frequency\n", name);
}

void initGui() {
	glfwInit();

	glfwOpenWindowHint(GLFW_FSAA_SAMPLES, 4);

	if (!glfwOpenWindow(800, 600, 0, 0, 0, 0, 0, 0, GLFW_WINDOW))
		exit(1);

	glfwSetWindowTitle("radio");

	glClearColor(0.0, 0.0, 0.0, 1.0);

	glfwSetWindowSizeCallback(windowSizeCallback);
}

void initSdr(int sampleRate, int centerFrequency) {
	int result;

	int device_count = rtlsdr_get_device_count();
	if (!device_count) {
		fprintf(stderr, "No supported devices found.\n");
		exit(1);
	}

	result = rtlsdr_open(&sdrDevice, 0);
	if (result < 0) {
		fprintf(stderr, "Failed to open rtlsdr device\n");
		exit(1);
	}

	result = rtlsdr_set_sample_rate(sdrDevice, sampleRate);
	if (result < 0) {
		fprintf(stderr, "Failed to set sample rate for rtlsdr device\n");
		exit(1);
	}

	result = rtlsdr_set_center_freq(sdrDevice, centerFrequency);

	if (result < 0) {
		fprintf(stderr, "Failed to set frequency\n");
		exit(1);
	}

	result = rtlsdr_set_tuner_gain_mode(sdrDevice, 0);
	if (result < 0) {
		fprintf(stderr, "Failed to enable automatic gain.\n");
		exit(1);
	}

	rtlsdr_reset_buffer(sdrDevice);
}

int main(int argc, char *argv[]) {
	float frequency = 88.0;
	float sampleRate = 2.4;

	// getopt should not print any errors
	opterr = 0;

	int c;
	while ((c = getopt(argc, argv, "s:")) != -1) {
		if (c == 's')
			sampleRate = atof(optarg);
		else {
			printHelp(argv[0]);
			exit(1);
		}
	}

	// Check if a frequency is not specified
	if (optind == argc) {
		printHelp(argv[0]);
		exit(1);
	}

	frequency = atof(argv[optind]);

	initGui();
	initSdr(sampleRate * 1e6, frequency * 1e6);

	unsigned char *radioBuffer = malloc(OUT_BLOCK_SIZE * 4);

	// Set up FFT resources
	fftw_complex *fftIn, *fftOut;
	fftIn = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    fftOut = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    fftw_plan fftPlan = fftw_plan_dft_1d(FFT_SIZE, fftIn, fftOut, FFTW_FORWARD, FFTW_ESTIMATE);

    float *averaged = malloc(FFT_SIZE * sizeof(float));
    memset(averaged, 0, FFT_SIZE * sizeof(float));

	int running = 1;

	// Main loop
	while (running) {
		int readSamples;
		int result = rtlsdr_read_sync(sdrDevice, radioBuffer, OUT_BLOCK_SIZE, &readSamples);

		if (result < 0) {
			fprintf(stderr, "WARNING: sync read failed.\n");
			break;
		}

		for (int n = 0; n < (OUT_BLOCK_SIZE / 2) / FFT_SIZE; ++n) {
			for(int i = 0; i < FFT_SIZE; ++i) {
				// Normalize the IQ data and put it as the complex FFT input
				fftIn[i][0] = ((float)radioBuffer[i * 2 + n * FFT_SIZE] - 127.0f) * 0.008f;
				fftIn[i][1] = ((float)radioBuffer[i * 2 + n * FFT_SIZE + 1] - 127.0f) * 0.008f;
			}

			fftw_execute(fftPlan);

			for (int i = 0; i < FFT_SIZE; ++i) {
				// Calculate the logarithmic magnitude of the complex FFT output
				float magnitude = 0.05 * log(fftOut[i][0] * fftOut[i][0] + fftOut[i][1] * fftOut[i][1] + 1.0);

				// Average the signal
				averaged[i] -= 0.01f * (averaged[i] - magnitude);
			}
		}

		glClear(GL_COLOR_BUFFER_BIT);

		glLoadIdentity();

		int windowWidth, windowHeight;
		glfwGetWindowSize(&windowWidth, &windowHeight);

		glColor3f(0.3f, 0.3f, 0.0f);

		// Draw horizontal grid lines
		glPushMatrix();
		glScalef(windowWidth, windowHeight, 1.0);
		glBegin(GL_LINES);
		for (float i = 0.0; i < 1.0; i += 0.05) {
			glVertex2f(0.0, i);
			glVertex2f(1.0, i);
		}
		glEnd();
		glPopMatrix();

		// Draw vertical grid with a spacing of 100 KHz
		glPushMatrix();
		glScalef(windowWidth / sampleRate, windowHeight, 1.0);
		glTranslatef(sampleRate * 0.5, 0.0, 0.0);
		glBegin(GL_LINES);
		for (float i = 0.0; i < sampleRate * 0.5; i += 0.1) {
			glVertex2f(i, 0);
			glVertex2f(i, 1.0);

			glVertex2f(-i, 0);
			glVertex2f(-i, 1.0);
		}
		glEnd();
		glPopMatrix();

		// Draw signal
		glColor3f(0.8, 0.8, 0.0);
		glPushMatrix();
		glScalef((float)windowWidth / FFT_SIZE, windowHeight, 1.0f);
		glBegin(GL_LINES);
		float last = 0.0f;
		for (int i = 1; i < FFT_SIZE; ++i) {
			float current = averaged[(i + FFT_SIZE / 2) % FFT_SIZE];
			glVertex2f(i - 1, last);
			glVertex2f(i, current);
			last = current;
		}
		glEnd();
		glPopMatrix();

		glfwSwapBuffers();

		running = !glfwGetKey(GLFW_KEY_ESC) && glfwGetWindowParam(GLFW_OPENED);
	}

	free(averaged);

	fftw_destroy_plan(fftPlan);
	fftw_free(fftIn);
	fftw_free(fftOut);

	glfwTerminate();

	return 0;
}
