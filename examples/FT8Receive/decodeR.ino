/*
 * decodeR.ino
 * Basically the Goba decode.h and .c with minor changes for Teensy
 * Arduino use along with the floating point OpenAudio_ArduinoLibrary.
 * Bob Larkin W7PUA, September 2022.
 *
 */

/* Thank you to Kārlis Goba, YL3JG, https://github.com/kgoba/ft8_lib
 * and to Charlie Hill, W5BAA, https://github.com/Rotron/Pocket-FT8
 * as well as the all the contributors to the Joe Taylor WSJT project.
 * See "The FT4 and FT8 Communication Protocols," Steve Franks, K9AN,
 * Bill Somerville, G4WJS and Joe Taylor, K1JT, QEX July/August 2020
 * pp 7-17 as well as https://www.physics.princeton.edu/pulsar/K1JT
 */

/*  *****  MIT License  ***

Copyright (c) 2018 Kārlis Goba

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*  CALL
  int  num_candidates = find_sync(export_fft_power,
                            ft8_msg_samples, HIGH_FREQ_INDEX, kCostas_map, kMax_candidates,
                            candidate_list, kMin_score);
 */
// Localize top N candidates in frequency and time according to their
// sync strength (looking at Costas symbols).
// We treat and organize the candidate list as a min-heap (empty initially).
//                                          92            368
int find_sync(const uint8_t *power, int num_blocks, int num_bins,           //     40
              const uint8_t *sync_map, int num_candidates, Candidate *heap, int min_score) {
    int heap_size = 0;
    const uint8_t *p8;

	// Get a noise power estimate by averaging power[] values on a subset of the data.
	// ToDo - Is the averaging of log values here adequate?  Really should be
	// power averaging, but that is time consuming.
	// This is usd only when all signals are low in amplitude. 
    // Noise estimate takes about 145 uSec.
	int npCount = 0;
	float32_t np = 0.0f;
	for(int jj=0; jj<368; jj += 4)     // 0 to 367, inclusive,  by 4's, 92 of them
	    {
        for(int kk=48; kk<280; kk+=2)
		    {
			np += (float32_t)power[jj*368+kk];
			npCount++;
			}
		}
	FT8noisePowerEstimateL = np/(float32_t)npCount;
	FT8noisePwrDBIntL = (int16_t)(0.5f*FT8noisePowerEstimateL);

#if DEBUG_N
    Serial.println("Data for low SNR noise estimate:");
	Serial.print("    Full Ave Noise = "); Serial.println(FT8noisePowerEstimateL);
	Serial.print("    Full Ave dB Noise Int = "); Serial.println(FT8noisePwrDBIntL);
	Serial.print("    Noise Sample Count = "); Serial.println(npCount);
#endif
	
    // Here we allow time offsets that exceed signal boundaries, as long as we still have all data bits.
    // I.e. we can afford to skip the first 7 or the last 7 Costas symbols, as long as we track how many
    // sync symbols we included in the score, so the score is averaged.
	// This sync search takes about 27 milliseconds
    for (int alt = 0; alt < 4; ++alt)
        {
		//                                             92-79+7=20
        for (int time_offset = -7; time_offset < num_blocks - FT8_NN + 7; ++time_offset)  // FT8_NN=79
            {
			//                         48                         368-8=360
            for (int freq_offset = LOW_FREQ_INDEX; freq_offset < num_bins - 8; ++freq_offset)
                {
                // There are about 33,000 combinations of time and frquency offsets
				// because we don't know the frequency and there may be time errors.
				// For each of these we find average sync score over sync
                // symbols (m+k = 0-7, 36-43, 72-79)
                int score = 0;    // 32-bit int
                int num_symbols = 0;
                for (int m = 0; m <= 72; m += 36)       // Over all 3 sync time groups of 7
                    {
                    for (int k = 0; k < 7; ++k)         // Over all 7 symbols (in time)
                        {
                        // Check for time boundaries
                        if (time_offset + k + m < 0) continue;
                        if (time_offset + k + m >= num_blocks) break;

                        int offset = ((time_offset + k + m) * 4 + alt) * num_bins + freq_offset;
                        p8 = power + offset;

                        // const uint8_t kCostas_map[7] = { 3,1,4,0,6,5,2 };  i.e., sync_map[]					
                        score += 8 * p8[sync_map[k]] -
                                     p8[0] - p8[1] - p8[2] - p8[3] -
                                     p8[4] - p8[5] - p8[6] - p8[7];
                        /*
                        // Check only the neighbors of the expected symbol frequency- and time-wise
                        int sm = sync_map[k];   // Index of the expected bin
                        if (sm > 0) {
                            // look at one frequency bin lower
                            score += p8[sm] - p8[sm - 1];
                        }
                        if (sm < 7) {
                            // look at one frequency bin higher
                            score += p8[sm] - p8[sm + 1];
                        }
                        if (k > 0) {
                            // look one symbol back in time
                            score += p8[sm] - p8[sm - 4 * num_bins];
                        }
                        if (k < 6) {
                            // look one symbol forward in time
                            score += p8[sm] - p8[sm + 4 * num_bins];
                        }
                        */
                        ++num_symbols;
                        }
                    }
                score /= num_symbols;
				// Todo - can this benefit from doing score in float?
				//Serial.print("num_symbols ");  Serial.println(num_symbols);
				//Serial.print("score ");  Serial.println(score);				
				
                // if(score > max_score) max_score = score;
                if (score < min_score)
                    continue;

                // If the heap is full AND the current candidate is better than
                // the worst in the heap, we remove the worst and make space
                if (heap_size == num_candidates && score > heap[0].score)
                    {
                    heap[0] = heap[heap_size - 1];
                    --heap_size;
                    heapify_down(heap, heap_size);
                    }
                // If there's free space in the heap, we add the current candidate
                if (heap_size < num_candidates)
                    {
                    heap[heap_size].score = score;
                    heap[heap_size].time_offset = time_offset;
                    heap[heap_size].freq_offset = freq_offset;
					heap[heap_size].alt = alt;          // Added for Teensy RSL
                    heap[heap_size].time_sub = alt / 2;
                    heap[heap_size].freq_sub = alt % 2;
					heap[heap_size].syncPower = 0.0f;   // Added for Teensy  RSL
                    ++heap_size;
                    heapify_up(heap, heap_size);
                    }
                }
			}
        } //end of alt loop

 	// For the frequencies and times that match the candidates, go back to the power
	// data and compute the power average over the 3 * 7 sync symbols
	// For 20 candidates this takes about 340 uSec.
	for(int hh=0; hh<heap_size; hh++)
	    {
        // Compute syncPower over sync symbols (m+k = 0-7, 36-43, 72-79)
		float32_t sPower = 0.0f;
        int num_symbols = 0;
        for (int m = 0; m <= 72; m += 36)       // Over all 3 sync time groups of 7
            {
            for (int k = 0; k < 7; ++k)         // Over all 7 symbols (in time)
                {
                // Check for time boundaries
                if (heap[hh].time_offset + k + m < 0) continue;
                if (heap[hh].time_offset + k + m >= num_blocks) break;
                int offset = (heap[hh].time_offset + k + m) * 4 + heap[hh].alt*num_bins
       			         + heap[hh].freq_offset;
                p8 = power + offset;
                sPower += powf(10.0f, 0.05*(float32_t)(p8[sync_map[k]]-136));
                ++num_symbols;
                }
            }
        heap[hh].syncPower = 10.0f*log10f(sPower/(float32_t)num_symbols);  // This is a measure of signal power level in dB
	  }
    return heap_size;
    }

