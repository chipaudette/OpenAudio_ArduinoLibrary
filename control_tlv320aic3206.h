/* 
	control_tlv320aic3206
	
	Created: BPF@creare.com  Jan-Feb 2017

	Purpose: Control module for TLV320AIC3206 compatible with Teensy Audio Library
 
	License: MIT License.  Use at your own risk.
 */

#ifndef control_tlv320aic3206_h_
#define control_tlv320aic3206_h_

#include "AudioControl.h"

class AudioControlTLV320AIC3206: public AudioControl
{
public:
	//GUI: inputs:0, outputs:0  //this line used for automatic generation of GUI node
	AudioControlTLV320AIC3206(void) {};
	bool enable(void);
	bool disable(void);
	bool volume(float n);
	bool volume_dB(float n);
	bool inputLevel(float n);  //dummy to be compatible with Teensy Audio Library
	bool inputSelect(int n);
	bool setInputGain_dB(float n);
	bool setMicBias(int n);

private:
  void aic_reset(void);
  void aic_init(void);
  void aic_initDAC(void);
  void aic_initADC(void);
  unsigned int aic_readPage(uint8_t page, uint8_t reg);
  bool aic_writePage(uint8_t page, uint8_t reg, uint8_t val);
  bool aic_writeAddress(uint16_t address, uint8_t val);
  bool aic_goToPage(uint8_t page);
};

#define TYMPAN_OUTPUT_HEADPHONE_JACK_OUT 1

#define TYMPAN_INPUT_LINE_IN 				0
#define TYMPAN_INPUT_JACK_AS_MIC 		1
#define TYMPAN_INPUT_JACK_AS_LINEIN 2
#define TYMPAN_INPUT_ON_BOARD_MIC 	3

#define TYMPAN_MIC_BIAS_OFF				0
#define TYMPAN_MIC_BIAS_1_25 			1
#define TYMPAN_MIC_BIAS_1_7  			2
#define TYMPAN_MIC_BIAS_2_5  			3
#define TYMPAN_MIC_BIAS_VSUPPLY 	4
#define TYMPAN_DEFAULT_MIC_BIAS TYMPAN_MIC_BIAS_2_5

#endif
