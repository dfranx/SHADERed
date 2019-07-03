#pragma once
#include <vector>
#include <valarray>
#include <complex>

#include <MoonLight/Audio/AudioFile.h>

namespace ed
{
	class AudioAnalyzer
	{
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

		double* FFT(ml::AudioFile& file, int curSample);

	private:
		void m_fftAlgorithm(std::valarray<std::complex<double>>& x);
		void m_seperateFreqBands(std::complex<double>* in, int n, int* lcf, int* hcf, float* k, double sensitivity, int in_samples);

		double m_fftOut[SampleCount];
		double m_sensitivity;
	};
}