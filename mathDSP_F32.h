/*
  mathDSP.h - Definitions and functions to support OpenAudio_ArduinoLibrary_F32
  Created by Bob Larkin 15 April 2020.
  
*/
#ifndef mathDSP_F32_h
#define mathDSP_F32_h


#ifndef M_PI_2
#define M_PI_2  1.57079632679489661923
#endif

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

#ifndef M_TWOPI
#define M_TWOPI  6.28318530717958647692
#endif

#ifndef MF_PI_2
#define MF_PI_2  1.5707963f
#endif

#ifndef MF_PI
#define MF_PI    3.14159265f
#endif

#ifndef MF_TWOPI
#define MF_TWOPI 6.2831853f
#endif

class mathDSP_F32
{
  public:
    float acos_f32(float x);
    float approxAcos(float x);
    float fastAtan2(float y, float x);
    float i0f(float x);
  private:
    // Support for FastAtan2(x,y)
    float _Atan(float z) {
      const float n1 = 0.97239411f;
      const float n2 = -0.19194795f;
      return (n1 + n2 * z * z) * z;
    }
};

#endif
