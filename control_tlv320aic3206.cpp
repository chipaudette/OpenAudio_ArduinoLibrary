/* 
	control_tlv320aic3206
	
	Created: Brendan Flynn (http://www.flexvoltbiosensor.com/) for Tympan, Jan-Feb 2017
	Purpose: Control module for Texas Instruments TLV320AIC3206 compatible with Teensy Audio Library
 
	License: MIT License.  Use at your own risk.
 */

#include "control_tlv320aic3206.h"
#include <Wire.h>

//********************************  Constants  *******************************//

#define AIC3206_I2C_ADDR                                             0x18


#ifndef AIC_FS
#  define AIC_FS                                                     44100UL
#endif

#define AIC_BITS                                                        16

#define AIC_I2S_SLAVE                                                     1
#if AIC_I2S_SLAVE
// Direction of BCLK and WCLK (reg 27) is input if a slave:
# define AIC_CLK_DIR                                                    0
#else
// If master, make outputs:
# define AIC_CLK_DIR                                                   0x0C
#endif

//#ifndef AIC_CODEC_CLKIN_BCLK
//# define AIC_CODEC_CLKIN_BCLK                                           0
//#endif

//**************************** Clock Setup **********************************//

//**********************************  44100  *********************************//
#if AIC_FS == 44100

// MCLK = 180000000 * 16 / 255 = 11.294117 MHz // FROM TEENSY, FIXED

// PLL setup.  PLL_OUT = MCLK * R * J.D / P
//// J.D = 7.5264, P = 1, R = 1 => 90.32 MHz // FROM 12MHz CHA AND WHF //
// J.D = 7.9968, P = 1, R = 1 => 90.3168 MHz // For 44.1kHz exact
// J.D = 8.0000000002, P = 1, R = 1 => 9.35294117888MHz // for TEENSY 44.11764706kHz
#define PLL_J                                                             8
#define PLL_D                                                             0

// Bitclock divisor.
// BCLK = DAC_CLK/N = PLL_OUT/NDAC/N = 32*fs or 16*fs
// PLL_OUT = fs*NDAC*MDAC*DOSR
// BLCK = 32*fs = 1411200 = PLL
#if AIC_BITS == 16
#define BCLK_N                                                            8
#elif AIC_BITS == 32
#define BCLK_N                                                            4
#endif

// ADC/DAC FS setup.
// ADC_MOD_CLK = CODEC_CLKIN / (NADC * MADC)
// DAC_MOD_CLK = CODEC_CLKIN / (NDAC * MDAC)
// ADC_FS = PLL_OUT / (NADC*MADC*AOSR)
// DAC_FS = PLL_OUT / (NDAC*MDAC*DOSR)
// FS = 90.3168MHz / (8*2*128) = 44100 Hz.
// MOD = 90.3168MHz / (8*2) = 5644800 Hz

// Actual from Teensy: 44117.64706Hz * 128 => 5647058.82368Hz * 8*2 => 90352941.17888Hz

// DAC clock config.
// Note: MDAC*DOSR/32 >= RC, where RC is 8 for the default filter.
// See Table 2-21
// http://www.ti.com/lit/an/slaa463b/slaa463b.pdf
// PB1 - RC = 8.  Use M8, N2
// PB25 - RC = 12.  Use M8, N2

#define DOSR                                                            128
#define NDAC                                                              2
#define MDAC                                                              8

#define AOSR                                                            128
#define NADC                                                              2
#define MADC                                                              8

// Signal Processing Modes, Playback and Recording.
#define PRB_P                                                             1
#define PRB_R                                                             1

#endif // end fs if block

//**************************** Chip Setup **********************************//

//******************* INPUT DEFINITIONS *****************************//
// MIC routing registers
#define TYMPAN_MICPGA_LEFT_POSITIVE_REG  0x0134 // page 1 register 52
#define TYMPAN_MICPGA_LEFT_NEGATIVE_REG  0x0136 // page 1 register 54
#define TYMPAN_MICPGA_RIGHT_POSITIVE_REG 0x0137 // page 1 register 55
#define TYMPAN_MICPGA_RIGHT_NEGATIVE_REG 0x0139 // page 1 register 57

