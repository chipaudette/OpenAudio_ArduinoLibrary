//   maidenheadR.ino  Bob Larkin 12 Nov 2022
// Built from the K. Goba double precision files.
// Data types float precision, funtion unchanged, names have added 'f'

char letterizeR(int x) {
    return (char) x + 65;
}

// Input lat, lon in degrees; Input grid locator size 4, 6, 8, or 10
// Output grid square string pointer.
char* get_mhf(float32_t lat, float32_t lon, int size) {
    static char locator[11];
    float32_t LON_F[]={20.0f, 2.0f, 0.0833333f, 0.008333f, 0.00034720833f};
    float32_t LAT_F[]={10.0f, 1.0f, 0.0416665f, 0.004166f, 0.00017358333f};
    int i;
    lon += 180.0f;
    lat += 90.0f;

    if (size <= 0 || size > 10) size = 6;
    size/=2; size*=2;

    for (i = 0; i < size/2; i++){
        if (i % 2 == 1) {
            locator[i*2]   = (char)(lon/LON_F[i] + '0');
            locator[i*2+1] = (char)(lat/LAT_F[i] + '0');
        } else {
            locator[i*2]   = letterizeR((int)(lon/LON_F[i]));
            locator[i*2+1] = letterizeR((int)(lat/LAT_F[i]));
        }
        lon = fmod(lon, LON_F[i]);
        lat = fmod(lat, LAT_F[i]);
    }
    locator[i*2]=0;
    return locator;
}

char* complete_mh(char* locator) {
    static char locator2[11] = "LL55LL55LL";
    int len = strlen(locator);
    if (len >= 10) return locator;
    memcpy(locator2, locator, strlen(locator));
    return locator2;
}

// Convert grid square string to longitude
float32_t mh2lonf(char* locator) {
    float32_t field, square, subsquare, extsquare, precsquare;
    int len = strlen(locator);
    if (len > 10) return 0;
    if (len < 10) locator = complete_mh(locator);
    field      = (locator[0] - 'A') * 20.0f;
    square     = (locator[2] - '0') * 2.0f;
    subsquare  = (locator[4] - 'A') / 12.0f;
    extsquare  = (locator[6] - '0') / 120.0f;
    precsquare = (locator[8] - 'A') / 2880.0f;
    return field + square + subsquare + extsquare + precsquare - 180.0f;
}

// Convert grid square string to latitude
float32_t mh2latf(char* locator) {
    float field, square, subsquare, extsquare, precsquare;
    int len = strlen(locator);
    if (len > 10) return 0;
    if (len < 10) locator = complete_mh(locator);
    field      = (locator[1] - 'A') * 10.0f;
    square     = (locator[3] - '0') * 1.0f;
    subsquare  = (locator[5] - 'A') / 24.0f;
    extsquare  = (locator[7] - '0') / 240.0f;
    precsquare = (locator[9] - 'A') / 5760.0f;
    return field + square + subsquare + extsquare + precsquare - 90;
}
