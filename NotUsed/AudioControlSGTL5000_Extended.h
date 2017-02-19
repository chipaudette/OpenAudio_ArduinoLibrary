
/* AudioContolSGTL5000_Extended.h
 *  
 *  Purpose: Extends the functionality of the AudioControlSGTL5000 from the Teensy Audio Library
 *  
 *  Created: Chip Audette, Nov 2016
 *  
 *  License: MIT License.  Use at your own risk
 */

#ifndef _AudioControlSGTL5000_Extended
#define _AudioControlSGTL5000_Extended

class AudioControlSGTL5000_Extended : public AudioControlSGTL5000
{
  //GUI: inputs:0, outputs:0  //this line used for automatic generation of GUI node
  public:
    AudioControlSGTL5000_Extended(void) {};
    bool micBiasEnable(void) {
      return micBiasEnable(3.0);
    }
    bool micBiasEnable(float volt) {
      //in the SGTL5000 datasheet see Table 29. CHIP_MIC_CTRL  Register 0x002A

      ///bias resistor is set via bits 9-8 (out of 15-0)
      uint16_t bias_resistor_setting = 1 << 8;  //set the bit for the registor to enable 2kOhm

      //bias voltage is set via bits 6-4 (out of 15-0)
      uint16_t  bias_voltage_setting = ((uint16_t)((volt - 1.25) / 0.250 + 0.5)) << 4;
      
      //combine the two settings and issue the command
      return write(0x002A, bias_voltage_setting | bias_resistor_setting);
    }
    bool micBiasDisable(void) {
      //in the SGTL5000 datasheet see Table 29. CHIP_MIC_CTRL  Register 0x002A
      return write(0x002A, 0x00);
    }
};

#endif

