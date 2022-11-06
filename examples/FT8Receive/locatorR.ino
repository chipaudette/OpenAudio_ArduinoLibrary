/*
 * locator.ino
 *
 *  Created on: Nov 4, 2019
 *      Author: user
 */

//#include "locator.h"
//#include <math.h>
//#include "arm_math.h"

// TODO -  Looks like single precision would be adequate - RSL

const double EARTH_RAD = 6371;  //radius in km
//void process_locator(char locator[]);
//double distance(double lat1, double lon1, double lat2, double lon2);
//double deg2rad(double deg);
float Latitude, Longitude;
float Station_Latitude, Station_Longitude;
float Target_Latitude, Target_Longitude;
//float Target_Distance(char target[]);

void set_Station_Coordinates(char station[]){
	process_locator(station);
	Station_Latitude = Latitude;
	Station_Longitude = Longitude;
}

float Target_Distance(char target[]) {
	float targetDistance;
	process_locator(target);
	Target_Latitude = Latitude;
	Target_Longitude = Longitude;
	targetDistance = (float) distance((double)Station_Latitude,(double)Station_Longitude,
	                                  (double)Target_Latitude,(double)Target_Longitude);
	return targetDistance;
}

void process_locator(char locator[]) {
	uint8_t A1, A2, N1, N2;
	uint8_t A1_value, A2_value, N1_value, N2_value;
	float Latitude_1, Latitude_2, Latitude_3;
	float Longitude_1, Longitude_2, Longitude_3;
	A1 = locator[0];
	A2 = locator[1];
	N1 = locator[2];
	N2= locator [3];
	A1_value = A1-65;
	A2_value = A2-65;
	N1_value = N1- 48;
	N2_value = N2 - 48;
	Latitude_1 = (float) A2_value * 10;
	Latitude_2 = (float) N2_value;
	Latitude_3 = (11.0/24.0 + 1.0/48.0) - 90.0;
	Latitude = Latitude_1 + Latitude_2 + Latitude_3;
	Longitude_1 = (float)A1_value * 20.0;
	Longitude_2 = (float)N1_value * 2.0;
	Longitude_3 = 11.0/12.0 +  1.0/24.0;
	Longitude =  Longitude_1  +  Longitude_2 + Longitude_3 - 180.0;
}

// distance (km) on earth's surface from point 1 to point 2
double distance(double lat1, double lon1, double lat2, double lon2) {
    double lat1r = deg2rad(lat1);
    double lon1r = deg2rad(lon1);
    double lat2r = deg2rad(lat2);
    double lon2r = deg2rad(lon2);
    return acos(sin(lat1r) * sin(lat2r)+cos(lat1r) * cos(lat2r) * cos(lon2r-lon1r)) * EARTH_RAD;
}

// convert degrees to radians
double deg2rad(double deg)
{
    return deg * (PI / 180.0);
}
