
#include "mathDSP_F32.h"
#include <math.h>

/*     acos_f32(x)  Bob Larkin   2020
 * This acos(x) approximation is intended as being fast, resonably accurate, and
 * with continuous function and derivative (slope) between -1 and 1 x value.
 * It is the result of a "Chebychev-zero" fit to the true values, and is a 7th
 * order polynomial for the full (-1.0, 1.0) range.
 *   Max error from -0.99 to 0.99 is < 0.018/Pi  (1.0 deg)
 *   Error at -1 or +1 is 0.112/Pi    (6.4 deg)
 * For acos, speed and accuracy are in conflict near x = +/- 1, but that
 * is not where communications phase detectors are normally used.
 * Using T3.6 this function, by itself, measures as 0.18 uSec
 *
 * Thanks to Bob K3KHF for ideas on minimizing errors with acos().
 *     RSL  5 April 2020.
 */
float mathDSP_F32::acos_f32(float x) {
  float w;
  // These next two error checks use 0.056 uSec per call
  if(x > 1.00000)  return 0.0f;
  if(x < -1.00000) return MF_PI;
  w = x * x;
  return 1.5707963268f+(x*((-0.97090f)+w*((-0.529008f)+w*(1.00279f-w*0.961446))));
}

/* *** Not currently used, but possible substitute for acosf(x) ***
 * Apparently based on Handbook of Mathematical Functions
 * M. Abramowitz and I.A. Stegun, Ed.  Check before using.
 * https://developer.download.nvidia.com/cg/acos.html
 * Absolute error <= 6.7e-5, good, but not as good as acosf()
 * T3.6 this measures 0.51 uSec (0.23 uSec from sqrtf() ),
 * better than acosf(x) by a factor of 2.
 */
float mathDSP_F32::approxAcos(float x) {
  if(x > 0.999999)  return 0.0f;
  if(x < -0.999999) return M_PI;  // 3.14159265358979f;
  float negate = float(x < 0);
  x = fabsf(x);
  float ret = -0.0187293f;
  ret = ret * x;
  ret = ret + 0.0742610f;
  ret = ret * x;
  ret = ret - 0.2121144f;
  ret = ret * x;
  ret = ret + 1.5707288f;
  ret = ret * sqrtf(1.0f-x);
  ret = ret - 2 * negate * ret;
  return negate * MF_PI + ret;
}

/* Polynomial approximating arctangenet on iput range (-1, 1)
 * giving result in a range of approximately (-pi/4, pi/4)
 * Max error < 0.005 radians (or 0.29 degrees)
 *
 * Directly from www.dsprelated.com/showarticle/1052.php
 * Thank you Nic Taylor---nice work.
 */
float mathDSP_F32::fastAtan2(float y, float x) {
    if (x != 0.0f) {
        if (fabsf(x) > fabsf(y)) {
            const float z = y / x;
            if (x > 0.0)
                // atan2(y,x) = atan(y/x) if x > 0
                return _Atan(z);
            else if (y >= 0.0)
                // atan2(y,x) = atan(y/x) + PI if x < 0, y >= 0
                return _Atan(z) + M_PI;
            else
                // atan2(y,x) = atan(y/x) - PI if x < 0, y < 0
                return _Atan(z) - M_PI;
        }
        else { // Use property atan(y/x) = PI/2-atan(x/y) if |y/x| > 1.
            const float z = x / y;
            if (y > 0.0)
                // atan2(y,x) = PI/2 - atan(x/y) if |y/x| > 1, y > 0
                return -_Atan(z) + M_PI_2;
            else
                // atan2(y,x) = -PI/2 - atan(x/y) if |y/x| > 1, y < 0
                return -_Atan(z) - M_PI_2;
        }
    }
    else {
        if (y > 0.0f) // x = 0, y > 0
            return M_PI_2;
        else if (y < 0.0f) // x = 0, y < 0
            return -M_PI_2;
    }
    return 0.0f; // x,y = 0. Undefined, stay finite.
}

/* float i0f(float x)  Returns the modified Bessel function Io(x).
 * Algorithm is based on Abromowitz and Stegun, Handbook of Mathematical
 * Functions, and Press, et. al., Numerical Recepies in C.
 * All in 32-bit floating point
 */
float mathDSP_F32::i0f(float x) {
    float af, bf, cf;
    if( (af=fabsf(x)) < 3.75f ) {
        cf = x/3.75f;
        cf = cf*cf;
        bf=1.0f+cf*(3.515623f+cf*(3.089943f+cf*(1.20675f+cf*(0.265973f+
             cf*(0.0360768f+cf*0.0045813f)))));
    }
    else {
        cf = 3.75f/af;
        bf=(expf(af)/sqrtf(af))*(0.3989423f+cf*(0.0132859f+cf*(0.0022532f+
             cf*(-0.0015756f+cf*(0.0091628f+cf*(-0.0205771f+cf*(0.0263554f+
             cf*(-0.0164763f+cf*0.0039238f))))))));
    }
    return bf;
    }

