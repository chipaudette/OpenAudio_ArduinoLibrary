
#ifndef _BTNRH_FFT_H
#define _BTNRH_FFT_H

#include <math.h>
//#include "chapro.h"
//#include "cha_ff.h"

/***********************************************************/
// FFT functions adapted from G. D. Bergland, "Subroutines FAST and FSST," (1979).
// In IEEE Acoustics, Speech, and Signal Processing Society.
// "Programs for Digital Signal Processing," IEEE Press, New York,

namespace BTNRH_FFT {
	void cha_fft_cr(float *, int);
	void cha_fft_rc(float *, int);
}

#endif