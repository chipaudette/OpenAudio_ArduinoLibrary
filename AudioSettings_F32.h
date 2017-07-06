
#ifndef _AudioSettings_F32_
#define _AudioSettings_F32_

class AudioSettings_F32 {
	public:
		AudioSettings_F32(float fs_Hz, int block_size) :
			sample_rate_Hz(fs_Hz), audio_block_samples(block_size) {}
		const float sample_rate_Hz;
		const int audio_block_samples;
		
		float cpu_load_percent(const int n);
		float processorUsage(void);
		float processorUsageMax(void);
		void processorUsageMaxReset(void);
};

#endif