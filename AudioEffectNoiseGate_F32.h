/*
 * AudioEffectNoiseGate_F32
 * 
 * Created: Max Huster, Feb 2021
 * Purpose: This module mutes the Audio completly, when it's below a given threshold.
 *          
 * This processes a single stream fo audio data (ie, it is mono)       
 *          
 * MIT License.  use at your own risk.
*/

#ifndef _AudioEffectNoiseGate_F32_h
#define _AudioEffectNoiseGate_F32_h

#include <arm_math.h> //ARM DSP extensions.  for speed!
#include <AudioStream_F32.h>

#ifdef NOISEGATE_EXTENDEDINFO
const float minLinear = pow10f(-60 / 20.0f);
#endif

class AudioEffectNoiseGate_F32 : public AudioStream_F32
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  //GUI: shortName:NoiseGate
public:
  //constructor
  AudioEffectNoiseGate_F32(void) : AudioStream_F32(1, inputQueueArray_f32){};
  AudioEffectNoiseGate_F32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32){};

  //here's the method that does all the work
  void update(void)
  {

    //Serial.println("AudioEffectNoiseGate_F32: updating.");  //for debugging.
    audio_block_f32_t *block;
    block = AudioStream_F32::receiveWritable_f32();
    if (!block)
      return;
    // create a new audio block for the gain
    audio_block_f32_t *gainBlock = AudioStream_F32::allocate_f32();
    // calculate the desired gain
    calcGain(block, gainBlock);
    // smooth the "blocky" gain block
    calcSmoothedGain(gainBlock);
#ifdef NOISEGATE_EXTENDEDINFO

    float32_t min;
    float32_t max;
    uint32_t index;
    arm_min_f32(gainBlock->data, gainBlock->length, &min, &index);
    arm_max_f32(gainBlock->data, gainBlock->length, &max, &index);

    _inBetween = min > minLinear && max < (1 - minLinear);
#endif
    // multiply it to the input singal
    arm_mult_f32(gainBlock->data, block->data, block->data, block->length);
    // release gainBlock
    AudioStream_F32::release(gainBlock);

    //transmit the block and be done
    AudioStream_F32::transmit(block);
    AudioStream_F32::release(block);
  }

  void setThreshold(float dbfs)
  {
    // convert dbFS to linear value to comapre against later
    linearThreshold = pow10f(dbfs / 20.0f);
  }

  void setOpeningTime(float timeInSeconds)
  {
    openingTimeConst = expf(-1.0f / (timeInSeconds * AUDIO_SAMPLE_RATE));
  }

  void setClosingTime(float timeInSeconds)
  {
    closingTimeConst = expf(-1.0f / (timeInSeconds * AUDIO_SAMPLE_RATE));
  }

  void setHoldTime(float timeInSeconds)
  {
    holdTimeNumSamples = timeInSeconds * AUDIO_SAMPLE_RATE;
  }

#ifdef NOISEGATE_EXTENDEDINFO
  bool infoIsOpeningOrClosing()
  {
    return _inBetween;
  }

#endif
  bool infoIsOpen()
  {
    return _isOpenDisplay;
  }

private:
  float32_t linearThreshold;
  float32_t prev_gain_dB = 0;
  float32_t openingTimeConst, closingTimeConst;
  float lastGainBlockValue = 0;
  int32_t counter, holdTimeNumSamples = 0;
  audio_block_f32_t *inputQueueArray_f32[1]; //memory pointer for the input to this module
  bool falling = false;

  bool _isOpen = false;
  bool _isOpenDisplay = false;

#ifdef NOISEGATE_EXTENDEDINFO
  bool _inBetween = false;
#endif
  void calcGain(audio_block_f32_t *input, audio_block_f32_t *gainBlock)
  {
    _isOpen = false;
    for (int i = 0; i < input->length; i++)
    {
      // take absolute value and compare it to the set threshold
      bool isAboveThres = abs(input->data[i]) > linearThreshold;
      _isOpen |= isAboveThres;
      // if above the threshold set volume to 1 otherwise to 0, we did not account for holdtime
      gainBlock->data[i] = isAboveThres ? 1 : 0;

      // if we are falling and are above the threshold, the level is not falling
      if (falling & isAboveThres)
      {
        falling = false;
      }
      // if we have a falling signal
      if (falling || lastGainBlockValue > gainBlock->data[i])
      {
        // check whether the hold time is not reached
        if (counter < holdTimeNumSamples)
        {
          // signal is (still) falling
          falling = true;
          counter++;
          gainBlock->data[i] = 1.0f;
        }
        // otherwise the signal is already muted due to the line: "gainBlock->data[i] = isAboveThres ? 1 : 0;"
      }
      // note the last gain value, so we can compare it if the signal is falling in the next sample
      lastGainBlockValue = gainBlock->data[i];
    }
    // note the display value
    _isOpenDisplay = _isOpen;
  };

  //this method applies the "opening" and "closing" constants to smooth the
  //target gain level through time.
  void calcSmoothedGain(audio_block_f32_t *gain_block)
  {
    float32_t gain;
    float32_t one_minus_opening_const = 1.0f - openingTimeConst;
    float32_t one_minus_closing_const = 1.0f - closingTimeConst;
    for (int i = 0; i < gain_block->length; i++)
    {
      gain = gain_block->data[i];

      //smooth the gain using the opening or closing constants
      if (gain < prev_gain_dB)
      { //are we in the opening phase?
        gain_block->data[i] = openingTimeConst * prev_gain_dB + one_minus_opening_const * gain;
      }
      else
      { //or, we're in the closing phase
        gain_block->data[i] = closingTimeConst * prev_gain_dB + one_minus_closing_const * gain;
      }

      //save value for the next time through this loop
      prev_gain_dB = gain_block->data[i];
    }

    //return
    return; //the output here is gain_block
  }
};

#endif