#define TYMPAN_MIC_ROUTING_POSITIVE_IN1     0b11000000 //
#define TYMPAN_MIC_ROUTING_POSITIVE_IN2     0b00110000 //
#define TYMPAN_MIC_ROUTING_POSITIVE_IN3     0b00001100 //
#define TYMPAN_MIC_ROUTING_POSITIVE_REVERSE 0b00000011 //

#define TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L  0b11000000 //
#define TYMPAN_MIC_ROUTING_NEGATIVE_IN2_REVERSE 0b00110000 //
#define TYMPAN_MIC_ROUTING_NEGATIVE_IN3_REVERSE 0b00001100 //
#define TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM2L  0b00000011 //

#define TYMPAN_MIC_ROUTING_RESISTANCE_10k 0b01010101
#define TYMPAN_MIC_ROUTING_RESISTANCE_20k 0b10101010
#define TYMPAN_MIC_ROUTING_RESISTANCE_40k 0b11111111
#define TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT TYMPAN_MIC_ROUTING_RESISTANCE_10k

#define TYMPAN_MICPGA_LEFT_VOLUME_REG    0x013B // page 1 register 59 // 0 to 47.5dB in 0.5dB steps
#define TYMPAN_MICPGA_RIGHT_VOLUME_REG   0x013C // page 1 register 60 // 0 to 47.5dB in 0.5dB steps

#define TYMPAN_MICPGA_VOLUME_ENABLE  0x00 // default is 0b11000000 - clear to 0 to enable

#define TYMPAN_MIC_BIAS_REG 0x0133 // page 1 reg 51
#define TYMPAN_MIC_BIAS_POWER_ON  0x40
#define TYMPAN_MIC_BIAS_POWER_OFF 0x00
#define TYMPAN_MIC_BIAS_OUTPUT_VOLTAGE_1_25     0x00
#define TYMPAN_MIC_BIAS_OUTPUT_VOLTAGE_1_7      0x01
#define TYMPAN_MIC_BIAS_OUTPUT_VOLTAGE_2_5      0x10
#define TYMPAN_MIC_BIAS_OUTPUT_VOLTAGE_VSUPPLY  0x11

#define TYMPAN_ADC_PROCESSING_BLOCK_REG 0x003d // page 0 register 61

#define TYMPAN_ADC_CHANNEL_POWER_REG 0x0051 // page 0 register81
#define TYMPAN_ADC_CHANNELS_ON 0b11000000 // power up left and right

#define TYMPAN_ADC_MUTE_REG 0x0052 //  page 0, register 82
#define TYMPAN_ADC_UNMUTE 0x00

bool AudioControlTLV320AIC3206::enable(void)
{
  delay(100);

  // Setup for Master mode, pins 18/19, external pullups, 400kHz, 200ms default timeout
	Wire.begin();
	delay(5);

  //hard reset the AIC
  //Serial.println("Hardware reset of AIC...");
  #define RESET_PIN  21
  pinMode(RESET_PIN,OUTPUT); 
  digitalWrite(RESET_PIN,HIGH);delay(50); //not reset
  digitalWrite(RESET_PIN,LOW);delay(50);  //reset
  digitalWrite(RESET_PIN,HIGH);delay(50);//not reset
	
  aic_reset(); delay(100);  //soft reset
  aic_init(); delay(100);
  aic_initADC(); delay(100);
  aic_initDAC(); delay(100);

  aic_readPage(0, 27); // check a specific register - a register read test

  if (debugToSerial) Serial.println("TLV320 enable done");

  return true;

}

bool AudioControlTLV320AIC3206::disable(void) {
  return true;
}

//dummy function to keep compatible with Teensy Audio Library
bool AudioControlTLV320AIC3206::inputLevel(float volume) {
  return false;
}

