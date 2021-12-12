#pragma once
#include <misc/miniaudio.h>
#include <string>
#include <string.h>

namespace ed {
	namespace eng {
		class AudioPlayer {
		public:
			AudioPlayer();
			~AudioPlayer();

			bool LoadFromFile(const std::string& filename);
			void Start();
			void Stop();

			// Read last 512 samples (x2 == 1024, because stereo)
			inline void GetSamples(short* out)
			{
				memcpy(out, m_audioBuffer, 1024 * sizeof(short));
			}

			void SeekFrame(unsigned int frameIndex);
			uint64_t GetCurrentFrame();
			uint64_t GetTotalFrameCount();

			float GetVolume();
			void SetVolume(float volume);

			inline short* GetAudioBuffer() { return m_audioBuffer; }
			inline ma_decoder* GetDecoder() { return &m_decoder; }
			inline ma_pcm_rb* GetRingBuffer() { return &m_rb; }

		private:
			void m_clean();

			bool m_loaded;
			bool m_paused;

			short m_audioBuffer[1024];

			ma_decoder_config m_decoderConfig;
			ma_decoder m_decoder;
			ma_device_config m_deviceConfig;
			ma_device m_device;
			ma_pcm_rb m_rb;
		};
	}
}