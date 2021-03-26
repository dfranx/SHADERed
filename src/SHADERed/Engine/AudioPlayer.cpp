#include <SHADERed/Engine/AudioPlayer.h>

#define STB_VORBIS_HEADER_ONLY
#include <misc/stb_vorbis.c>

#define MINIAUDIO_IMPLEMENTATION
#include <misc/miniaudio.h>

#undef STB_VORBIS_HEADER_ONLY
#include <misc/stb_vorbis.c>

#define PLAYER_PCM_FRAME_CHUNK_SIZE 512

void audioPlayerCallbackFixed(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	ed::eng::AudioPlayer* player = (ed::eng::AudioPlayer*)pDevice->pUserData;
	if (player == NULL)
		return;

	ma_uint64 readFrames = ma_decoder_read_pcm_frames(player->GetDecoder(), pOutput, frameCount);
	memcpy(player->GetAudioBuffer(), pOutput, readFrames * 2 * sizeof(short));

	if (readFrames < frameCount)
		player->SeekFrame(0);

	(void)pInput;
}
void audioPlayerCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	ed::eng::AudioPlayer* player = (ed::eng::AudioPlayer*)pDevice->pUserData;
	if (player == NULL)
		return;

	ma_uint32 pcmFramesAvailableInRB;
	ma_uint32 pcmFramesProcessed = 0;
	ma_uint8* pRunningOutput = (ma_uint8*)pOutput;

	/*
    The first thing to do is check if there's enough data available in the ring buffer. If so we can read from it. Otherwise we need to keep filling
    the ring buffer until there's enough, making sure we only fill the ring buffer in chunks of PCM_FRAME_CHUNK_SIZE.
    */
	while (pcmFramesProcessed < frameCount) { /* Keep going until we've filled the output buffer. */
		ma_uint32 framesRemaining = frameCount - pcmFramesProcessed;

		pcmFramesAvailableInRB = ma_pcm_rb_available_read(player->GetRingBuffer());
		if (pcmFramesAvailableInRB > 0) {
			ma_uint32 framesToRead = (framesRemaining < pcmFramesAvailableInRB) ? framesRemaining : pcmFramesAvailableInRB;
			void* pReadBuffer;

			ma_pcm_rb_acquire_read(player->GetRingBuffer(), &framesToRead, &pReadBuffer);
			{
				memcpy(pRunningOutput, pReadBuffer, framesToRead * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
			}
			ma_pcm_rb_commit_read(player->GetRingBuffer(), framesToRead, pReadBuffer);

			pRunningOutput += framesToRead * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels);
			pcmFramesProcessed += framesToRead;
		} else {
			/*
            There's nothing in the buffer. Fill it with more data from the callback. We reset the buffer first so that the read and write pointers
            are reset back to the start so we can fill the ring buffer in chunks of PCM_FRAME_CHUNK_SIZE which is what we initialized it with. Note
            that this is not how you would want to do it in a multi-threaded environment. In this case you would want to seek the write pointer
            forward via the producer thread and the read pointer forward via the consumer thread (this thread).
            */
			ma_uint32 framesToWrite = PLAYER_PCM_FRAME_CHUNK_SIZE;
			void* pWriteBuffer;

			ma_pcm_rb_reset(player->GetRingBuffer());
			ma_pcm_rb_acquire_write(player->GetRingBuffer(), &framesToWrite, &pWriteBuffer);
			{
				// MA_ASSERT(framesToWrite == PCM_FRAME_CHUNK_SIZE); /* <-- This should always work in this example because we just reset the ring buffer. */
				audioPlayerCallbackFixed(pDevice, pWriteBuffer, NULL, framesToWrite);
			}
			ma_pcm_rb_commit_write(player->GetRingBuffer(), framesToWrite, pWriteBuffer);
		}
	}

	/* Unused in this example. */
	(void)pInput;
}

namespace ed {
	namespace eng {
		AudioPlayer::AudioPlayer()
		{
			m_loaded = false;
			m_paused = true;
		}
		AudioPlayer::~AudioPlayer()
		{
			if (m_loaded)
				m_clean();
		}

		bool AudioPlayer::LoadFromFile(const std::string& filename)
		{
			if (m_loaded)
				m_clean();

			m_loaded = false;

			ma_pcm_rb_init(ma_format_s16, 2, PLAYER_PCM_FRAME_CHUNK_SIZE, NULL, NULL, &m_rb);
			m_decoderConfig = ma_decoder_config_init(ma_format_s16, 2, 48000);

			ma_result result = ma_decoder_init_file(filename.c_str(), &m_decoderConfig, &m_decoder);
			if (result != MA_SUCCESS)
				return false;

			m_deviceConfig = ma_device_config_init(ma_device_type_playback);
			m_deviceConfig.playback.format = m_decoder.outputFormat;
			m_deviceConfig.playback.channels = m_decoder.outputChannels;
			m_deviceConfig.sampleRate = m_decoder.outputSampleRate;
			m_deviceConfig.dataCallback = audioPlayerCallback;
			m_deviceConfig.pUserData = this;

			if (ma_device_init(NULL, &m_deviceConfig, &m_device) != MA_SUCCESS) {
				ma_decoder_uninit(&m_decoder);
				ma_pcm_rb_uninit(&m_rb);
				return false;
			}

			m_loaded = true;

			return true;
		}

		void AudioPlayer::Start()
		{
			m_paused = false;
			if (ma_device_start(&m_device) != MA_SUCCESS) {
				m_loaded = false;
				m_clean();
			}
		}

		void AudioPlayer::Stop()
		{
			m_paused = true;
			if (ma_device_stop(&m_device) != MA_SUCCESS) {
				m_loaded = false;
				m_clean();
			}
		}

		void AudioPlayer::m_clean()
		{
			ma_device_uninit(&m_device);
			ma_decoder_uninit(&m_decoder);
			ma_pcm_rb_uninit(&m_rb);
			m_paused = true;
			m_loaded = false;
		}
	}
}