bool AudioControlTLV320AIC3206::inputSelect(int n) {
  if (n == TYMPAN_INPUT_LINE_IN) {
    // USE LINE IN SOLDER PADS
    aic_writeAddress(TYMPAN_MICPGA_LEFT_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN1 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_LEFT_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_RIGHT_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN1 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_RIGHT_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    // BIAS OFF
    setMicBias(TYMPAN_MIC_BIAS_OFF);

    if (debugToSerial) Serial.println("Set Audio Input to Line In");
    return true;
  } else if (n == TYMPAN_INPUT_JACK_AS_MIC) {
    // mic-jack = IN3
    aic_writeAddress(TYMPAN_MICPGA_LEFT_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN3 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_LEFT_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_RIGHT_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN3 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_RIGHT_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    // BIAS on, using default
    setMicBias(TYMPAN_DEFAULT_MIC_BIAS);

    if (debugToSerial) Serial.println("Set Audio Input to JACK AS MIC, BIAS SET TO DEFAULT 2.5V");
    return true;
  } else if (n == TYMPAN_INPUT_JACK_AS_LINEIN) {
    // 1
    // mic-jack = IN3
    aic_writeAddress(TYMPAN_MICPGA_LEFT_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN3 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_LEFT_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_RIGHT_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN3 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_RIGHT_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    // BIAS Off
    setMicBias(TYMPAN_MIC_BIAS_OFF);

    if (debugToSerial) Serial.println("Set Audio Input to JACK AS LINEIN, BIAS OFF");
    return true;
  } else if (n == TYMPAN_INPUT_ON_BOARD_MIC) {
    // on-board = IN2
    aic_writeAddress(TYMPAN_MICPGA_LEFT_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN2 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_LEFT_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_RIGHT_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN2 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    aic_writeAddress(TYMPAN_MICPGA_RIGHT_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
    // BIAS Off
    setMicBias(TYMPAN_MIC_BIAS_OFF);
    if (debugToSerial) Serial.println("Set Audio Input to Tympan On-Board MIC, BIAS OFF");

    return true;
  }
  Serial.print("controlTLV320AIC3206: ERROR: Unable to Select Input - Value not supported: ");
  Serial.println(n);
  return false;
}

bool AudioControlTLV320AIC3206::setMicBias(int n) {
  if (n == TYMPAN_MIC_BIAS_1_25) {
    aic_writeAddress(TYMPAN_MIC_BIAS_REG, TYMPAN_MIC_BIAS_POWER_ON | TYMPAN_MIC_BIAS_OUTPUT_VOLTAGE_1_25); // power up mic bias
    return true;
  } else if (n == TYMPAN_MIC_BIAS_1_7) {
    aic_writeAddress(TYMPAN_MIC_BIAS_REG, TYMPAN_MIC_BIAS_POWER_ON | TYMPAN_MIC_BIAS_OUTPUT_VOLTAGE_1_7); // power up mic bias
    return true;
  } else if (n == TYMPAN_MIC_BIAS_2_5) {
    aic_writeAddress(TYMPAN_MIC_BIAS_REG, TYMPAN_MIC_BIAS_POWER_ON | TYMPAN_MIC_BIAS_OUTPUT_VOLTAGE_2_5); // power up mic bias
    return true;
  } else if (n == TYMPAN_MIC_BIAS_VSUPPLY) {
    aic_writeAddress(TYMPAN_MIC_BIAS_REG, TYMPAN_MIC_BIAS_POWER_ON | TYMPAN_MIC_BIAS_OUTPUT_VOLTAGE_VSUPPLY); // power up mic bias
    return true;
  } else if (n == TYMPAN_MIC_BIAS_OFF) {
    aic_writeAddress(TYMPAN_MIC_BIAS_REG, TYMPAN_MIC_BIAS_POWER_OFF); // power up mic bias
    return true;
  }
  Serial.print("controlTLV320AIC3206: ERROR: Unable to set MIC BIAS - Value not supported: ");
  Serial.println(n);
  return false;
}

void AudioControlTLV320AIC3206::aic_reset() {
  if (debugToSerial) Serial.println("INFO: Reseting AIC");
  aic_writePage(0x00, 0x01, 0x01);
  // aic_writeAddress(0x0001, 0x01);

  delay(10);
}


// example - turn on IN3 - mic jack, with negatives routed to CM1L and with 10k resistance
// aic_writeAddress(TYMPAN_LEFT_MICPGA_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN3 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
// aic_writeAddress(TYMPAN_LEFT_MICPGA_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
// aic_writeAddress(TYMPAN_RIGHT_MICPGA_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN3 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
// aic_writeAddress(TYMPAN_RIGHT_MICPGA_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);

void AudioControlTLV320AIC3206::aic_initADC() {
  if (debugToSerial) Serial.println("INFO: Initializing AIC ADC");
  aic_writeAddress(TYMPAN_ADC_PROCESSING_BLOCK_REG, PRB_R);  // processing blocks - ADC
  aic_writePage(1, 61, 0); // 0x3D // Select ADC PTM_R4 Power Tune?
  aic_writePage(1, 71, 0b00110001); // 0x47 // Set MicPGA startup delay to 3.1ms
  aic_writeAddress(TYMPAN_MIC_BIAS_REG, TYMPAN_MIC_BIAS_POWER_ON | TYMPAN_MIC_BIAS_2_5); // power up mic bias

  aic_writeAddress(TYMPAN_MICPGA_LEFT_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN2 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
  aic_writeAddress(TYMPAN_MICPGA_LEFT_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
  aic_writeAddress(TYMPAN_MICPGA_RIGHT_POSITIVE_REG, TYMPAN_MIC_ROUTING_POSITIVE_IN2 & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);
  aic_writeAddress(TYMPAN_MICPGA_RIGHT_NEGATIVE_REG, TYMPAN_MIC_ROUTING_NEGATIVE_CM_TO_CM1L & TYMPAN_MIC_ROUTING_RESISTANCE_DEFAULT);

  aic_writeAddress(TYMPAN_MICPGA_LEFT_VOLUME_REG, TYMPAN_MICPGA_VOLUME_ENABLE); // enable Left MicPGA, set gain to 0 dB
  aic_writeAddress(TYMPAN_MICPGA_RIGHT_VOLUME_REG, TYMPAN_MICPGA_VOLUME_ENABLE); // enable Right MicPGA, set gain to 0 dB

  aic_writeAddress(TYMPAN_ADC_MUTE_REG, TYMPAN_ADC_UNMUTE); // Unmute Left and Right ADC Digital Volume Control
  aic_writeAddress(TYMPAN_ADC_CHANNEL_POWER_REG, TYMPAN_ADC_CHANNELS_ON); // Unmute Left and Right ADC Digital Volume Control
}

// set MICPGA volume, 0-47.5dB in 0.5dB setps
bool AudioControlTLV320AIC3206::setInputGain_dB(float volume) {
  if (volume < 0.0) {
    volume = 0.0; // 0.0 dB
    Serial.println("controlTLV320AIC3206: WARNING: Attempting to set MIC volume outside range");
  }
  if (volume > 47.5) {
    volume = 47.5; // 47.5 dB
    Serial.println("controlTLV320AIC3206: WARNING: Attempting to set MIC volume outside range");
  }

  volume = volume * 2.0; // convert to value map (0.5 dB steps)
  int8_t volume_int = (int8_t) (round(volume)); // round

  if (debugToSerial) {
	Serial.print("INFO: Setting MIC volume to ");
	Serial.print(volume, 1);
	Serial.print(".  Converted to volume map => ");
	Serial.println(volume_int);
  }

  aic_writeAddress(TYMPAN_MICPGA_LEFT_VOLUME_REG, TYMPAN_MICPGA_VOLUME_ENABLE | volume_int); // enable Left MicPGA, set gain to 0 dB
  aic_writeAddress(TYMPAN_MICPGA_RIGHT_VOLUME_REG, TYMPAN_MICPGA_VOLUME_ENABLE | volume_int); // enable Right MicPGA, set gain to 0 dB
  return true;
}

//******************* OUTPUT DEFINITIONS *****************************//
#define TYMPAN_DAC_PROCESSING_BLOCK_REG 0x003c // page 0 register 60
#define TYMPAN_DAC_VOLUME_LEFT_REG 0x0041 // page 0 reg 65
#define TYMPAN_DAC_VOLUME_RIGHT_REG 0x0042 // page 0 reg 66


//volume control, similar to Teensy Audio Board
// value between 0.0 and 1.0.  Set to span -58 to +15 dB
bool AudioControlTLV320AIC3206::volume(float volume) {
	volume = max(0.0, min(1.0, volume));
	float vol_dB = -58.f + (15.0 - (-58.0f)) * volume;
	volume_dB(vol_dB);
	return true;
}

// -63.6 to +24 dB in 0.5dB steps.  uses signed 8-bit
bool AudioControlTLV320AIC3206::volume_dB(float volume) {

  // Constrain to limits
  if (volume > 24.0) {
    volume = 24.0;
    Serial.println("controlTLV320AIC3206: WARNING: Attempting to set DAC Volume outside range");
  }
  if (volume < -63.5) {
    volume = -63.5;
    Serial.println("controlTLV320AIC3206: WARNING: Attempting to set DAC Volume outside range");
  }

  volume = volume * 2.0; // convert to value map (0.5 dB steps)
  int8_t volume_int = (int8_t) (round(volume)); // round

  if (debugToSerial) {
	Serial.print("INFO: Setting DAC volume to ");
	Serial.print(volume, 1);
	Serial.print(".  Converted to volume map => ");
	Serial.println(volume_int);
  }

  aic_writeAddress(TYMPAN_DAC_VOLUME_RIGHT_REG, volume_int);
  aic_writeAddress(TYMPAN_DAC_VOLUME_LEFT_REG, volume_int);
  return true;
}

void AudioControlTLV320AIC3206::aic_initDAC() {
  if (debugToSerial) Serial.println("INFO: Initializing AIC DAC");
  // PLAYBACK SETUP

  aic_writeAddress(TYMPAN_DAC_PROCESSING_BLOCK_REG, PRB_P); // processing blocks - DAC

  //aic_writePage(1, 20, 0x25); // 0x14 De-Pop
  aic_writePage(1, 12, 8); // route LDAC/RDAC to HPL/HPR
  aic_writePage(1, 13, 8); // route LDAC/RDAC to HPL/HPR
  aic_writePage(0, 63, 0xD6); // 0x3F // Power up LDAC/RDAC
//  aic_writePage(1, 14, 8); // route LDAC/RDAC to LOL/LOR
//  aic_writePage(1, 15, 8); // route LDAC/RDAC to LOL/LOR
  aic_writePage(1, 16, 0); // unmute HPL Driver, 0 gain
  aic_writePage(1, 17, 0); // unmute HPR Driver, 0 gain
//  aic_writePage(1, 18, 0); // unmute LOL Driver, 0 gain
//  aic_writePage(1, 19, 0); // unmute LOR Driver, 0 gain
  aic_writePage(1, 9, 0x30); // Power up HPL/HPR and LOL/LOR drivers
  delay(100);
  aic_writeAddress(TYMPAN_DAC_VOLUME_LEFT_REG,  0); // default to 0 dB
  aic_writeAddress(TYMPAN_DAC_VOLUME_RIGHT_REG, 0); // default to 0 dB
  aic_writePage(0, 64, 0); // 0x40 // Unmute LDAC/RDAC
}

void AudioControlTLV320AIC3206::aic_init() {
  if (debugToSerial) Serial.println("INFO: Initializing AIC");
  
  // PLL
  aic_writePage(0, 4, 3); // 0x04 low PLL clock range, MCLK is PLL input, PLL_OUT is CODEC_CLKIN
  aic_writePage(0, 5, (PLL_J != 0 ? 0x91 : 0x11));
  aic_writePage(0, 6, PLL_J);
  aic_writePage(0, 7, PLL_D >> 8);
  aic_writePage(0, 8, PLL_D &0xFF);

  // CLOCKS
  aic_writePage(0, 11, 0x80 | NDAC); // 0x0B
  aic_writePage(0, 12, 0x80 | MDAC); // 0x0C
  aic_writePage(0, 13, 0); // 0x0D
  aic_writePage(0, 14, DOSR); // 0x0E
  // aic_writePage(0, 18, 0); // 0x12 // powered down, ADC_CLK same as DAC_CLK
  // aic_writePage(0, 19, 0); // 0x13 // powered down, ADC_MOD_CLK same as DAC_MOD_CLK
  aic_writePage(0, 18, 0x80 | NADC); // 0x12
  aic_writePage(0, 19, 0x80 | MADC); // 0x13
  aic_writePage(0, 20, AOSR);
  aic_writePage(0, 30, 0x80 | BCLK_N); // power up BLCK N Divider, default is 128

  // POWER
  aic_writePage(1, 1, 8); // 0x01
  aic_writePage(1, 2, 0); // 0x02 Enable Master Analog Power Control
  aic_writePage(1, 10, 0); // common mode 0.9 for full chip, HP, LO  // from WHF/CHA
  aic_writePage(1, 71, 0x31); // 0x47 Set input power-up time to 3.1ms (for ADC)
  aic_writePage(1, 123, 1); // 0x7B Set reference to power up in 40ms when analog blocks are powered up
  //aic_writePage(1, 124, 6); // 0x7D Charge Pump
  aic_writePage(1, 125, 0x53); // Enable ground-centered mode, DC offset correction  // from WHF/CHA

  // !!!!!!!!! The below writes are from WHF/CHA - probably don't need?
  // aic_writePage(1, 1, 10); // 0x01 // Charge pump
 aic_writePage(0, 27, 0x01 | AIC_CLK_DIR | (AIC_BITS == 32 ? 0x30 : 0)); // 0x1B
  // aic_writePage(0, 28, 0); // 0x1C
}

unsigned int AudioControlTLV320AIC3206::aic_readPage(uint8_t page, uint8_t reg)
{
  unsigned int val;
  if (aic_goToPage(page)) {
    Wire.beginTransmission(AIC3206_I2C_ADDR);
    Wire.write(reg);
    unsigned int result = Wire.endTransmission();
    if (result != 0) {
      Serial.print("controlTLV320AIC3206: ERROR: Read Page.  Page: ");Serial.print(page);
      Serial.print(" Reg: ");Serial.print(reg);
      Serial.print(".  Received Error During Read Page: ");
      Serial.println(result);
      val = 300 + result;
      return val;
    }
    if (Wire.requestFrom(AIC3206_I2C_ADDR, 1) < 1) {
      Serial.print("controlTLV320AIC3206: ERROR: Read Page.  Page: ");Serial.print(page);
      Serial.print(" Reg: ");Serial.print(reg);
      Serial.println(".  Nothing to return");
      val = 400;
      return val;
    }
    if (Wire.available() >= 1) {
      uint16_t val = Wire.read();
	  if (debugToSerial) {
		Serial.print("INFO: Read Page.  Page: ");Serial.print(page);
		Serial.print(" Reg: ");Serial.print(reg);
		Serial.print(".  Received: ");
		Serial.println(val, HEX);
	  }
      return val;
    }
  } else {
    Serial.print("controlTLV320AIC3206: INFO: Read Page.  Page: ");Serial.print(page);
    Serial.print(" Reg: ");Serial.print(reg);
    Serial.println(".  Failed to go to read page.  Could not go there.");
    val = 500;
    return val;
  }
  val = 600;
  return val;
}

bool AudioControlTLV320AIC3206::aic_writeAddress(uint16_t address, uint8_t val) {
  uint8_t reg = (uint8_t) (address & 0xFF);
  uint8_t page = (uint8_t) ((address >> 8) & 0xFF);

  return aic_writePage(page, reg, val);
}

bool AudioControlTLV320AIC3206::aic_writePage(uint8_t page, uint8_t reg, uint8_t val) {
  if (debugToSerial) {
	Serial.print("INFO: Write Page.  Page: ");Serial.print(page);
	Serial.print(" Reg: ");Serial.print(reg);
	Serial.print(" Val: ");Serial.println(val);
  }
  if (aic_goToPage(page)) {
    Wire.beginTransmission(AIC3206_I2C_ADDR);
    Wire.write(reg);delay(10);
    Wire.write(val);delay(10);
    uint8_t result = Wire.endTransmission();
    if (result == 0) return true;
    else {
      Serial.print("controlTLV320AIC3206: Received Error During writePage(): Error = ");
      Serial.println(result);
    }
  }
  return false;
}

bool AudioControlTLV320AIC3206::aic_goToPage(byte page) {
  Wire.beginTransmission(AIC3206_I2C_ADDR);
  Wire.write(0x00); delay(10);// page register  //was delay(10) from BPF
  Wire.write(page); delay(10);// go to page   //was delay(10) from BPF
  byte result = Wire.endTransmission();
  if (result != 0) {
    Serial.print("controlTLV320AIC3206: Received Error During goToPage(): Error = ");
    Serial.println(result);
    if (result == 2) {
      // failed to transmit address
      //return aic_goToPage(page);
    } else if (result == 3) {
      // failed to transmit data
      //return aic_goToPage(page);
    }
    return false;
  }
  return true;
}