// Compute log likelihood log(p(1) / p(0)) of 174 message bits
// for later use in soft-decision LDPC decoding
// Takes about 28 uSec for each Candidate 
void extract_likelihood(const uint8_t *power, int num_bins,
                        Candidate cand, const uint8_t *code_map,
                        float *log174) {
    int ft8_offset = (cand.time_offset * 4 + cand.time_sub * 2 + cand.freq_sub)
                      *num_bins + cand.freq_offset;
    // Go over FSK tones and skip Costas sync symbols
    const int n_syms = 1;
    // const int n_bits = 3 * n_syms;
    // const int n_tones = (1 << n_bits);
    for (int k = 0; k < FT8_ND; k += n_syms)
        {
        int sym_idx = (k < FT8_ND / 2) ? (k + 7) : (k + 14);
        int bit_idx = 3 * k;
        // Pointer to 8 bins of the current symbol
        const uint8_t *ps = power + (ft8_offset + sym_idx * 4 * num_bins);
        decode_symbol(ps, code_map, bit_idx, log174);
        }
    // Compute the variance of log174
    float sum   = 0;
    float sum2  = 0;
    float inv_n = 1.0f / FT8_N;
    for (int i = 0; i < FT8_N; ++i)
        {
        sum  += log174[i];
        sum2 += log174[i] * log174[i];
        }
    float variance = (sum2 - sum * sum * inv_n) * inv_n;
    // Normalize log174 such that sigma = 2.83 (Why? It's in WSJT-X, ft8b.f90)
    // Seems to be 2.83 = sqrt(8). Experimentally sqrt(16) works better.
    float norm_factor = sqrtf(16.0f / variance);
    for (int i = 0; i < FT8_N; ++i)
        {
        log174[i] *= norm_factor;
        }
    }

static float max2(float a, float b) {
    return (a >= b) ? a : b;
    }

static float max4(float a, float b, float c, float d) {
    return max2(max2(a, b), max2(c, d));
    }

static void heapify_down(Candidate *heap, int heap_size) {
    // heapify from the root down
    int current = 0;
    while (true)
        {
        int largest = current;
        int left = 2 * current + 1;
        int right = left + 1;
        if (left < heap_size && heap[left].score < heap[largest].score)
            largest = left;
        if (right < heap_size && heap[right].score < heap[largest].score)
            largest = right;
        if (largest == current)
            break;
        Candidate tmp = heap[largest];
        heap[largest] = heap[current];
        heap[current] = tmp;
        current = largest;
        }
    }

static void heapify_up(Candidate *heap, int heap_size) {
    // heapify from the last node up
    int current = heap_size - 1;
    while (current > 0)
        {
        int parent = (current - 1)/ 2;
        if (heap[current].score >= heap[parent].score)
            break;
        Candidate tmp = heap[parent];
        heap[parent] = heap[current];
        heap[current] = tmp;
        current = parent;
        }
    }

// Compute unnormalized log likelihood log(p(1)/p(0)) of
// 3 message bits (1 FSK symbol)
static void decode_symbol(const uint8_t *power, const uint8_t *code_map,
                          int bit_idx, float *log174) {
// Cleaned up code for the simple case of n_syms==1
    float s2[8];
    for (int j = 0; j < 8; ++j)
        {
        s2[j] = (float)power[code_map[j]];
        //s2[j] = (float)work_fft_power[offset+code_map[j]];
        }
    log174[bit_idx + 0] = max4(s2[4], s2[5], s2[6], s2[7]) - max4(s2[0], s2[1], s2[2], s2[3]);
    log174[bit_idx + 1] = max4(s2[2], s2[3], s2[6], s2[7]) - max4(s2[0], s2[1], s2[4], s2[5]);
    log174[bit_idx + 2] = max4(s2[1], s2[3], s2[5], s2[7]) - max4(s2[0], s2[2], s2[4], s2[6]);
    }

