
#include "AudioControlTester.h"

void AudioTestSignalGenerator_F32::update(void) {
  //receive the input audio data
  audio_block_f32_t *in_block = AudioStream_F32::receiveReadOnly_f32();
  if (!in_block) return;

  //if we're not testing, just transmit the block
  if (!is_testing) {
    AudioStream_F32::transmit(in_block); // send the FIR output
    AudioStream_F32::release(in_block);
    return;
  }

  //if we are testing, we're going to substitute a new signal,
  //so we can release the block for the original signal
  AudioStream_F32::release(in_block);

  //allocate memory for block that we'll send
  audio_block_f32_t *out_block = allocate_f32();
  if (!out_block) return;

  //now generate the new siginal
  sine_gen.begin(); record_queue.begin();  //activate
  sine_gen.update();  sine_gen.end();
  gain_alg.update();
  record_queue.update();  record_queue.end();
  audio_block_f32_t *queue_block = record_queue.getAudioBlock();
  
  //copy to out_block (why can't I just send the queue_block?  I tried.  It doesn't seem to work. try it again later!
  for (int i=0; i < queue_block->length; i++) out_block->data[i] = queue_block->data[i];
  record_queue.freeAudioBlock();

  //send the data
  AudioStream_F32::transmit(out_block); // send the FIR output
  AudioStream_F32::release(out_block);
}


void AudioTestSignalMeasurement_F32::update(void) {
  
  //if we're not testing, just return
  if (!is_testing) {
    return;
  }

  //receive the input audio data...the baseline and the test
  audio_block_f32_t *in_block_baseline = AudioStream_F32::receiveReadOnly_f32(0);
  if (!in_block_baseline) return;
  audio_block_f32_t *in_block_test = AudioStream_F32::receiveReadOnly_f32(1);
  if (!in_block_test) {
    AudioStream_F32::release(in_block_baseline);
    return;
  }

  //compute the rms of both signals
  float baseline_rms = computeRMS(in_block_baseline->data, in_block_baseline->length);
  float test_rms = computeRMS(in_block_test->data, in_block_test->length);

  //Release memory
  AudioStream_F32::release(in_block_baseline);
  AudioStream_F32::release(in_block_test);
  
  //notify controller
  if (testController != NULL) testController->transferRMSValues(baseline_rms, test_rms);
}

void AudioTestSignalMeasurementMulti_F32::update(void) {
  
  //if we're not testing, just return
  if (!is_testing) {
    return;
  }

  //receive the input audio data...the baseline and the test
  audio_block_f32_t *in_block_baseline = AudioStream_F32::receiveReadOnly_f32(0);
  if (in_block_baseline==NULL) return;
  float baseline_rms = computeRMS(in_block_baseline->data, in_block_baseline->length);
  AudioStream_F32::release(in_block_baseline);
  
  //loop over each of the test data connections
  float test_rms[num_test_values];
  int n_with_data = 0;
  for (int Ichan=0; Ichan < num_test_values; Ichan++) {
	audio_block_f32_t *in_block_test = AudioStream_F32::receiveReadOnly_f32(1+Ichan);
	if (in_block_test==NULL) {
		//no data
		test_rms[Ichan]=0.0f;
	} else {
		//process data
		n_with_data = Ichan+1;
		test_rms[Ichan]=computeRMS(in_block_test->data, in_block_test->length);
		AudioStream_F32::release(in_block_test);
	}
  }
 
  
  //notify controller
  if (testController != NULL) testController->transferRMSValues(baseline_rms, test_rms, n_with_data);
}
