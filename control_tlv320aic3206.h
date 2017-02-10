/* Extension control files for TLV320AIC3206
 * Copyright (c) 2016, Creare, bpf@creare.com
 *
 *
 */

#ifndef control_tlv320_h_
#define control_tlv320_h_

#include "AudioControl.h"

class AudioControlTLV320AIC3206: public AudioControl
{
public:
	bool enable(void);
	bool disable(void);
	bool volume(float n);
	bool inputLevel(float n);
	bool inputSelect(int n);
	bool micGain(float n);
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
