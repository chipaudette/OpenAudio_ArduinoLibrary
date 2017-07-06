
//ifndef _BTNRH_FFT_H
//define _BTNRH_FFT_H

#include "BTNRH_rfft.h"
#include <math.h>
//#include "chapro.h"
//#include "cha_ff.h"

/***********************************************************/
// FFT functions adapted from G. D. Bergland, "Subroutines FAST and FSST," (1979).
// In IEEE Acoustics, Speech, and Signal Processing Society.
// "Programs for Digital Signal Processing," IEEE Press, New York,

namespace BTNRH_FFT {

	static __inline int
	ilog2(int n)
	{
		int     m;
	
		for (m = 1; m < 32; m++)
			if (n == (1 << m))
				return (m);
		return (-1);
	}
	
	static __inline int
	bitrev(int ii, int m)
	{
		register int jj;
	
		jj = ii & 1;
		--m;
		while (--m > 0) {
			ii >>= 1;
			jj <<= 1;
			jj |= ii & 1;
		}
		return (jj);
	}
	
	static __inline void
	rad2(int ii, float *x0, float *x1)
	{
		int     k;
		float   t;
	
		for (k = 0; k < ii; k++) {
			t = x0[k] + x1[k];
			x1[k] = x0[k] - x1[k];
			x0[k] = t;
		}
	}
	
	static __inline void
	reorder1(int m, float *x)
	{
		int     j, k, kl, n;
		float   t;
	
		k = 4;
		kl = 2;
		n = 1 << m;
		for (j = 4; j <= n; j += 2) {
			if (k > j) {
				t = x[j - 1];
				x[j - 1] = x[k - 1];
				x[k - 1] = t;
			}
			k -= 2;
			if (k <= kl) {
				k = 2 * j;
				kl = j;
			}
		}
	}
	
	static __inline void
	reorder2(int m, float *x)
	{
		int     ji, ij, n;
		float   t;
	
		n = 1 << m;
		for (ij = 0; ij <= (n - 2); ij += 2) {
			ji = bitrev(ij >> 1, m) << 1;
			if (ij < ji) {
				t = x[ij];
				x[ij] = x[ji];
				x[ji] = t;
				t = x[ij + 1];
				x[ij + 1] = x[ji + 1];
				x[ji + 1] = t;
			}
		}
	}
	
	/***********************************************************/
	
	// rcfft
	
	static void
	rcrad4(int ii, int nn,
		float *x0, float *x1, float *x2, float *x3,
		float *x4, float *x5, float *x6, float *x7)
	{
		double  arg, tpiovn;
		float   c1, c2, c3, s1, s2, s3, pr, pi, r1, r5;
		float   t0, t1, t2, t3, t4, t5, t6, t7;
		int     i0, i4, j, j0, ji, jl, jr, jlast, k, k0, kl, m, n, ni;
	
		n = nn / 4;
		for (m = 1; (1 << m) < n; m++)
			continue;
		tpiovn = 2 * M_PI / nn;
		ji = 3;
		jl = 2;
		jr = 2;
		ni = (n + 1) / 2;
		for (i0 = 0; i0 < ni; i0++) {
			if (i0 == 0) {
				for (k = 0; k < ii; k++) {
					t0 = x0[k] + x2[k];
					t1 = x1[k] + x3[k];
					x2[k] = x0[k] - x2[k];
					x3[k] = x1[k] - x3[k];
					x0[k] = t0 + t1;
					x1[k] = t0 - t1;
				}
				if (nn > 4) {
					k0 = ii * 4;
					kl = k0 + ii;
					for (k = k0; k < kl; k++) {
						pr = (float) (M_SQRT1_2 * (x1[k] - x3[k]));
						pi = (float) (M_SQRT1_2 * (x1[k] + x3[k]));
						x3[k] = x2[k] + pi;
						x1[k] = pi - x2[k];
						x2[k] = x0[k] - pr;
						x0[k] += pr;
					}
				}
			} else {
				arg = tpiovn * bitrev(i0, m);
				c1 = cosf(arg);
				s1 = sinf(arg);
				c2 = c1 * c1 - s1 * s1;
				s2 = c1 * s1 + c1 * s1;
				c3 = c1 * c2 - s1 * s2;
				s3 = c2 * s1 + s2 * c1;
				i4 = ii * 4;
				j0 = jr * i4;
				k0 = ji * i4;
				jlast = j0 + ii;
				for (j = j0; j < jlast; j++) {
					k = k0 + j - j0;
					r1 = x1[j] * c1 - x5[k] * s1;
					r5 = x1[j] * s1 + x5[k] * c1;
					t2 = x2[j] * c2 - x6[k] * s2;
					t6 = x2[j] * s2 + x6[k] * c2;
					t3 = x3[j] * c3 - x7[k] * s3;
					t7 = x3[j] * s3 + x7[k] * c3;
					t0 = x0[j] + t2;
					t4 = x4[k] + t6;
					t2 = x0[j] - t2;
					t6 = x4[k] - t6;
					t1 = r1 + t3;
					t5 = r5 + t7;
					t3 = r1 - t3;
					t7 = r5 - t7;
					x0[j] = t0 + t1;
					x7[k] = t4 + t5;
					x6[k] = t0 - t1;
					x1[j] = t5 - t4;
					x2[j] = t2 - t7;
					x5[k] = t6 + t3;
					x4[k] = t2 + t7;
					x3[j] = t3 - t6;
				}
				jr += 2;
				ji -= 2;
				if (ji <= jl) {
					ji = 2 * jr - 1;
					jl = jr;
				}
			}
		}
	}
	
	//-----------------------------------------------------------
	
