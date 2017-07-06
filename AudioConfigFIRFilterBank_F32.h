/*
 * fir_filterbank.h
 * 
 * Created: Chip Audette, Creare LLC, Feb 2017
 *   Primarly built upon CHAPRO "Generic Hearing Aid" from 
 *   Boys Town National Research Hospital (BTNRH): https://github.com/BTNRH/chapro
 *   
 * License: MIT License.  Use at your own risk.
 * 
 */

#ifndef AudioConfigFIRFilterBank_F32_h
#define AudioConfigFIRFilterBank_F32_h

//include <Tympan_Library.h>
#include <OpenAudio_ArduinoLibrary.h>

#define fmove(x,y,n)    memmove(x,y,(n)*sizeof(float))
#define fcopy(x,y,n)    memcpy(x,y,(n)*sizeof(float))
#define fzero(x,n)      memset(x,0,(n)*sizeof(float))

class AudioConfigFIRFilterBank_F32 {
  //GUI: inputs:0, outputs:0  //this line used for automatic generation of GUI node  
  //GUI: shortName:config_FIRbank
  public:
    AudioConfigFIRFilterBank_F32(void) {}
	AudioConfigFIRFilterBank_F32(const AudioSettings_F32 &settings) {}
    AudioConfigFIRFilterBank_F32(const int n_chan, const int n_fir, const float sample_rate_Hz, float *corner_freq, float *filter_coeff)
	{
      createFilterCoeff(n_chan, n_fir, sample_rate_Hz, corner_freq, filter_coeff);
    }


    //createFilterCoeff:
    //   Purpose: create all of the FIR filter coefficients for the FIR filterbank
    //   Syntax: createFilterCoeff(n_chan, n_fir, sample_rate_Hz, corner_freq, filter_coeff)
    //      int n_chan (input): number of channels (number of filters) you desire.  Must be 2 or greater
    //      int n_fir (input): length of each FIR filter (should probably be 8 or greater)
    //      float sample_rate_Hz (input): sample rate of your system (used to scale the corner_freq values)
    //      float *corner_freq (input): array of frequencies (Hz) seperating each band in your filter bank.
    //          should contain n_chan-1 values because it should exclude the bottom (0 Hz) and the top
    //          (Nyquist) as those values are already assumed by this routine. An valid example is below:
    //          int n_chan = 8;  float cf[] = {317.1666, 502.9734, 797.6319, 1264.9, 2005.9, 3181.1, 5044.7}; 
    //      float *filter_coeff (output): array of FIR filter coefficients that are computed by this
    //          routine.  You must have pre-allocated the array such as: float filter_coeff[N_CHAN][N_FIR];
    //Optional Usage: if you want 8 default filters spaced logarithmically, use: float *corner_freq = NULL
    void createFilterCoeff(const int n_chan, const int n_fir, const float sample_rate_Hz, float *corner_freq, float *filter_coeff) {
      float *cf = corner_freq;
      int flag__free_cf = 0;
      if (cf == NULL) {
        //compute corner frequencies that are logarithmically spaced
        cf = (float *) calloc(n_chan, sizeof(float));
        flag__free_cf = 1;
        computeLogSpacedCornerFreqs(n_chan, sample_rate_Hz, cf);
      }
      const int window_type = 0;  //0 = Hamming, 1=Blackmann, 2 = Hanning
      fir_filterbank(filter_coeff, cf, n_chan, n_fir, window_type, sample_rate_Hz);
      if (flag__free_cf) free(cf); 
    }

    //compute frequencies that space zero to nyquist.  Leave zero off, because it is assumed to exist in the later code.
    //example of an *8* channel set of frequencies: cf = {317.1666, 502.9734, 797.6319, 1264.9, 2005.9, 3181.1, 5044.7}
    void computeLogSpacedCornerFreqs(const int n_chan, const float sample_rate_Hz, float *cf) {
      float cf_8_band[] = {317.1666, 502.9734, 797.6319, 1264.9, 2005.9, 3181.1, 5044.7};
      float scale_fac = expf(logf(cf_8_band[6]/cf_8_band[0]) / ((float)(n_chan-2)));
      //Serial.print("MakeFIRFilterBank: computeEvenlySpacedCornerFreqs: scale_fac = "); Serial.println(scale_fac);
      cf[0] = cf_8_band[0];
      //Serial.println("MakeFIRFilterBank: computeEvenlySpacedCornerFreqs: cf = ");Serial.print(cf[0]); Serial.print(", ");
      for (int i=1; i < n_chan-1; i++) {
        cf[i] = cf[i-1]*scale_fac;
        //Serial.print(cf[i]); Serial.print(", ");
      }
      //Serial.println();
    }
  private:

    int nextPowerOfTwo(int n) {
      const int n_out_vals = 8;
      int out_vals[n_out_vals] = {8, 16, 32, 64, 128, 256, 512, 1024};
      if (n < out_vals[0]) return out_vals[0];
      for (int i=1;i<n_out_vals; i++) {
        if ((n > out_vals[i-1]) & (n <= out_vals[i])) {
          return out_vals[i];
        }
      }
      return n;
    }

    void fir_filterbank(float *bb, float *cf, const int nc, const int nw_orig, const int wt, const float sr);
};
#endif

//    static CHA_DSL dsl = {5, 50, 119, 0, 8,
//        {317.1666,502.9734,797.6319,1264.9,2005.9,3181.1,5044.7}, //log spaced frequencies.
//        {-13.5942,-16.5909,-3.7978,6.6176,11.3050,23.7183,35.8586,37.3885},
//        {0.7,0.9,1,1.1,1.2,1.4,1.6,1.7},
//        {32.2,26.5,26.7,26.7,29.8,33.6,34.3,32.7},
//        {78.7667,88.2,90.7,92.8333,98.2,103.3,101.9,99.8}
//    };

