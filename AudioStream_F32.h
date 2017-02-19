/*
 * AudioStream_F32
 * 
 * Created: Chip Audette, November 2016
 * Purpose; Extend the Teensy Audio Library's "AudioStream" to permit floating-point audio data.
 *          
 * I modeled it directly on the Teensy code in "AudioStream.h" and "AudioStream.cpp", which are 
 * available here: https://github.com/PaulStoffregen/cores/tree/master/teensy3
 * 
 * MIT License.  use at your own risk.
*/

#ifndef _AudioStream_F32_h
#define _AudioStream_F32_h

#include <arm_math.h> //ARM DSP extensions.  for speed!
#include <Audio.h> //Teensy Audio Library


class AudioStream_F32;
class AudioConnection_F32;
class AudioSettings_F32;


class AudioSettings_F32 {
	public:
		AudioSettings_F32(float fs_Hz, int block_size) :
			sample_rate_Hz(fs_Hz), audio_block_samples(block_size) {}
		const float sample_rate_Hz;
		const int audio_block_samples;
		
		float cpu_load_percent(const int n) { //n is the number of cycles
			#define CYCLE_COUNTER_APPROX_PERCENT(n) (((n) + (F_CPU / 32 / AUDIO_SAMPLE_RATE * AUDIO_BLOCK_SAMPLES / 100)) / (F_CPU / 16 / AUDIO_SAMPLE_RATE * AUDIO_BLOCK_SAMPLES / 100))
			float foo1 = ((float)(F_CPU / 32))/sample_rate_Hz;
			foo1 *= ((float)audio_block_samples);
			foo1 /= 100.f;
			foo1 += (float)n;
			float foo2 = (float)(F_CPU / 16)/sample_rate_Hz;
			foo2 *= ((float)audio_block_samples);
			foo2 /= 100.f;
			return  foo1 / foo2;
			//return (((n) + (F_CPU / 32 / sample_rate_Hz * audio_block_samples / 100)) / (F_CPU / 16 / sample_rate_Hz * audio_block_samples / 100));
		}

		float processorUsage(void) { return cpu_load_percent(AudioStream::cpu_cycles_total); };
		float processorUsageMax(void) { return cpu_load_percent(AudioStream::cpu_cycles_total_max); }
		void processorUsageMaxReset(void) { AudioStream::cpu_cycles_total_max = AudioStream::cpu_cycles_total; }
};

//create a new structure to hold audio as floating point values.
//modeled on the existing teensy audio block struct, which uses Int16
//https://github.com/PaulStoffregen/cores/blob/268848cdb0121f26b7ef6b82b4fb54abbe465427/teensy3/AudioStream.h
class audio_block_f32_t {
	public:
		audio_block_f32_t(void) {};
		audio_block_f32_t(const AudioSettings_F32 &settings) {
			fs_Hz = settings.sample_rate_Hz;
			length = settings.audio_block_samples;
		};
		
		unsigned char ref_count;
		unsigned char memory_pool_index;
		unsigned char reserved1;
		unsigned char reserved2;
		float32_t data[AUDIO_BLOCK_SAMPLES]; // AUDIO_BLOCK_SAMPLES is 128, from AudioStream.h
		const int full_length = AUDIO_BLOCK_SAMPLES;
		int length = AUDIO_BLOCK_SAMPLES; // AUDIO_BLOCK_SAMPLES is 128, from AudioStream.h
		float fs_Hz = AUDIO_SAMPLE_RATE; // AUDIO_SAMPLE_RATE is 44117.64706 from AudioStream.h
};

class AudioConnection_F32
{
  public:
    AudioConnection_F32(AudioStream_F32 &source, AudioStream_F32 &destination) :
      src(source), dst(destination), src_index(0), dest_index(0),
      next_dest(NULL)
      { connect(); }
    AudioConnection_F32(AudioStream_F32 &source, unsigned char sourceOutput,
      AudioStream_F32 &destination, unsigned char destinationInput) :
      src(source), dst(destination),
      src_index(sourceOutput), dest_index(destinationInput),
      next_dest(NULL)
      { connect(); }
    friend class AudioStream_F32;
  protected:
    void connect(void);
    AudioStream_F32 &src;
    AudioStream_F32 &dst;
    unsigned char src_index;
    unsigned char dest_index;
    AudioConnection_F32 *next_dest;
};

#define AudioMemory_F32(num) ({ \
  static audio_block_f32_t data_f32[num]; \
  AudioStream_F32::initialize_f32_memory(data_f32, num); \
})

#define AudioMemory_F32_wSettings(num,settings) ({ \
  static audio_block_f32_t data_f32[num]; \
  AudioStream_F32::initialize_f32_memory(data_f32, num, settings); \
})


#define AudioMemoryUsage_F32() (AudioStream_F32::f32_memory_used)
#define AudioMemoryUsageMax_F32() (AudioStream_F32::f32_memory_used_max)
#define AudioMemoryUsageMaxReset_F32() (AudioStream_F32::f32_memory_used_max = AudioStream_F32::f32_memory_used)

class AudioStream_F32 : public AudioStream {
  public:
    AudioStream_F32(unsigned char n_input_f32, audio_block_f32_t **iqueue) : AudioStream(1, inputQueueArray_i16), 
        num_inputs_f32(n_input_f32), inputQueue_f32(iqueue) {
      //active_f32 = false;
      destination_list_f32 = NULL;
      for (int i=0; i < n_input_f32; i++) {
        inputQueue_f32[i] = NULL;
      }
    };
    static void initialize_f32_memory(audio_block_f32_t *data, unsigned int num);
    static void initialize_f32_memory(audio_block_f32_t *data, unsigned int num, const AudioSettings_F32 &settings);
    //virtual void update(audio_block_f32_t *) = 0; 
    static uint8_t f32_memory_used;
    static uint8_t f32_memory_used_max;
    
  protected:
    //bool active_f32;
    unsigned char num_inputs_f32;
    static audio_block_f32_t * allocate_f32(void);
    static void release(audio_block_f32_t * block);
    void transmit(audio_block_f32_t *block, unsigned char index = 0);
    audio_block_f32_t * receiveReadOnly_f32(unsigned int index = 0);
    audio_block_f32_t * receiveWritable_f32(unsigned int index = 0);  
    friend class AudioConnection_F32;
	
  private:
    AudioConnection_F32 *destination_list_f32;
    audio_block_f32_t **inputQueue_f32;
    virtual void update(void) = 0;
    audio_block_t *inputQueueArray_i16[1];  //two for stereo
    static audio_block_f32_t *f32_memory_pool;
    static uint32_t f32_memory_pool_available_mask[6];
};



#endif