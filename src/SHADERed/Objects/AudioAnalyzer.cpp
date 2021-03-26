#include <SHADERed/Objects/AudioAnalyzer.h>

#define _USE_MATH_DEFINES
#include <math.h>

const float ed::AudioAnalyzer::Smooth[] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
const float ed::AudioAnalyzer::Gravity = 0.0006f;
const float ed::AudioAnalyzer::LogScale = 1.0;

namespace ed {
	/**************************************
		Modified version of:
		https://github.com/dgranosa/liveW
	***************************************/
	AudioAnalyzer::AudioAnalyzer()
	{
		m_sensitivity = 1.0;
		m_isSetup = 0;
		m_setup(48000);
	}

	AudioAnalyzer::~AudioAnalyzer()
	{
	}
	void AudioAnalyzer::m_setup(int rate)
	{
		// set up
		double freqconst = log(HighFrequency - LowFrequency) / log(pow(BufferOutSize, LogScale));
		float x;

		// Calc freq range for each bar
		for (int i = 0; i < BufferOutSize; i++) {
			m_fc[i] = pow(powf(i, (LogScale - 1.0) * ((double)i + 1.0) / ((double)BufferOutSize) + 1.0), freqconst) + LowFrequency;
			x = m_fc[i] / (rate / 2);
			m_lcf[i] = x * (SampleCount / 2);
			if (i != 0)
				m_hcf[i - 1] = m_lcf[i] - 1 > m_lcf[i - 1] ? m_lcf[i] - 1 : m_lcf[i - 1];
		}
		m_hcf[BufferOutSize - 1] = HighFrequency * SampleCount / rate;

		// Calc smoothing
		for (int i = 0; i < BufferOutSize; i++) {
			m_smoothing[i] = pow(m_fc[i], 0.64); // TODO: Add smoothing factor to config
			m_smoothing[i] *= Smooth[(int)(i / BufferOutSize * sizeof(Smooth) / sizeof(*Smooth))];
		}

		// Clear arrays
		for (int i = 0; i < BufferOutSize; i++)
			m_fall[i] = m_fpeak[i] = m_flast[i] = m_fmem[i] = 0;
	}
	double* AudioAnalyzer::FFT(const short* samples)
	{
		// Spliting channels
		std::valarray<std::complex<double>> fftIn(SampleCount);
		for (int i = 0; i < SampleCount * 2; i += 2)
			fftIn[i / 2] = (samples[i] + samples[i + 1]) / 2; // TODO: Add stereo option

		// Run fftw
		m_fftAlgorithm(fftIn);

		// Separate fftw output
		m_seperateFreqBands(&fftIn[0], BufferOutSize, m_lcf, m_hcf, m_smoothing, m_sensitivity, BufferOutSize);

		/* Processing */
		// Waves
		for (int i = 0; i < BufferOutSize; i++) {
			m_fftOut[i] *= 0.8;
			for (int j = i - 1; j >= 0; j--) {
				if (m_fftOut[i] - (i - j) * (i - j) / 1000.0 > m_fftOut[j])
					m_fftOut[j] = m_fftOut[i] - (i - j) * (i - j) / 1000.0;
			}
			for (int j = i + 1; j < BufferOutSize; j++)
				if (m_fftOut[i] - (i - j) * (i - j) / 1000.0 > m_fftOut[j])
					m_fftOut[j] = m_fftOut[i] - (i - j) * (i - j) / 1000.0;
		}

		// Gravity
		for (int i = 0; i < BufferOutSize; i++) {
			if (m_fftOut[i] < m_flast[i]) {
				m_fftOut[i] = m_fpeak[i] - (Gravity * m_fall[i] * m_fall[i]);
				m_fall[i]++;
			} else {
				m_fpeak[i] = m_fftOut[i];
				m_fall[i] = 0;
			}

			m_flast[i] = m_fftOut[i];
		}

		// Integral
		for (int i = 0; i < BufferOutSize; i++) {
			m_fftOut[i] = (int)(m_fftOut[i] * 100);
			m_fftOut[i] += m_fmem[i] * 0.9; // TODO: Add integral to config
			m_fmem[i] = m_fftOut[i];

			int diff = 100 - m_fftOut[i];
			if (diff < 0) diff = 0;
			double div = 1 / (diff + 1);
			m_fmem[i] *= 1 - div / 20;
			m_fftOut[i] /= 100.0;
		}

		// Auto sensitivity
		for (int i = 0; i < BufferOutSize; i++) {
			if (m_fftOut[i] > 0.95) {
				m_sensitivity *= 0.985;
				break;
			}
			if (i == BufferOutSize - 1 && m_sensitivity < 1.0) m_sensitivity *= 1.002;
		}
		if (m_sensitivity < 0.0001) m_sensitivity = 0.0001;

		return &m_fftOut[0];
	}
	void AudioAnalyzer::m_fftAlgorithm(std::valarray<std::complex<double>>& input)
	{
		const int len = input.size();
		if (len <= 1) return;

		std::valarray<std::complex<double>> even = input[std::slice(0, len / 2, 2)];
		std::valarray<std::complex<double>> odd = input[std::slice(1, len / 2, 2)];

		m_fftAlgorithm(even);
		m_fftAlgorithm(odd);

		for (int i = 0; i < len / 2; i++) {
			std::complex<double> temp = std::polar(1.0, (double)-2 * M_PI * i / len) * odd[i];
			input[i] = even[i] + temp;
			input[i + len / 2] = even[i] - temp;
		}
	}
	void AudioAnalyzer::m_seperateFreqBands(std::complex<double>* in, int n, int* lcf, int* hcf, float* k, double sensitivity, int in_samples)
	{
		double* peak = new double[n];
		double* y = new double[in_samples];
		double temp;

		for (int i = 0; i < n; i++) {
			peak[i] = 0;

			for (int j = lcf[i]; j <= hcf[i]; j++) {
				y[j] = sqrt(in[j].real() * in[j].real() + in[j].imag() * in[j].imag());
				peak[i] += y[j];
			}

			peak[i] = peak[i] / (hcf[i] - lcf[i] + 1);
			temp = peak[i] * sensitivity * k[i] / 1000000;
			m_fftOut[i] = temp / 100.0;
		}

		delete[] peak;
		delete[] y;
	}
}