	static int
	rcfft2(float *x, int m)
	{
		int     ii, nn, m2, it, n;
	
		n = 1 << m;;
		m2 = m / 2;
	
	// radix 2
	
		if (m <= m2 * 2) {
			nn = 1;
		} else {
			nn = 2;
			ii = n / nn;
			rad2(ii, x, x + ii);
		}
	
	// radix 4
	
		if (m2 != 0) {
			for (it = 0; it < m2; it++) {
				nn = nn * 4;
				ii = n / nn;
				rcrad4(ii, nn, x, x + ii, x + 2 * ii, x + 3 * ii,
					x, x + ii, x + 2 * ii, x + 3 * ii);
			}
		}
	
	// re-order
	
		reorder1(m, x);
		reorder2(m, x);
		for (it = 3; it < n; it += 2)
			x[it] = -x[it];
		x[n] = x[1];
		x[1] = 0.0;
		x[n + 1] = 0.0;
	
		return (0);
	}
	
	/***********************************************************/
	
	// rcfft
	
	static void
	crrad4(int jj, int nn,
		float *x0, float *x1, float *x2, float *x3,
		float *x4, float *x5, float *x6, float *x7)
	{
		double  arg, tpiovn;
		float   c1, c2, c3, s1, s2, s3;
		float   t0, t1, t2, t3, t4, t5, t6, t7;
		int     ii, j, j0, ji, jr, jl, jlast, j4, k, k0, kl, m, n, ni;
	
		tpiovn = 2 * M_PI / nn;
		ji = 3;
		jl = 2;
		jr = 2;
		n = nn / 4;
		for (m = 1; (1 << m) < n; m++)
			continue;
		ni = (n + 1) / 2;
		for (ii = 0; ii < ni; ii++) {
			if (ii == 0) {
				for (k = 0; k < jj; k++) {
					t0 = x0[k] + x1[k];
					t1 = x0[k] - x1[k];
					t2 = x2[k] * 2;
					t3 = x3[k] * 2;
					x0[k] = t0 + t2;
					x2[k] = t0 - t2;
					x1[k] = t1 + t3;
					x3[k] = t1 - t3;
				}
				if (nn > 4) {
					k0 = jj * 4;
					kl = k0 + jj;
					for (k = k0; k < kl; k++) {
						t2 = x0[k] - x2[k];
						t3 = x1[k] + x3[k];
						x0[k] = (x0[k] + x2[k]) * 2;
						x2[k] = (x3[k] - x1[k]) * 2;
						x1[k] = (float) ((t2 + t3) * M_SQRT2);
						x3[k] = (float) ((t3 - t2) * M_SQRT2);
					}
				}
			} else {
				arg = tpiovn * bitrev(ii, m);
				c1 = cosf(arg);
				s1 = -sinf(arg);
				c2 = c1 * c1 - s1 * s1;
				s2 = c1 * s1 + c1 * s1;
				c3 = c1 * c2 - s1 * s2;
				s3 = c2 * s1 + s2 * c1;
				j4 = jj * 4;
				j0 = jr * j4;
				k0 = ji * j4;
				jlast = j0 + jj;
				for (j = j0; j < jlast; j++) {
					k = k0 + j - j0;
					t0 = x0[j] + x6[k];
					t1 = x7[k] - x1[j];
					t2 = x0[j] - x6[k];
					t3 = x7[k] + x1[j];
					t4 = x2[j] + x4[k];
					t5 = x5[k] - x3[j];
					t6 = x5[k] + x3[j];
					t7 = x4[k] - x2[j];
					x0[j] = t0 + t4;
					x4[k] = t1 + t5;
					x1[j] = (t2 + t6) * c1 - (t3 + t7) * s1;
					x5[k] = (t2 + t6) * s1 + (t3 + t7) * c1;
					x2[j] = (t0 - t4) * c2 - (t1 - t5) * s2;
					x6[k] = (t0 - t4) * s2 + (t1 - t5) * c2;
					x3[j] = (t2 - t6) * c3 - (t3 - t7) * s3;
					x7[k] = (t2 - t6) * s3 + (t3 - t7) * c3;
				}
				jr += 2;
				ji -= 2;
				if (ji <= jl) {
					ji = 2 * jr - 1;
					jl = jr;
				}
			}
		}
	}
	
	//-----------------------------------------------------------
	
	static int
	crfft2(float *x, int m)
	{
		int     n, i, it, nn, jj, m2;
	
		n = 1 << m;
		x[1] = x[n];
		m2 = m / 2;
	
	// re-order
	
		for (i = 3; i < n; i += 2)
			x[i] = -x[i];
		reorder2(m, x);
		reorder1(m, x);
	
	// radix 4
	
		if (m2 != 0) {
			nn = 4 * n;
			for (it = 0; it < m2; it++) {
				nn = nn / 4;
				jj = n / nn;
				crrad4(jj, nn, x, x + jj, x + 2 * jj, x + 3 * jj,
					x, x + jj, x + 2 * jj, x + 3 * jj);
			}
		}
	
	// radix 2
	
		if (m > m2 * 2) {
			jj = n / 2;
			rad2(jj, x, x + jj);
		}
	
		return (0);
	}
	
	/***********************************************************/
	
	// real-to-complex FFT
	
	//FUNC(void)
	void cha_fft_rc(float *x, int n)
	{
		int m;
	
		// assume n is a power of two 
		m = ilog2(n);
		rcfft2(x, m);
	}
	
	// complex-to-real inverse FFT
	
	//FUNC(void)
	void cha_fft_cr(float *x, int n)
	{
		int i, m;
	
		// assume n is a power of two 
		m = ilog2(n);
		crfft2(x, m);
		// scale inverse by 1/n
		for (i = 0; i < n; i++) {
			x[i] /= n;
		}
	}

};

//endif