//    //x is the input waveform
//    //y is the processed waveform
//    //n is the length of the waveform
//    //fs is the sample rate...24000 Hz
//    //dsl are the settings for each band
//   t1 = amplify(x, y, n, fs, &dsl);

//amplify(float *x, float *y, int n, double fs, CHA_DSL *dsl)
//{
//    int nc;
//    static int    nw = 256;         // window size
//    static int    cs = 32;          // chunk size
//    static int    wt = 0;           // window type: 0=Hamming, 1=Blackman
//    static void *cp[NPTR] = {0};
//    static CHA_WDRC gha = {1, 50, 24000, 119, 0, 105, 10, 105};
//
//    nc = dsl->nchannel;  //8?
//    cha_firfb_prepare(cp, dsl->cross_freq, nc, fs, nw, wt, cs);
//    cha_agc_prepare(cp, dsl, &gha);
//    sp_tic();
//    WDRC(cp, x, y, n, nc);
//    return (sp_toc());
//}

//FUNC(int)
//cha_firfb_prepare(CHA_PTR cp, double *cf, int nc, double fs, 
//    int nw, int wt, int cs)
//{
//    float   *bb;
//    int      ns, nt;
//
//    if (cs <= 0) {
//        return (1);
//    }
//    cha_prepare(cp);
//    CHA_IVAR[_cs] = cs; //cs = 32
//    CHA_DVAR[_fs] = fs; //fs = 24000
//    // allocate window buffers
//    CHA_IVAR[_nw] = nw; //nw = 256
//    CHA_IVAR[_nc] = nc; //nc = 32
//    nt = nw * 2;  //nt = 256*2 = 512
//    ns = nt + 2;  //ns = 512+2 = 514
//    cha_allocate(cp, ns, sizeof(float), _ffxx);  //allocate for input
//    cha_allocate(cp, ns, sizeof(float), _ffyy);  //allocate for output
//    cha_allocate(cp, nc * (nw + cs), sizeof(float), _ffzz);  //allocate per channel
//    // compute FIR-filterbank coefficients
//    bb = calloc(nc * nw, sizeof(float));  //allocate for filter coeff (256 long, 8 channels)
//    fir_filterbank(bb, cf, nc, nw, wt, fs);  //make the fir filter bank
//    // Fourier-transform FIR coefficients
//    if (cs < nw) {  // short chunk
//        fir_transform_sc(cp, bb, nc, nw, cs);
//    } else {        // long chunk
//        fir_transform_lc(cp, bb, nc, nw, cs);
//    }
//    free(bb);
//
//    return (0);
//}

// fir_filterbank(   float *bb,   double *cf, int nc, int nw, int wt, double sr)
//                filter coeff, corner freqs,      8,    256,      0,     24000)
//{
//    double   p, w, a = 0.16, sm = 0;
//    float   *ww, *bk, *xx, *yy;
//    int      j, k, kk, nt, nf, ns, *be;
//
//    nt = nw * 2;  //nt = 256*2 = 512
//    nf = nw + 1;  //nyquist frequency bin is 256+1 = 257
//    ns = nf * 2;  //when complex, number values to carry is nyquist * 2 = 514
//    be = (int *) calloc(nc + 1, sizeof(int));  
//    ww = (float *) calloc(nw, sizeof(float)); //window is 256 long
//    xx = (float *) calloc(ns, sizeof(float)); //input data is 514 points long
//    yy = (float *) calloc(ns, sizeof(float)); //output data is 514 points long
//    // window
//    for (j = 0; j < nw; j++) { //nw = 256
//        p = M_PI * (2.0 * j - nw) / nw;  //phase for computing window, radians
//        if (wt == 0) {  //wt is zero
//            w = 0.54 + 0.46 * cos(p);                   // Hamming
//        } else {
//            w = (1 - a + cos(p) + a * cos(2 * p)) / 2;  // Blackman
//        }
//        sm += w;  //sum the window value.  Doesn't appear to be used anywhere
//        ww[j] = (float) w;  //save the windowing coefficient...there are 256 of them
//    }
//    // frequency bands
//    be[0] = 0;  //first channel is DC bin
//    for (k = 1; k < nc; k++) { //loop over the rest of the 8 channels
//        kk = round(nf * cf[k - 1] * (2 / sr));  //get bin of the channel (upper?) corner frequency...assumes factor of two zero-padding?
//        be[k] = (kk > nf) ? nf : kk;  //make sure we don't go above the nyquist bin (bin 257, assuming a 512 FFT)
//    }
//    be[nc] = nf;  //the last one is the nyquist freuquency
//    // channel tranfer functions
//    fzero(xx, ns);  //zero the xx vector
//    xx[nw / 2] = 1;  //create an impulse in the middle of the (non-overlapped part of the) time-domain...sample 129
//    cha_fft_rc(xx, nt);  //convert to frequency domain..512 points long
//    for (k = 0; k < nc; k++) {  //loop over each channel
//        bk = bb + k * nw;  //bin index for this channel
//        fzero(yy, ns);  //zero out the output bins
//        fcopy(yy + be[k] * 2, xx + be[k] * 2, (be[k + 1] - be[k]) * 2); //copy just the desired frequeny bins in our passband
//        cha_fft_cr(yy, nt);  //convert back to time domain
// // apply window to iFFT of bandpass
//        for (j = 0; j < nw; j++) {
//            yy[j] *= ww[j];
//        }
//        fcopy(bk, yy, nw); //copy output into the output filter...just the 256 points
//    }
//    free(be);
//    free(ww);
//    free(xx);
//    free(yy);
//}

