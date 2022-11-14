/*
 *  Was locator.ino
 *  Created on: Nov 4, 2019
 *      Author: user
 */
//   locatorR.ino  Bob Larkin 12 Nov 2022
// Built from the K. Goba double precision files.
// Data types float precision, funtion unchanged, names have added 'f'
// Added azimuth calculations  Bob  14 Nov 2022

const float32_t EARTH_RAD = 6371.0f;  //radius in km
float32_t Latitude, Longitude;

void set_Station_Coordinates(char station[]){
	process_locator(station);
	Station_Latitude = Latitude;
	Station_Longitude = Longitude;
}

float32_t Target_Distancef(char target[]) {
	float32_t targetDistance;
	process_locator(target);
	Target_Latitude = Latitude;
	Target_Longitude = Longitude;
	targetDistance = distancef(Station_Latitude, Station_Longitude,
	                            Target_Latitude, Target_Longitude);
	return targetDistance;
}

// Azimuth added 14 Nov 2022 - This duplicates some of the calculations in distance
// but the number of targets this is run on is small, so won't restructure.
float32_t Target_Azimuthf(char target[]) {
	float32_t targetAz;
	float d2r = 0.017453292f;
	process_locator(target);
	Target_Latitude = Latitude;
	Target_Longitude = Longitude;
    float32_t y = sinf(d2r*(Target_Longitude - Station_Longitude)) * cosf(Target_Latitude*d2r);
    float32_t x = cosf(Station_Latitude*d2r) * sinf(Target_Latitude*d2r) - 
      sinf(Station_Latitude*d2r) * cosf(Target_Latitude*d2r) * cosf(d2r*(Target_Longitude - Station_Longitude));
    targetAz = 57.2957795f*atan2f(y, x);
    if(targetAz<0.0f)
       targetAz += 360.0f;		
	return targetAz;
}

void process_locator(char locator[]) {
	uint8_t A1, A2, N1, N2;
	uint8_t A1_value, A2_value, N1_value, N2_value;
	float32_t Latitude_1, Latitude_2, Latitude_3;
	float32_t Longitude_1, Longitude_2, Longitude_3;
	A1 = locator[0];
	A2 = locator[1];
	N1 = locator[2];
	N2= locator [3];
	A1_value = A1-65;
	A2_value = A2-65;
	N1_value = N1- 48;
	N2_value = N2 - 48;
	Latitude_1 = (float32_t) A2_value * 10.0f;
	Latitude_2 = (float32_t) N2_value;
	Latitude_3 = (11.0f/24.0f + 1.0f/48.0f) - 90.0f;
	Latitude = Latitude_1 + Latitude_2 + Latitude_3;
	Longitude_1 = (float32_t)A1_value * 20.0f;
	Longitude_2 = (float32_t)N1_value * 2.0f;
	Longitude_3 = 11.0f/12.0f +  1.0f/24.0f;
	// global Longitude
	Longitude =  Longitude_1  +  Longitude_2 + Longitude_3 - 180.0f;
}

// distance (km) on earth's surface from point 1 to point 2
float32_t distancef(float32_t lat1, float32_t lon1, float32_t lat2, float32_t lon2) {
    float32_t lat1r = deg2radf(lat1);
    float32_t lon1r = deg2radf(lon1);
    float32_t lat2r = deg2radf(lat2);
    float32_t lon2r = deg2radf(lon2);
    return acos(sinf(lat1r) * sinf(lat2r) +
        cosf(lat1r) * cosf(lat2r) * cosf(lon2r-lon1r)) * EARTH_RAD;
}

// convert degrees to radians (i.e., * PI/180)
float32_t deg2radf(float32_t deg)                    {
    return deg*0.017453292f;
}
