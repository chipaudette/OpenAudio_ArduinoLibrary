/*
 * decode_ft8R.ino
 * Basically the Goba decode_ft8.h and .c with minor changes for Teensy
 * Arduino use along with the floating point OpenAudio_ArduinoLibrary.
 * Bob Larkin W7PUA, September 2022.
 *
 */

/* Thank you to Kārlis Goba, YL3JG, https://github.com/kgoba/ft8_lib
 * and to Charley Hill, W5BAA, https://github.com/Rotron/Pocket-FT8
 * as well as the all the contributors to the Joe Taylor WSJT project.
 * See "The FT4 and FT8 Communication Protocols," Steve Franks, K9AN,
 * Bill Somerville, G4WJS and Joe Taylor, K1JT, QEX July/August 2020
 * pp 7-17 as well as https://www.physics.princeton.edu/pulsar/K1JT
 */

/*  *****  MIT License  ***

Copyright (c) 2018 Kārlis Goba
Modifications copyright 2022 Bob Larkin W7PUA

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

const int kLDPC_iterations = 10;
const int kMax_candidates = 40;
const int kMax_decoded_messages = 20;
const int kMax_message_length = 20;
const int kMin_score = 40;    // Minimum sync score threshold for candidates
int validate_locator(char locator[]);
int strindex(char s[], char t[]);

typedef struct
    {
    char field1[14];
    char field2[14];
    char field3[7];
    int  freq_hz;
	float32_t  dTime;           // Added Nov 2022
    char decode_time[10];
    int  sync_score;
    int  snr;
    int  distance;
	float32_t azimuth;           // Added Nov 2022
    } Decode;

Decode new_decoded[20];
int    num_calls;  // number of unique calling stations
int    num_call_checks;
int    message_limit = 15;

// Moved out of ft8_decode
float freq_hz;
float32_t dt;
char rtc_string[10];   // print format stuff
int num_decoded;
char noiseType;

int ft8_decode(void) {
   // decoded[] are temporary message stores for searching out duplicates
   // Just use new_decoded[].field1, 2, and 3 ??
   char decoded[kMax_decoded_messages][kMax_message_length];
   const float fsk_dev = 6.25f;    // tone spacing in Hz and symbol rate
   Candidate candidate_list[kMax_candidates];
   float   log174[FT8_N];
   uint8_t plain[FT8_N];
   uint8_t a91[FT8_K_BYTES];

   // Find top candidates by Costas sync score and localize them in time and frequency
   int  num_candidates = find_sync(export_fft_power,
                            ft8_msg_samples, HIGH_FREQ_INDEX, kCostas_map, kMax_candidates,
                            candidate_list, kMin_score);

   // Go over candidates and attempt to decode messages
   num_decoded = 0;
   for (int idx = 0; idx < num_candidates; ++idx) {
      Candidate cand = candidate_list[idx];
      freq_hz  = (cand.freq_offset + cand.freq_sub / 2.0f) * fsk_dev;
      extract_likelihood(export_fft_power, HIGH_FREQ_INDEX, cand, kGray_map, log174);
      // bp_decode() produces better decodes, uses way less memory

      int n_errors = 0;
      bp_decode(log174, kLDPC_iterations, plain, &n_errors);
      if (n_errors > 0)
         continue;
      // Extract payload + CRC (first K bits)
      pack_bits(plain, FT8_K, a91);
      // Extract CRC and check it
      uint16_t chksum = ((a91[9] & 0x07) << 11) | (a91[10] << 3) | (a91[11] >> 5);
      a91[9] &= 0xF8;
      a91[10] = 0;
      a91[11] = 0;
      uint16_t chksum2 = crc(a91, 96 - 14);
      if (chksum != chksum2)   continue;    // Decode failed, around to start of loop again
      char message[kMax_message_length];
      char field1[14];
      char field2[14];
      char field3[7];
      int rc = unpack77_fields(a91, field1, field2, field3);
      if (rc < 0) continue;   // Decode failed
      sprintf(message,"%s %s %s ",field1, field2, field3);

      // Check for duplicate messages (TODO: use hashing)
      bool found = false;
	  int indexFound = -1;
      for (int i = 0; i < num_decoded; ++i)
         {
         if (0 == strcmp(decoded[i], message))
            {
            found = true;
			indexFound = i;
            break;
            }
         }
      float distance;

      getTeensy3Time();     // MOVE OUT OF LOOP????   <<<<<<<<<<<<<<<<<<<<<<<<<<
      sprintf( rtc_string, " %2i:%2i:%2i ", hour(), minute(), second() );
	  for(int kk=1; kk<9; kk++)
	     {
	     if(rtc_string[kk] == ' ')
		    rtc_string[kk] = '0';
		 }
      dt = 0.16f*(float)cand.time_offset;

      // Revised procedure to support snr measurement.  Now keep the strongest
	  // of duplicates.  Bob Larkin  Nov 2020.
      if(found)   // Keep the one with bigger score, put into [
	     {
#ifdef DEBUG_D
		 Serial.print("cand.score = "); Serial.print(cand.score);
         Serial.print(" candidate_list[indexFound].score = "); Serial.println(candidate_list[indexFound].score);
#endif
	     if(cand.score > candidate_list[indexFound].score)
		    {
#ifdef DEBUG_D
	        Serial.print("Replace "); Serial.print(indexFound);
	        Serial.print(" with "); Serial.println(num_decoded);
	        Serial.print("Old score = "); Serial.print(candidate_list[indexFound].score);
	        Serial.print(" New score = "); Serial.println(cand.score);
#endif
			// .decode_time and field1, 2 and 3 are correct already
            new_decoded[indexFound].sync_score = cand.score;
            new_decoded[indexFound].freq_hz = (int)freq_hz;
			new_decoded[indexFound].dTime = dt;
            if(FT8noisePeakAveRatio < 100.0f) // No big signals
			   {
               new_decoded[indexFound].snr =
       		      cand.syncPower - (float32_t)FT8noisePwrDBIntL + 51.0f;
			   noiseType = 'L';
			   }
			else
			   {
	           new_decoded[indexFound].snr =
       		      cand.syncPower - (float32_t)FT8noisePwrDBIntH - 13.0f;
			   noiseType = 'H';
			   }
            if(new_decoded[indexFound].snr < -21)
			   new_decoded[indexFound].snr = -21;   // It is never lower!!
		    // num_decoded is left unchanged
            }
		 }
      // !found means this is a new entry to add to decoded list
      else        // Duplicate not found
	    {
		if(num_decoded < kMax_decoded_messages)  // Make new entry if room
         {
         if(strlen(message) < kMax_message_length)
            {
#ifdef DEBUG_D
	        Serial.print("Enter "); Serial.print(num_decoded);
	        Serial.print(rtc_string);
	        Serial.print("msg: ");
	        Serial.print(message);
#endif
            strcpy(decoded[num_decoded], message);
            new_decoded[num_decoded].sync_score = cand.score;
            new_decoded[num_decoded].freq_hz = (int)freq_hz;
			new_decoded[num_decoded].dTime = dt;
            strcpy(new_decoded[num_decoded].field1, field1);
            strcpy(new_decoded[num_decoded].field2, field2);
            strcpy(new_decoded[num_decoded].field3, field3);
            strcpy(new_decoded[num_decoded].decode_time, rtc_string);
            if(FT8noisePeakAveRatio < 100.0f) // No big signals for quiet timing
               new_decoded[num_decoded].snr =
       		      cand.syncPower - (float32_t)FT8noisePwrDBIntL + 51.0f;
			else
	           new_decoded[num_decoded].snr =
       		      cand.syncPower - (float32_t)FT8noisePwrDBIntH - 13.0f;
            if(new_decoded[num_decoded].snr < -21)
			   new_decoded[num_decoded].snr = -21;   // It is never lower!!
#ifdef DEBUG_D
            Serial.print(" dt="); Serial.print(0.16f*(float)cand.time_offset); // secs
            Serial.print(" freq="); Serial.print((int)freq_hz);
            Serial.print(" snr="); Serial.println(new_decoded[num_decoded].snr);
#endif
#ifdef DEBUG_N
            Serial.print("syncPower, noiseL, noiseH, ratio { "); Serial.print(cand.syncPower, 1);
            Serial.print(" "); Serial.print(FT8noisePwrDBIntL);
            Serial.print(" "); Serial.print(FT8noisePwrDBIntH);
            Serial.print(" "); Serial.print(FT8noisePeakAveRatio, 1);
            Serial.println(" }");
#endif
            char Target_Locator[] = "    ";
            strcpy(Target_Locator, new_decoded[num_decoded].field3);
            if (validate_locator(Target_Locator)  == 1)
               {
               distance = Target_Distancef(Target_Locator);
               new_decoded[num_decoded].distance = (int)(0.5f + distance);
			   new_decoded[num_decoded].azimuth = Target_Azimuthf(Target_Locator);
               }
            else
               {
               new_decoded[num_decoded].distance = 0;
               }
            ++num_decoded;
            }
         }  // End, there was room for new entry
		}  // End, duplicate not found
      }  //End of big decode loop
	  noiseMeasured = false;    // Global flag for Process_DSP_R
      //printFT8Received();  // Move to main INO
   return num_decoded;
   }

void printFT8Received(void)  {
	char message2[kMax_message_length];
	Serial.print("FT8 Received TRansmissions, Noise Type=");
	Serial.println(noiseType);
	for(int kk=0; kk<num_decoded; kk++)
	    {
        Serial.print(kk);
        Serial.print(rtc_string);
        Serial.print(" msg: ");
		sprintf(message2,"%s %s %s ",new_decoded[kk].field1,
		    new_decoded[kk].field2, new_decoded[kk].field3);
        Serial.print(message2);
        Serial.print(" dt="); Serial.print(new_decoded[kk].dTime); // secs
        Serial.print(" freq="); Serial.print(new_decoded[kk].freq_hz);
        Serial.print(" snr="); Serial.print(new_decoded[kk].snr);
		if(new_decoded[kk].distance > 0)
		   {
		   Serial.print(" km="); Serial.print(new_decoded[kk].distance);
		   Serial.print(" az="); Serial.println(new_decoded[kk].azimuth, 1);
		   }
	    else
		   Serial.println("");
        }
	Serial.println("");
	}

int validate_locator(char locator[]) {

  uint8_t A1, A2, N1, N2;
  uint8_t test = 0;

  A1 = locator[0] - 65;
  A2 = locator[1] - 65;
  N1 = locator[2] - 48;
  N2= locator [3] - 48;

  if (A1 >= 0 && A1 <= 17) test++;
  if (A2 > 0 && A2 < 17) test++; //block RR73 Artic and Anartica
  if (N1 >= 0 && N1 <= 9) test++;
  if (N2 >= 0 && N2 <= 9) test++;

  if (test == 4) return 1;
  else
  return 0;
  }

int strindex(char s[],char t[])  {
   int i,j,k, result;
   result = -1;
   for(i=0; s[i]!='\0'; i++)
      {
      for(j=i,k=0; t[k]!='\0' && s[j]==t[k]; j++,k++)
         ;
      if(k>0 && t[k] == '\0')
         result = i;
      }
   return result;
   }
