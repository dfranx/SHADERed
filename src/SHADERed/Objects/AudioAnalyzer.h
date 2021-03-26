#pragma once
#include <complex>
#include <valarray>
#include <vector>

namespace ed {
	class AudioAnalyzer {
	public: /*   CONFIG   */
		static const int SampleCount = 512;
		static const int BufferOutSize = SampleCount / 2;
		static const float Smooth[];
		static const float Gravity;
		static const int HighFrequency = 18000;
		static const int LowFrequency = 20;
		static const float LogScale;

	public:
		AudioAnalyzer();
		~AudioAnalyzer();

		double* FFT(const short* samples);

	private:
		void m_fftAlgorithm(std::valarray<std::complex<double>>& x);
		void m_seperateFreqBands(std::complex<double>* in, int n, int* lcf, int* hcf, float* k, double sensitivity, int in_samples);

		int m_isSetup;
		void m_setup(int rate);

		float m_smoothing[BufferOutSize];

		int m_fall[BufferOutSize];
		float m_fpeak[BufferOutSize], m_flast[BufferOutSize], m_fmem[BufferOutSize];

		float m_fc[BufferOutSize];
		int m_lcf[BufferOutSize], m_hcf[BufferOutSize];

		double m_fftOut[SampleCount];
		double m_sensitivity;
	};
}