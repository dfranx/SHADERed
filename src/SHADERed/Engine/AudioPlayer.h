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

			// read last 512 samples (x2 == 1024, because stereo)
			inline void GetSamples(short* out)
			{
				memcpy(out, m_audioBuffer, 1024 * sizeof(short));
			}
			inline short* GetAudioBuffer() { return m_audioBuffer; }

			inline ma_decoder* GetDecoder() { return &m_decoder; }
			inline ma_pcm_rb* GetRingBuffer() { return &m_rb; }

			inline void SeekFrame(unsigned int frameIndex)
			{
				ma_decoder_seek_to_pcm_frame(&m_decoder, frameIndex);
			}
			inline uint64_t GetCurrentFrame()
			{
				ma_uint64 ret = 0;
				ma_decoder_get_cursor_in_pcm_frames(&m_decoder, &ret);
				return ret;
			}
			inline uint64_t GetTotalFrameCount()
			{
				return ma_decoder_get_length_in_pcm_frames(&m_decoder);
			}
			inline float GetVolume()
			{
				float ret = 0.0f;
				ma_device_get_master_volume(&m_device, &ret);
				return ret;
			}
			inline void SetVolume(float volume)
			{
				ma_device_set_master_volume(&m_device, volume);
			}

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