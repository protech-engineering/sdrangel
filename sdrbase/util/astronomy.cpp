///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>               //
// Copyright (C) 2004 Rutherford Appleton Laboratory                             //
// Copyright (C) 2012 Science and Technology Facilities Council.                 //
// Copyright (C) 2013-2021, NumFOCUS Foundation.                                 //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include <QDateTime>
#include <QDebug>

#include "util/units.h"
#include "astronomy.h"

const double Astronomy::m_boltzmann = 1.380649e-23; //!< Boltzmann constant (k) in J/K
const double Astronomy::m_hydrogenLineFrequency = 1420405751.768; //!< Hydrogen line frequency in Hz
const double Astronomy::m_hydroxylLineFrequency = 1612231000.0; //!< Strongest hydroxyl line frequency in Hz
const double Astronomy::m_deuteriumLineFrequency = 327384000.0; //!< Deuterium line frequency in Hz - https://www.nap.edu/read/21774/chapter/11
const double Astronomy::m_speedOfLight = 299792458.0; // Speed of light in m/s
const double Astronomy::m_hydrogenMass = 1.674e-27; // Mass of hydrogen atom in kg (same as proton)

// Function prototypes
static void palRefz(double zu, double refa, double refb, double *zr);
static void palRefco (double hm, double tdk, double pmb, double rh,
                double wl, double phi, double tlr, double eps,
                double *refa, double *refb);
static void palRefro( double zobs, double hm, double tdk, double pmb,
               double rh, double wl, double phi, double tlr,
               double eps, double * ref);
static void palEpv( double date, double ph[3], double vh[3],
             double pb[3], double vb[3] );
static int eraEpv00(double date1, double date2,
             double pvh[2][3], double pvb[2][3]);


// Calculate Julian date (days since January 1, 4713 BC) from Gregorian calendar date
double Astronomy::julianDate(int year, int month, int day, int hours, int minutes, int seconds)
{
    int julian_day;
    double julian_date;

    // From: https://en.wikipedia.org/wiki/Julian_day
    julian_day = (1461 * (year + 4800 + (month - 14)/12))/4 +(367 * (month - 2 - 12 * ((month - 14)/12)))/12 - (3 * ((year + 4900 + (month - 14)/12)/100))/4 + day - 32075;
    julian_date = julian_day + (hours/24.0 - 0.5) + minutes/(24.0*60.0) + seconds/(24.0*60.0*60.0);

    return julian_date;
}

// Calculate Julian date
double Astronomy::julianDate(QDateTime dt)
{
    QDateTime utc = dt.toUTC();
    QDate date = utc.date();
    QTime time = utc.time();
    return julianDate(date.year(), date.month(), date.day(), time.hour(), time.minute(), time.second());
}

// Calculate modified Julian date
double Astronomy::modifiedJulianDate(QDateTime dt)
{
    return Astronomy::julianDate(dt) - 2400000.5;
}

// Convert Julian date to QDateTime
QDateTime Astronomy::julianDateToDateTime(double jd)
{
    return QDateTime::fromSecsSinceEpoch((jd - 2440587.5) * 24.0*60.0*60.0);
}

// Get Julian date of J2000 Epoch
double Astronomy::jd_j2000(void)
{
    static double j2000 = 0.0;

    if (j2000 == 0.0) {
        j2000 = julianDate(2000, 1, 1, 12, 0, 0);
    }
    return j2000;
}

// Get Julian date of B1950 Epoch
double Astronomy::jd_b1950(void)
{
    static double b1950 = 0.0;

    if (b1950 == 0.0) {
        b1950 = julianDate(1949, 12, 31, 22, 9, 0);
    }
    return b1950;
}

// Get Julian date of current time Epoch
double Astronomy::jd_now(void)
{
    time_t system_time;
    struct tm *utc_time;

    // Get current time in seconds since Unix Epoch (1970)
    time(&system_time);
    // Convert to UTC (GMT)
    utc_time = gmtime(&system_time);

    // Convert to Julian date
    return julianDate(utc_time->tm_year + 1900, utc_time->tm_mon + 1, utc_time->tm_mday,
                      utc_time->tm_hour, utc_time->tm_min, utc_time->tm_sec);
}

// Precess a RA/DEC between two given Epochs
RADec Astronomy::precess(RADec rd_in, double jd_from, double jd_to)
{
    RADec rd_out;
    double x, y, z;
    double xp, yp, zp;
    double ra_rad, dec_rad;
    double rot[3][3];      // [row][col]
    double ra_deg;

    double days_per_century = 36524.219878;
    double t0 = (jd_from - jd_b1950())/days_per_century; // Tropical centuries since B1950.0
    double t = (jd_to - jd_from)/days_per_century;     // Tropical centuries from starting epoch to ending epoch

    // From: https://www.cloudynights.com/topic/561254-ra-dec-epoch-conversion/
    rot[0][0] = 1.0 - ((29696.0 + 26.0*t0)*t*t - 13.0*t*t*t)*.00000001;
    rot[1][0] = ((2234941.0 + 1355.0*t0)*t - 676.0*t*t + 221.0*t*t*t)*.00000001;
    rot[2][0] = ((971690.0 - 414.0*t0)*t + 207.0*t*t + 96.0*t*t*t)*.00000001;
    rot[0][1] = -rot[1][0];
    rot[1][1] = 1.0 - ((24975.0 + 30.0*t0)*t*t - 15.0*t*t*t)*.00000001;
    rot[2][1] = -((10858.0 + 2.0*t0)*t*t)*.00000001;
    rot[0][2] = -rot[2][0];
    rot[1][2] = rot[2][1];
    rot[2][2] = 1.0 - ((4721.0 - 4.0*t0)*t*t)*.00000001;

    // Hours to degrees
    ra_deg = rd_in.ra*(360.0/24.0);

    // Convert to rectangular coordinates
    ra_rad = Units::degreesToRadians(ra_deg);
    dec_rad = Units::degreesToRadians(rd_in.dec);
    x = cos(ra_rad) * cos(dec_rad);
    y = sin(ra_rad) * cos(dec_rad);
    z = sin(dec_rad);

    // Rotate
    xp = rot[0][0]*x + rot[0][1]*y + rot[0][2]*z;
    yp = rot[1][0]*x + rot[1][1]*y + rot[1][2]*z;
    zp = rot[2][0]*x + rot[2][1]*y + rot[2][2]*z;

    // Convert back to spherical coordinates
    rd_out.ra = Units::radiansToDegrees(atan(yp/xp));
    if (xp < 0.0) {
        rd_out.ra += 180.0;
    } else if ((yp < 0) && (xp > 0)) {
        rd_out.ra += 360.0;
    }
    rd_out.dec = Units::radiansToDegrees(asin(zp));

    // Degrees to hours
    rd_out.ra /= (360.0/24.0);

    return rd_out;
}

// Calculate local mean sidereal time (LMST) in degrees
double Astronomy::localSiderealTime(QDateTime dateTime, double longitude)
{
    double jd = julianDate(dateTime);

    double d = (jd - jd_j2000()); // Days since J2000 epoch (including fraction)
    double f = fmod(jd, 1.0); // Fractional part is decimal days
    double ut = (f+0.5)*24.0; // Universal time in decimal hours

    // https://astronomy.stackexchange.com/questions/24859/local-sidereal-time
    // 100.46 is offset for GMST at 0h UT on 1 Jan 2000
    // 0.985647 is number of degrees per day over 360 the Earth rotates in one solar day
    // Approx to 0.3 arcseconds
    return fmod(100.46 + 0.985647 * d + longitude + (360/24) * ut, 360.0);
}

// Convert from J2000 right ascension (decimal hours) and declination (decimal degrees) to altitude and azimuth, for location (decimal degrees) and time
AzAlt Astronomy::raDecToAzAlt(RADec rd, double latitude, double longitude, QDateTime dt, bool j2000)
{
    AzAlt aa;
    double ra_deg; // Right ascension in degrees
    double lst_deg; // Local sidereal time in degrees
    double ha; // Hour angle
    double a, az;
    double dec_rad, lat_rad, ha_rad, alt_rad; // Corresponding variables as radians

    double jd = julianDate(dt);

    // Precess RA/DEC from J2000 Epoch to desired (typically current) Epoch
    if (j2000)
        rd = precess(rd, jd_j2000(), jd);

    // Calculate local mean sidereal time (LMST) in degrees
    // https://astronomy.stackexchange.com/questions/24859/local-sidereal-time
    // 100.46 is offset for GMST at 0h UT on 1 Jan 2000
    // 0.985647 is number of degrees per day over 360 the Earth rotates in one solar day
    // Approx to 0.3 arcseconds
    lst_deg = Astronomy::localSiderealTime(dt, longitude);

    // Convert right ascension from hours to degrees
    ra_deg = rd.ra * (360.0/24.0);

    // Calculate hour angle
    ha = fmod(lst_deg - ra_deg, 360.0);

    // Convert degrees to radians
    dec_rad = Units::degreesToRadians(rd.dec);
    lat_rad = Units::degreesToRadians(latitude);
    ha_rad = Units::degreesToRadians(ha);

    // Calculate altitude and azimuth - no correction for atmospheric refraction
    // From: http://www.convertalot.com/celestial_horizon_co-ordinates_calculator.html
    alt_rad = asin(sin(dec_rad)*sin(lat_rad) + cos(dec_rad)*cos(lat_rad)*cos(ha_rad));
    a = Units::radiansToDegrees(acos((sin(dec_rad)-sin(alt_rad)*sin(lat_rad)) / (cos(alt_rad)*cos(lat_rad))));
    if (sin(ha_rad) < 0.0) {
        az = a;
    } else {
        az = 360.0 - a;
    }

    aa.alt = Units::radiansToDegrees(alt_rad);
    aa.az = az;
    return aa;
}

// Convert from altitude and azimuth, for location (decimal degrees) and time, to Jnow right ascension (decimal hours) and declination (decimal degrees)
// See: http://jonvoisey.net/blog/2018/07/data-converting-alt-az-to-ra-dec-example/ appears to be wrong for HA
// See: https://www.cambridge.org/ca/academic/subjects/physics/amateur-and-popular-astronomy/practical-astronomy-your-calculator-or-spreadsheet-4th-edition?format=PB HOREQ.xls
RADec Astronomy::azAltToRaDec(AzAlt aa, double latitude, double longitude, QDateTime dt)
{
    RADec rd;
    double lst_deg; // Local sidereal time
    double ha_rad, ha_deg; // Hour angle
    double alt_rad, az_rad, lat_rad, dec_rad; // Corresponding variables as radians

    // Calculate local mean sidereal time (LMST) in degrees (see raDecToAzAlt)
    lst_deg = Astronomy::localSiderealTime(dt, longitude);

    // Convert degrees to radians
    alt_rad = Units::degreesToRadians(aa.alt);
    az_rad = Units::degreesToRadians(aa.az);
    lat_rad = Units::degreesToRadians(latitude);

    // Calculate declination
    double sin_dec_rad = sin(lat_rad)*sin(alt_rad)+cos(lat_rad)*cos(alt_rad)*cos(az_rad);
    dec_rad = asin(sin_dec_rad);

    // Calculate hour angle
    if (0)
    {
        double quotient = (sin(alt_rad)-sin(lat_rad)*sin(dec_rad))/(cos(lat_rad)*cos(dec_rad));
        // At extreme altitudes, we seem to get small numerical errors that causes values to be out of range, so clip to [-1,1]
        if (quotient < -1.0) {
            ha_rad = acos(-1.0);
        } else if (quotient > 1.0) {
            ha_rad = acos(1.0);
        } else {
            ha_rad = acos(quotient);
        }
    }
    else
    {
        double y = -cos(alt_rad)*cos(lat_rad)*sin(az_rad);
        double x = sin(alt_rad)-sin(lat_rad)*sin_dec_rad;
        ha_rad = atan2(y, x);
    }

    // Convert radians to degrees
    rd.dec = Units::radiansToDegrees(dec_rad);
    ha_deg = Units::radiansToDegrees(ha_rad);

    // Calculate right ascension in decimal hours
    rd.ra = modulo((lst_deg - ha_deg) / (360.0/24.0), 24.0);

    return rd;
}

// https://ntrs.nasa.gov/api/citations/19670030005/downloads/19670030005.pdf
// X85 is positive Southward
// Y85 is positive Eastward
// X30 is positive Eastward
// Y30 is positive Northward
// atan2 range is (-pi,pi], we want az (0,360], so need to add pi
void Astronomy::azAltToXY85(AzAlt aa, double& x, double& y)
{
    if (aa.alt == 90.0)
    {
        x = 0.0;
        y = 0.0;
        //qDebug() << "azAltToXY85" << aa.az << aa.alt << x << y;
        return;
    }
    double az = aa.az;
    double el = aa.alt;
    if (az >= 360.0) {
        az -= 360.0;
    }
    if (el > 90.0)
    {
        el = 180.0 - el;
        if (az >= 180.0) {
            az = az - 180.0;
        } else {
            az = az + 180.0;
        }
    }
    double azr = Units::degreesToRadians(az);
    double elr = Units::degreesToRadians(el);
    y = Units::radiansToDegrees(asin(cos(elr) * sin(azr)));
    if (az == 0.0)
    {
        // cot(0) == Inf
        if ((az == 90.0) || (az == 270.0)) {
            x = 0.0;
        } else if ((az > 90.0f) && (az < 270.0)) {
            x = 90.0;
        } else {
            x = -90.0;
        }
    }
    else
    {
        // cot(x)=1/tan(x)=cos(x)/sin(x)
        x = Units::radiansToDegrees(atan(-(cos(elr)/sin(elr)) * cos(azr)));
    }

    //qDebug() << "azAltToXY85" << aa.az << aa.alt << x << y;
}

void Astronomy::azAltToXY30(AzAlt aa, double& x, double& y)
{
    if (aa.alt == 90.0)
    {
        x = 0.0;
        y = 0.0;
        //qDebug() << "azAltToXY30" << aa.az << aa.alt << x << y;
        return;
    }
    double az = aa.az;
    double el = aa.alt;
    if (az >= 360.0) {
        az -= 360.0;
    }
    if (el > 90.0)
    {
        el = 180.0 - el;
        if (az >= 180.0) {
            az = az - 180.0;
        } else {
            az = az + 180.0;
        }
    }
    double azr = Units::degreesToRadians(az);
    double elr = Units::degreesToRadians(el);
    y = Units::radiansToDegrees(asin(cos(elr) * cos(azr)));
    if (el == 0.0)
    {
        if ((az == 0.0) || (az == 180.0)) {
            x = 0.0;
        } else if ((az >= 0.0f) && (az <= 180.0)) {
            x = 90.0;
        } else {
            x = -90.0;
        }
    }
    else
    {
        x = Units::radiansToDegrees(atan((cos(elr)/sin(elr)) * sin(azr)));
    }
    //qDebug() << "azAltToXY30" << aa.az << aa.alt << x << y;
}

AzAlt Astronomy::xy85ToAzAlt(double x, double y)
{
    AzAlt aa;
    if ((x == 0.0) && (y == 0.0))
    {
        aa.az = 0.0;
        aa.alt = 90.0;
        //qDebug() << "xy85ToAzAlt" << x << y << aa.az << aa.alt;
        return aa;
    }
    double xr = Units::degreesToRadians(x);
    double yr = Units::degreesToRadians(y);
    double elr = asin(cos(yr) * cos(xr));
    double azr;
    if (x == 0.0)
    {
        // 1/sin(x) == Inf
        azr = y >= 0.0 ? M_PI/2.0 : 2.0*M_PI*3.0/4.0;
    }
    else if (y == 90.0)
    {
        // tan(90) == Inf
        azr = M_PI/2.0;
    }
    else if (y == -90.0)
    {
        // tan(-90) == Inf
        azr = 2.0*M_PI*3.0/4.0;
    }
    else
    {
         // atan2(y,x) = atan(y/x)
         azr = atan2(-tan(yr), sin(xr)) + M_PI;
    }
    aa.az = Units::radiansToDegrees(azr);
    aa.alt = Units::radiansToDegrees(elr);
    //qDebug() << "xy85ToAzAlt" << x << y << aa.az << aa.alt;
    return aa;
}

AzAlt Astronomy::xy30ToAzAlt(double x, double y)
{
    AzAlt aa;
    if ((x == 0.0) && (y == 0.0))
    {
        aa.az = 0.0;
        aa.alt = 90.0;
        //qDebug() << "xy30ToAzAlt" << x << y << aa.az << aa.alt;
        return aa;
    }
    double xr = Units::degreesToRadians(x);
    double yr = Units::degreesToRadians(y);
    double elr = asin(cos(yr) * cos(xr));
    double azr;
    if (y == 0.0)
    {
        // cot(0) == Inf
        azr = x >= 0.0 ? M_PI/2.0 : 2.0*M_PI*3.0/4.0;
    }
    else if (y == 90.0)
    {
        // tan(90) == Inf
        azr = 0.0;
    }
    else if (y == -90.0)
    {
        // tan(-90) == Inf
        azr = M_PI;
    }
    else
    {
        azr = atan2(sin(xr), tan(yr));
        if (azr < 0.0) {
            azr += 2.0*M_PI;
        }
    }
    aa.az = Units::radiansToDegrees(azr);
    aa.alt = Units::radiansToDegrees(elr);
    //qDebug() << "xy30ToAzAlt" << x << y << aa.az << aa.alt;
    return aa;
}

// Needs to work for negative a
double Astronomy::modulo(double a, double b)
{
    return a - b * floor(a/b);
}

// Calculate azimuth and altitude angles to the sun from the given latitude and longitude at the given time
// Refer to:
// https://en.wikipedia.org/wiki/Position_of_the_Sun
// https://www.aa.quae.nl/en/reken/zonpositie.html
// Said to be accurate to .01 degree (36") for dates between 1950 and 2050
// although we use slightly more accurate constants and an extra term in the equation
// of centre from the second link
// For an alternative, see: http://www.psa.es/sdg/sunpos.htm
// Most accurate algorithm is supposedly: https://midcdmz.nrel.gov/spa/
void Astronomy::sunPosition(AzAlt& aa, RADec& rd, double latitude, double longitude, QDateTime dt)
{
    double jd = julianDate(dt);
    double n = (jd - jd_j2000()); // Days since J2000 epoch (including fraction)

    double l = 280.461 + 0.9856474 * n; // Mean longitude of the Sun, corrected for the aberration of light
    double g = 357.5291 + 0.98560028 * n; // Mean anomaly of the Sun - how far around orbit from perihlion, in degrees

    l = modulo(l, 360.0);
    g = modulo(g, 360.0);

    double gr = Units::degreesToRadians(g);

    double la = l + 1.9148 * sin(gr) + 0.0200 * sin(2.0*gr) + 0.0003 * sin(3.0*gr); // Ecliptic longitude (Ecliptic latitude b set to 0)

    // Convert la, b=0, which give the position of the Sun in the ecliptic coordinate system, to
    // equatorial coordinates

    double e = 23.4393 - 3.563E-7 * n; // Obliquity of the ecliptic - tilt of Earth's axis of rotation

    double er = Units::degreesToRadians(e);
    double lr = Units::degreesToRadians(la);

    double a = atan2(cos(er) * sin(lr), cos(lr)); // Right ascension, radians
    double d = asin(sin(er) * sin(lr)); // Declination, radians

    rd.ra = modulo(Units::radiansToDegrees(a), 360.0) / (360.0/24.0); // Convert to hours
    rd.dec = Units::radiansToDegrees(d); // Convert to degrees

    aa = raDecToAzAlt(rd, latitude, longitude, dt, false);
}

double Astronomy::moonDays(QDateTime dt)
{
    QDateTime utc = dt.toUTC();
    QDate date = utc.date();
    QTime time = utc.time();

    int y = date.year();
    int m = date.month();
    int D = date.day();
    int d = 367 * y - 7 * (y + (m+9)/12) / 4 - 3 * ((y + (m-9)/7) / 100 + 1) / 4 + 275*m/9 + D - 730515;

    return d + time.hour()/24.0 + time.minute()/(24.0*60.0) + time.second()/(24.0*60.0*60.0);
}

// Calculate azimuth and altitude angles to the moon from the given latitude and longitude at the given time
// Refer to: https://stjarnhimlen.se/comp/ppcomp.html
// Accurate to 4 arcminute
void Astronomy::moonPosition(AzAlt& aa, RADec& rd, double latitude, double longitude, QDateTime dt)
{
    double d = moonDays(dt);

    double ecl = Units::degreesToRadians(23.4393 - 3.563E-7 * d); // Obliquity of the ecliptic - tilt of Earth's axis of rotation

    // Orbital elements for the Sun
    double ws = Units::degreesToRadians(282.9404 + 4.70935E-5 * d);
    double Ms = Units::degreesToRadians(356.0470 + 0.9856002585 * d);

    // Orbital elements for the Moon
    double Nm = Units::degreesToRadians(125.1228 - 0.0529538083 * d);           // longitude of the ascending node
    double im = Units::degreesToRadians(5.1454);                                // inclination to the ecliptic (plane of the Earth's orbit)
    double wm = Units::degreesToRadians(318.0634 + 0.1643573223 * d);           // argument of perihelion
    double am = 60.2666;                                                         // (Earth radii) semi-major axis, or mean distance from Sun
    double em = 0.054900;      // ecm                                                 // eccentricity (0=circle, 0-1=ellipse, 1=parabola)
    double Mm = Units::degreesToRadians(115.3654 + 13.0649929509 * d);          // mean anomaly (0 at perihelion; increases uniformly with time), degrees

    double Em = Mm + em * sin(Mm) * (1.0 + em * cos(Mm)); // Eccentric anomaly in radians

    double xv = am * (cos(Em) - em);
    double yv = am * (sqrt(1.0 - em*em) * sin(Em));

    double vm = atan2(yv, xv); // True anomaly
    double rm = sqrt(xv*xv + yv*yv); // Distance

    // Compute position in space (for the Moon, this is geocentric)
    double xh = rm * (cos(Nm) * cos(vm+wm) - sin(Nm) * sin(vm+wm) * cos(im));
    double yh = rm * (sin(Nm) * cos(vm+wm) + cos(Nm) * sin(vm+wm) * cos(im));
    double zh = rm * (sin(vm+wm) * sin(im));

    // Convert to ecliptic longitude and latitude
    double lonecl = atan2(yh, xh);
    double latecl = atan2(zh, sqrt(xh*xh+yh*yh));

    // Perturbations of the Moon

    double Ls = Ms + ws;                // Mean Longitude of the Sun  (Ns=0)
    double Lm = Mm + wm + Nm;           // Mean longitude of the Moon
    double D = Lm - Ls;                 // Mean elongation of the Moon
    double F = Lm - Nm;                 // Argument of latitude for the Moon

    double dlon;
    dlon = -1.274 * sin(Mm - 2*D);           // (the Evection)
    dlon += +0.658 * sin(2*D);               // (the Variation)
    dlon += -0.186 * sin(Ms);                // (the Yearly Equation)
    dlon += -0.059 * sin(2*Mm - 2*D);
    dlon += -0.057 * sin(Mm - 2*D + Ms);
    dlon += +0.053 * sin(Mm + 2*D);
    dlon += +0.046 * sin(2*D - Ms);
    dlon += +0.041 * sin(Mm - Ms);
    dlon += -0.035 * sin(D);                 // (the Parallactic Equation)
    dlon += -0.031 * sin(Mm + Ms);
    dlon += -0.015 * sin(2*F - 2*D);
    dlon += +0.011 * sin(Mm - 4*D);

    double dlat;
    dlat = -0.173 * sin(F - 2*D);
    dlat += -0.055 * sin(Mm - F - 2*D);
    dlat += -0.046 * sin(Mm + F - 2*D);
    dlat += +0.033 * sin(F + 2*D);
    dlat += +0.017 * sin(2*Mm + F);

    lonecl += Units::degreesToRadians(dlon);
    latecl += Units::degreesToRadians(dlat);

    rm += -0.58 * cos(Mm - 2*D);
    rm += -0.46 * cos(2*D);

    // Convert perturbed
    xh = rm * cos(lonecl) * cos(latecl);
    yh = rm * sin(lonecl) * cos(latecl);
    zh = rm               * sin(latecl);

    // Convert to geocentric coordinates (already the case for the Moon)
    double xg = xh;
    double yg = yh;
    double zg = zh;

    // Convert to equatorial coordinates
    double xe = xg;
    double ye = yg * cos(ecl) - zg * sin(ecl);
    double ze = yg * sin(ecl) + zg * cos(ecl);

    // Compute right ascension and declination
    double ra = atan2(ye, xe);
    double dec = atan2(ze, sqrt(xe*xe+ye*ye));

    rd.ra = modulo(Units::radiansToDegrees(ra), 360.0) / (360.0/24.0); // Convert to hours
    rd.dec = Units::radiansToDegrees(dec); // Convert to degrees

    // Convert from geocentric to topocentric
    double mpar = asin(1/rm);

    double lat = Units::degreesToRadians(latitude);
    double gclat = Units::degreesToRadians(latitude - 0.1924 * sin(2.0 * lat));
    double rho = 0.99833 + 0.00167 * cos(2*lat);

    QTime time = dt.toUTC().time();
    double UT = time.hour() + time.minute()/60.0 + time.second()/(60.0*60.0);

    double GMST0 = Units::radiansToDegrees(Ls)/15.0 + 12; // In hours
    double GMST = GMST0 + UT;
    double LST = GMST + longitude/15.0;

    double HA = Units::degreesToRadians(LST*15.0 - Units::radiansToDegrees(ra));    // Hour angle in radians
    double g = atan(tan(gclat) / cos(HA));

    double topRA = ra - mpar * rho * cos(gclat) * sin(HA) / cos(dec);
    double topDec;
    if (g != 0.0)
        topDec = dec - mpar * rho * sin(gclat) * sin(g - dec) / sin(g);
    else
        topDec = dec - mpar * rho * sin(-dec) * cos(HA);

    rd.ra = modulo(Units::radiansToDegrees(topRA), 360.0) / (360.0/24.0); // Convert to hours
    rd.dec = Units::radiansToDegrees(topDec); // Convert to degrees
    aa = raDecToAzAlt(rd, latitude, longitude, dt, false);
}

// Calculated adjustment to altitude angle from true to apparent due to atmospheric refraction using
// Saemundsson's formula (which is used by Stellarium and is primarily just for
// optical wavelengths)
// See: https://en.wikipedia.org/wiki/Atmospheric_refraction#Calculating_refraction
// Alt is in degrees. 90 = Zenith gives a factor of 0.
// Pressure in millibars
// Temperature in Celsius
// We divide by 60.0 to get a value in degrees (as original formula is in arcminutes)
double Astronomy::refractionSaemundsson(double alt, double pressure, double temperature)
{
    double pt = (pressure/1010.0) * (283.0/(273.0+temperature));

    return pt * (1.02 / tan(Units::degreesToRadians(alt+10.3/(alt+5.11))) + 0.0019279) / 60.0;
}

// Calculated adjustment to altitude angle from true to apparent due to atmospheric refraction using
// code from Starlink Positional Astronomy Library. This is more accurate for
// radio than Saemundsson's formula, but also more complex.
// See: https://github.com/Starlink/pal
// Alt is in degrees. 90 = Zenith gives a factor of 0.
// Pressure in millibars
// Temperature in Celsius
// Humdity in %
// Frequency in Hertz
// Latitude in decimal degrees
// HeightAboveSeaLevel in metres
// Temperature lapse rate in K/km
double Astronomy::refractionPAL(double alt, double pressure, double temperature, double humidity, double frequency,
                                double latitude, double heightAboveSeaLevel, double temperatureLapseRate)
{
    double tdk = Units::celsiusToKelvin(temperature);   // Ambient temperature at the observer (K)
    double wl = (299792458.0/frequency)*1000000.0;      // Wavelength in micrometres
    double rh = humidity/100.0;                         // Relative humidity in range 0-1
    double phi = Units::degreesToRadians(latitude);     // Latitude of the observer (radian, astronomical)
    double tlr = temperatureLapseRate/1000.0;           // Temperature lapse rate in the troposphere (K/metre)
    double refa, refb;
    double z = 90.0-alt;
    double zu = Units::degreesToRadians(z);
    double zr;

    palRefco(heightAboveSeaLevel, tdk, pressure, rh,
                wl, phi, tlr, 1E-10,
                &refa, &refb);
    palRefz(zu, refa, refb, &zr);

    return z-Units::radiansToDegrees(zr);
}

double Astronomy::lstAndRAToLongitude(double lst, double raHours)
{
    double longitude = lst - (raHours * 15.0); // Convert hours to degrees
    if (longitude < -180.0)
        longitude += 360.0;
    else if (longitude > 180.0)
        longitude -= 360.0;
    return -longitude; // East positive
}

// Return right ascension and declination of North Galactic Pole in J2000 Epoch
// http://www.lsc-group.phys.uwm.edu/lal/lsd/node1777.html
void Astronomy::northGalacticPoleJ2000(double& ra, double& dec)
{
    ra = 192.8594813/15.0;
    dec = 27.1282511;
}

// Convert from equatorial to Galactic coordinates, J2000 Epoch
void Astronomy::equatorialToGalactic(double ra, double dec, double& l, double& b)
{
    const double raRad = Units::degreesToRadians(ra * 15.0);
    const double decRad = Units::degreesToRadians(dec);

    // Calculate RA and dec for North Galactic pole, J2000
    double ngpRa, ngpDec;
    northGalacticPoleJ2000(ngpRa, ngpDec);
    const double ngpRaRad = Units::degreesToRadians(ngpRa * 15.0);
    const double ngpDecRad = Units::degreesToRadians(ngpDec);

    // Calculate galactic longitude in radians
    double bRad = asin(sin(ngpDecRad)*sin(decRad)+cos(ngpDecRad)*cos(decRad)*cos(raRad - ngpRaRad));

    // Calculate galactic latitiude in radians
    double lRad = atan2(sin(decRad)-sin(bRad)*sin(ngpDecRad), cos(decRad)*cos(ngpDecRad)*sin(raRad - ngpRaRad));

    // Ascending node of the galactic plane in degrees
    double lAscend = 33.0;

    // Convert to degrees in range -90,90 and 0,360
    b = Units::radiansToDegrees(bRad);
    l = Units::radiansToDegrees(lRad) + lAscend;
    if (l < 0.0) {
        l += 360.0;
    }
    if (l > 360.0) {
        l -= 360.0;
    }
}

// Convert from Galactic to equatorial coordinates, J2000 Epoch
void Astronomy::galacticToEquatorial(double l, double b, double& ra, double& dec)
{
    const double lRad = Units::degreesToRadians(l);
    const double bRad = Units::degreesToRadians(b);

    // Calculate RA and dec for North Galactic pole, J2000
    double ngpRa, ngpDec;
    northGalacticPoleJ2000(ngpRa, ngpDec);
    const double ngpRaRad = Units::degreesToRadians(ngpRa * 15.0);
    const double ngpDecRad = Units::degreesToRadians(ngpDec);

    // Galactic longitude of North Celestial Pole
    const double ncpLRad = Units::degreesToRadians(122.93129);

    // Calculate declination in radians
    double decRad = asin(sin(bRad)*sin(ngpDecRad)+cos(bRad)*cos(ngpDecRad)*cos(lRad - ncpLRad));

    // Calculate right ascension in radians
    double y = sin(lRad-ncpLRad);
    double x = cos(lRad-ncpLRad)*sin(ngpDecRad)-tan(bRad)*cos(ngpDecRad);
    double raRad = atan2(y, x) + (ngpRaRad - M_PI);

    // Convert to degrees
    dec = Units::radiansToDegrees(decRad);
    ra = Units::radiansToDegrees(raRad) / 15.0;
    if (ra < 0.0) {
        ra += 24.0;
    } else if (ra >= 24.0) {
        ra -= 24.0;
    }
}

// Calculate velocity in m/s given a Doppler shift from frequency f0 to frequency f in Hz
// Non-relativistic "radio definition"
// + velocity is approaching
double Astronomy::dopplerToVelocity(double f, double f0)
{
    return m_speedOfLight * f / f0 - m_speedOfLight;
}

// Calculate Doppler shift in Hz given a velocity in m/s
double Astronomy::velocityToDoppler(double v, double f0)
{
    return f0 * (v + m_speedOfLight) / m_speedOfLight;
}

// Get velocity (km/s) in a given direction due to the Earth's rotation
// Adapted from palRverot
double Astronomy::earthRotationVelocity(RADec rd, double latitude, double longitude, QDateTime dt)
{
    const double earthSpeed = 0.4655; // km/s (Earth's circumference / seconds in one sidereal day)
    double latRad = Units::degreesToRadians(latitude);
    double raRad = Units::degreesToRadians(rd.ra * (360.0/24.0));
    double decRad = Units::degreesToRadians(rd.dec);
    double st = Astronomy::localSiderealTime(dt, longitude);
    double a = Units::degreesToRadians(st) - raRad;
    return -earthSpeed * cos(latRad) * sin(a) * cos(decRad);
}

// Get velocity (km/s) in a given direction due to the Earth's orbit around the Sun in barcycentric reference frame
double Astronomy::earthOrbitVelocityBCRS(RADec rd, QDateTime dt)
{
    double ph[3];
    double vh[3];
    double pb[3];
    double vb[3];

    double date = Astronomy::modifiedJulianDate(dt);

    // Get Earth position and velocity
    palEpv(date, ph, vh, pb, vb);

    // Convert from AU/day to km/s
    double vel[3];
    for (int i = 0; i < 3; i++) {
        vel[i] = vb[i] * 1.731e3;
    }

    // Equatorial to cartesian vector
    double raRad = Units::degreesToRadians(rd.ra * (360.0/24.0));
    double decRad = Units::degreesToRadians(rd.dec);
    double cp = cos(decRad);
    double vd[3];
    vd[0] = cos(raRad) * cp;
    vd[1] = sin(raRad) * cp;
    vd[2] = sin(decRad);

    // Calculate component of velocity along target direction via dot product
    return vel[0] * vd[0] + vel[1] * vd[1] + vel[2] * vd[2];
}

// Get velocity (km/s) in a given direction due to the Sun's motion w.r.t. Local Standard of Rest
// Adapted from palRvlsrk
// See: https://advlabwiki.johnshopkins.edu/images/Vlsr.pdf
double Astronomy::sunVelocityLSRK(RADec rd)
{
    // Solar motion - 20 km/s towards RA 18h Dec +30d (1900)
    // expressed as J2000.0 equatorial cartesian vector
    // Some other s/w uses 18h03m50.29s, +30:00:16.8 (J2000) -> {0.01934,-17.31968,10.001411}
    const double va[3] = {0.29000, -17.31726, 10.00141};
    double vb[3];

    // Equatorial to cartesian vector
    double raRad = Units::degreesToRadians(rd.ra * (360.0/24.0));
    double decRad = Units::degreesToRadians(rd.dec);
    double cp = cos(decRad);
    vb[0] = cos(raRad) * cp;
    vb[1] = sin(raRad) * cp;
    vb[2] = sin(decRad);

    // Calculate component of velocity along target direction via dot product
    return va[0] * vb[0] + va[1] * vb[1] + va[2] * vb[2];
}

// Get velocity (km/s) of Earth based observer in a given direction due to the Sun's and Earth's motion w.r.t. Local Standard of Rest
double Astronomy::observerVelocityLSRK(RADec rd, double latitude, double longitude, QDateTime dt)
{
    double vRot = Astronomy::earthRotationVelocity(rd, latitude, longitude, dt);
    double vOrbit = Astronomy::earthOrbitVelocityBCRS(rd, dt);
    double vSun = Astronomy::sunVelocityLSRK(rd);
    return vRot + vOrbit + vSun;
}

// Calculate sunrise and sunset time
// From: https://en.wikipedia.org/wiki/Sunrise_equation
// Probably accurate to within a couple of minutes
void Astronomy::sunrise(QDate date, double latitude, double longitude, QDateTime& rise, QDateTime& set)
{
    // Calculate Julian day
    double n = std::ceil(Astronomy::julianDate(QDateTime(date, QTime(0, 0, 0))) - 2451545.0 + (69.184 / 86400.0));

    // Mean solar time
    double jStar = n - longitude / 360.0;

    // Solar mean anomaly
    double m = Astronomy::modulo(357.5291 + 0.98560028 * jStar, 360.0);
    double mRad = Units::degreesToRadians(m);

    // Equation of the center
    double c = 1.9148 * sin(mRad) + 0.02 * sin(2.0 * mRad) + 0.0003 * sin(3 * mRad);

    // Ecliptic longitude
    double lambda = Astronomy::modulo(m + c + 180.0 + 102.9372, 360.0);
    double lambdaRad = Units::degreesToRadians(lambda);

    // Solar transit
    double jTransit = 2451545.0 + jStar + 0.0053 * sin(mRad) - 0.0069 * sin(2.0 * lambdaRad);

    // Declination of the Sun
    const double tilt = 23.4397;
    const double tiltRad = Units::degreesToRadians(tilt);
    double sunDecRad = asin(sin(lambdaRad) * sin(tiltRad));

    // Hour angle
    double latitudeRad = Units::degreesToRadians(latitude);
    double omega0Rad = acos((sin(Units::degreesToRadians(-0.833)) - sin(latitudeRad) * sin(sunDecRad)) / (cos(latitudeRad) *  cos(sunDecRad)));
    double omega0 = Units::radiansToDegrees(omega0Rad);

    // Rise and set times
    double jRise = jTransit - omega0 / 360.0;
    double jSet = jTransit + omega0 / 360.0;

    rise = Astronomy::julianDateToDateTime(jRise);
    set =  Astronomy::julianDateToDateTime(jSet);
}

// Calculate thermal noise power for a given temperature in Kelvin and bandwidth in Hz
double Astronomy::noisePowerdBm(double temp, double bw)
{
    return 10.0 * std::log10(m_boltzmann * temp * bw) + 30.0;
}

// Calculate noise temperature for a given power in dBm and bandwidth in Hz
double Astronomy::noiseTemp(double dBm, double bw)
{
    return std::pow(10.0, ((dBm - 30.0) / 10.0)) / (m_boltzmann * bw);
}

// The following functions are from Starlink Positional Astronomy Library
// https://github.com/Starlink/pal

/* Pi */
static const double PAL__DPI = 3.1415926535897932384626433832795028841971693993751;

/* 2Pi */
static const double PAL__D2PI = 6.2831853071795864769252867665590057683943387987502;

/* pi/180:  degrees to radians */
static const double PAL__DD2R = 0.017453292519943295769236907684886127134428718885417;

/* Radians to degrees */
static const double PAL__DR2D = 57.295779513082320876798154814105170332405472466564;

/* Start of SLA modified Julian date epoch */
static const double PAL__MJD0 = 2400000.5;

/* DMAX(A,B) - return maximum value - evaluates arguments multiple times */
#define DMAX(A,B) ((A) > (B) ? (A) : (B) )

/* DMIN(A,B) - return minimum value - evaluates arguments multiple times */
#define DMIN(A,B) ((A) < (B) ? (A) : (B) )

// Normalize angle into range +/- pi
double palDrange( double angle ) {
   double result = fmod( angle, PAL__D2PI );
   if( result > PAL__DPI ) {
      result -= PAL__D2PI;
   } else if( result < -PAL__DPI ) {
      result += PAL__D2PI;
   }
   return result;
}

//  Calculate stratosphere parameters
static void pal1Atms ( double rt, double tt, double dnt, double gamal,
                double r, double * dn, double * rdndr ) {

  double b;
  double w;

  b = gamal / tt;
  w = (dnt - 1.0) * exp( -b * (r-rt) );
  *dn = 1.0 + w;
  *rdndr = -r * b * w;

}

// Calculate troposphere parameters
static void pal1Atmt ( double r0, double t0, double alpha, double gamm2,
                double delm2, double c1, double c2, double c3, double c4,
                double c5, double c6, double r,
                double *t, double *dn, double *rdndr ) {

  double tt0;
  double tt0gm2;
  double tt0dm2;

  *t = DMAX( DMIN( t0 - alpha*(r-r0), 320.0), 100.0 );
  tt0 = *t / t0;
  tt0gm2 = pow( tt0, gamm2 );
  tt0dm2 = pow( tt0, delm2 );
  *dn = 1.0 + ( c1 * tt0gm2 - ( c2 - c5 / *t ) * tt0dm2 ) * tt0;
  *rdndr = r * ( -c3 * tt0gm2 + ( c4 - c6 / tt0 ) * tt0dm2 );

}

// Adjust unrefracted zenith distance
static void palRefz ( double zu, double refa, double refb, double *zr ) {

  /* Constants */

  /* Largest usable ZD (deg) */
  const double D93 = 93.0;

  /* ZD at which one model hands over to the other (radians) */
  const double Z83 = 83.0 * PAL__DD2R;

  /* coefficients for high ZD model (used beyond ZD 83 deg) */
  const double C1 = +0.55445;
  const double C2 = -0.01133;
  const double C3 = +0.00202;
  const double C4 = +0.28385;
  const double C5 = +0.02390;

  /* High-ZD-model prefiction (deg) for that point */
  const double REF83 = (C1+C2*7.0+C3*49.0)/(1.0+C4*7.0+C5*49.0);

  double zu1,zl,s,c,t,tsq,tcu,ref,e,e2;

  /*  perform calculations for zu or 83 deg, whichever is smaller */
  zu1 = DMIN(zu,Z83);

  /*  functions of ZD */
  zl = zu1;
  s = sin(zl);
  c = cos(zl);
  t = s/c;
  tsq = t*t;
  tcu = t*tsq;

  /*  refracted zd (mathematically to better than 1 mas at 70 deg) */
  zl = zl-(refa*t+refb*tcu)/(1.0+(refa+3.0*refb*tsq)/(c*c));

  /*  further iteration */
  s = sin(zl);
  c = cos(zl);
  t = s/c;
  tsq = t*t;
  tcu = t*tsq;
  ref = zu1-zl+
    (zl-zu1+refa*t+refb*tcu)/(1.0+(refa+3.0*refb*tsq)/(c*c));

  /*  special handling for large zu */
  if (zu > zu1) {
    e = 90.0-DMIN(D93,zu*PAL__DR2D);
    e2 = e*e;
    ref = (ref/REF83)*(C1+C2*e+C3*e2)/(1.0+C4*e+C5*e2);
  }

  /*  return refracted zd */
  *zr = zu-ref;

}

// Determine constants in atmospheric refraction model
static void palRefco ( double hm, double tdk, double pmb, double rh,
                double wl, double phi, double tlr, double eps,
                double *refa, double *refb ) {

  double r1, r2;

  /*  Sample zenith distances: arctan(1) and arctan(4) */
  const double ATN1 = 0.7853981633974483;
  const double ATN4 = 1.325817663668033;

  /*  Determine refraction for the two sample zenith distances */
  palRefro(ATN1,hm,tdk,pmb,rh,wl,phi,tlr,eps,&r1);
  palRefro(ATN4,hm,tdk,pmb,rh,wl,phi,tlr,eps,&r2);

  /*  Solve for refraction constants */
  *refa = (64.0*r1-r2)/60.0;
  *refb = (r2-4.0*r1)/60.0;

}

// Atmospheric refraction for radio and optical/IR wavelengths
static void palRefro( double zobs, double hm, double tdk, double pmb,
               double rh, double wl, double phi, double tlr,
               double eps, double * ref ) {

  /*
   *  Fixed parameters
   */

  /*  93 degrees in radians */
  const double D93 = 1.623156204;
  /*  Universal gas constant */
  const double GCR = 8314.32;
  /*  Molecular weight of dry air */
  const double DMD = 28.9644;
  /*  Molecular weight of water vapour */
  const double DMW = 18.0152;
  /*  Mean Earth radius (metre) */
  const double S = 6378120.;
  /*  Exponent of temperature dependence of water vapour pressure */
  const double DELTA = 18.36;
  /*  Height of tropopause (metre) */
  const double HT = 11000.;
  /*  Upper limit for refractive effects (metre) */
  const double HS = 80000.;
  /*  Numerical integration: maximum number of strips. */
  const int ISMAX=16384l;

  /* Local variables */
  int is, k, n, i, j;

  int optic, loop; /* booleans */

  double zobs1,zobs2,hmok,tdkok,pmbok,rhok,wlok,alpha,
    tol,wlsq,gb,a,gamal,gamma,gamm2,delm2,
    tdc,psat,pwo,w,
    c1,c2,c3,c4,c5,c6,r0,tempo,dn0,rdndr0,sk0,f0,
    rt,tt,dnt,rdndrt,sine,zt,ft,dnts,rdndrp,zts,fts,
    rs,dns,rdndrs,zs,fs,refold,z0,zrange,fb,ff,fo,fe,
    h,r,sz,rg,dr,tg,dn,rdndr,t,f,refp,reft;

  /*  The refraction integrand */
#define refi(DN,RDNDR) RDNDR/(DN+RDNDR)

  /*  Transform ZOBS into the normal range. */
  zobs1 = palDrange(zobs);
  zobs2 = DMIN(fabs(zobs1),D93);

  /*  keep other arguments within safe bounds. */
  hmok = DMIN(DMAX(hm,-1e3),HS);
  tdkok = DMIN(DMAX(tdk,100.0),500.0);
  pmbok = DMIN(DMAX(pmb,0.0),10000.0);
  rhok = DMIN(DMAX(rh,0.0),1.0);
  wlok = DMAX(wl,0.1);
  alpha = DMIN(DMAX(fabs(tlr),0.001),0.01);

  /*  tolerance for iteration. */
  tol = DMIN(DMAX(fabs(eps),1e-12),0.1)/2.0;

  /*  decide whether optical/ir or radio case - switch at 100 microns. */
  optic = wlok < 100.0;

  /*  set up model atmosphere parameters defined at the observer. */
  wlsq = wlok*wlok;
  gb = 9.784*(1.0-0.0026*cos(phi+phi)-0.00000028*hmok);
  if (optic) {
    a = (287.6155+(1.62887+0.01360/wlsq)/wlsq) * 273.15e-6/1013.25;
  } else {
    a = 77.6890e-6;
  }
  gamal = (gb*DMD)/GCR;
  gamma = gamal/alpha;
  gamm2 = gamma-2.0;
  delm2 = DELTA-2.0;
  tdc = tdkok-273.15;
  psat = pow(10.0,(0.7859+0.03477*tdc)/(1.0+0.00412*tdc)) *
    (1.0+pmbok*(4.5e-6+6.0e-10*tdc*tdc));
  if (pmbok > 0.0) {
    pwo = rhok*psat/(1.0-(1.0-rhok)*psat/pmbok);
  } else {
    pwo = 0.0;
  }
  w = pwo*(1.0-DMW/DMD)*gamma/(DELTA-gamma);
  c1 = a*(pmbok+w)/tdkok;
  if (optic) {
    c2 = (a*w+11.2684e-6*pwo)/tdkok;
  } else {
    c2 = (a*w+6.3938e-6*pwo)/tdkok;
  }
  c3 = (gamma-1.0)*alpha*c1/tdkok;
  c4 = (DELTA-1.0)*alpha*c2/tdkok;
  if (optic) {
    c5 = 0.0;
    c6 = 0.0;
  } else {
    c5 = 375463e-6*pwo/tdkok;
    c6 = c5*delm2*alpha/(tdkok*tdkok);
  }

  /*  conditions at the observer. */
  r0 = S+hmok;
  pal1Atmt(r0,tdkok,alpha,gamm2,delm2,c1,c2,c3,c4,c5,c6,
           r0,&tempo,&dn0,&rdndr0);
  sk0 = dn0*r0*sin(zobs2);
  f0 = refi(dn0,rdndr0);

  /*  conditions in the troposphere at the tropopause. */
  rt = S+DMAX(HT,hmok);
  pal1Atmt(r0,tdkok,alpha,gamm2,delm2,c1,c2,c3,c4,c5,c6,
           rt,&tt,&dnt,&rdndrt);
  sine = sk0/(rt*dnt);
  zt = atan2(sine,sqrt(DMAX(1.0-sine*sine,0.0)));
  ft = refi(dnt,rdndrt);

  /*  conditions in the stratosphere at the tropopause. */
  pal1Atms(rt,tt,dnt,gamal,rt,&dnts,&rdndrp);
  sine = sk0/(rt*dnts);
  zts = atan2(sine,sqrt(DMAX(1.0-sine*sine,0.0)));
  fts = refi(dnts,rdndrp);

  /*  conditions at the stratosphere limit. */
  rs = S+HS;
  pal1Atms(rt,tt,dnt,gamal,rs,&dns,&rdndrs);
  sine = sk0/(rs*dns);
  zs = atan2(sine,sqrt(DMAX(1.0-sine*sine,0.0)));
  fs = refi(dns,rdndrs);

  /*  variable initialization to avoid compiler warning. */
  reft = 0.0;

  /*  integrate the refraction integral in two parts;  first in the
   *  troposphere (k=1), then in the stratosphere (k=2). */

  for (k=1; k<=2; k++) {

    /*     initialize previous refraction to ensure at least two iterations. */
    refold = 1.0;

    /*     start off with 8 strips. */
    is = 8;

    /*     start z, z range, and start and end values. */
    if (k==1) {
      z0 = zobs2;
      zrange = zt-z0;
      fb = f0;
      ff = ft;
    } else {
      z0 = zts;
      zrange = zs-z0;
      fb = fts;
      ff = fs;
    }

    /*     sums of odd and even values. */
    fo = 0.0;
    fe = 0.0;

    /*     first time through the loop we have to do every point. */
    n = 1;

    /*     start of iteration loop (terminates at specified precision). */
    loop = 1;
    while (loop) {

      /*        strip width. */
      h = zrange/((double)is);

      /*        initialize distance from earth centre for quadrature pass. */
      if (k == 1) {
        r = r0;
      } else {
        r = rt;
      }

      /*        one pass (no need to compute evens after first time). */
      for (i=1; i<is; i+=n) {

              /*           sine of observed zenith distance. */
        sz = sin(z0+h*(double)(i));

        /*           find r (to the nearest metre, maximum four iterations). */
        if (sz > 1e-20) {
          w = sk0/sz;
          rg = r;
          dr = 1.0e6;
          j = 0;
          while ( fabs(dr) > 1.0 && j < 4 ) {
            j++;
            if (k==1) {
              pal1Atmt(r0,tdkok,alpha,gamm2,delm2,
                       c1,c2,c3,c4,c5,c6,rg,&tg,&dn,&rdndr);
            } else {
              pal1Atms(rt,tt,dnt,gamal,rg,&dn,&rdndr);
            }
            dr = (rg*dn-w)/(dn+rdndr);
            rg = rg-dr;
          }
          r = rg;
        }

        /*           find the refractive index and integrand at r. */
        if (k==1) {
          pal1Atmt(r0,tdkok,alpha,gamm2,delm2,
                   c1,c2,c3,c4,c5,c6,r,&t,&dn,&rdndr);
        } else {
          pal1Atms(rt,tt,dnt,gamal,r,&dn,&rdndr);
        }
        f = refi(dn,rdndr);

        /*           accumulate odd and (first time only) even values. */
        if (n==1 && i%2 == 0) {
          fe += f;
        } else {
          fo += f;
        }
      }

      /*        evaluate the integrand using simpson's rule. */
      refp = h*(fb+4.0*fo+2.0*fe+ff)/3.0;

      /*        has the required precision been achieved (or can't be)? */
      if (fabs(refp-refold) > tol && is < ISMAX) {

        /*           no: prepare for next iteration.*/

        /*           save current value for convergence test. */
        refold = refp;

        /*           double the number of strips. */
        is += is;

        /*           sum of all current values = sum of next pass's even values. */
        fe += fo;

        /*           prepare for new odd values. */
        fo = 0.0;

        /*           skip even values next time. */
        n = 2;
      } else {

        /*           yes: save troposphere component and terminate the loop. */
        if (k==1) reft = refp;
        loop = 0;
      }
    }
  }

  /*  result. */
  *ref = reft+refp;
  if (zobs1 < 0.0) *ref = -(*ref);

}

// Earth position and velocity with respect to the BCRS
static void palEpv( double date, double ph[3], double vh[3],
             double pb[3], double vb[3] ) {

  int i;
  double pvh[2][3];
  double pvb[2][3];

  eraEpv00( PAL__MJD0, date, pvh, pvb );

  /* Copy into output arrays */
  for (i=0; i<3; i++) {
    ph[i] = pvh[0][i];
    vh[i] = pvh[1][i];
    pb[i] = pvb[0][i];
    vb[i] = pvb[1][i];
  }

}

// The following functions are from ERFA (Essential Routines for Fundamental Astronomy)
// https://github.com/liberfa/erfa.git

/* Days per Julian year */
#define ERFA_DJY (365.25)

/* Reference epoch (J2000.0), Julian Date */
#define ERFA_DJ00 (2451545.0)

static int eraEpv00(double date1, double date2,
             double pvh[2][3], double pvb[2][3])
/*
**  - - - - - - - - -
**   e r a E p v 0 0
**  - - - - - - - - -
**
**  Earth position and velocity, heliocentric and barycentric, with
**  respect to the Barycentric Celestial Reference System.
**
**  Given:
**     date1,date2  double        TDB date (Note 1)
**
**  Returned:
**     pvh          double[2][3]  heliocentric Earth position/velocity
**     pvb          double[2][3]  barycentric Earth position/velocity
**
**  Returned (function value):
**                  int           status: 0 = OK
**                                       +1 = warning: date outside
**                                            the range 1900-2100 AD
**
**  Notes:
**
**  1) The TDB date date1+date2 is a Julian Date, apportioned in any
**     convenient way between the two arguments.  For example,
**     JD(TDB)=2450123.7 could be expressed in any of these ways, among
**     others:
**
**            date1          date2
**
**         2450123.7           0.0       (JD method)
**         2451545.0       -1421.3       (J2000 method)
**         2400000.5       50123.2       (MJD method)
**         2450123.5           0.2       (date & time method)
**
**     The JD method is the most natural and convenient to use in cases
**     where the loss of several decimal digits of resolution is
**     acceptable.  The J2000 method is best matched to the way the
**     argument is handled internally and will deliver the optimum
**     resolution.  The MJD method and the date & time methods are both
**     good compromises between resolution and convenience.  However,
**     the accuracy of the result is more likely to be limited by the
**     algorithm itself than the way the date has been expressed.
**
**     n.b. TT can be used instead of TDB in most applications.
**
**  2) On return, the arrays pvh and pvb contain the following:
**
**        pvh[0][0]  x       }
**        pvh[0][1]  y       } heliocentric position, au
**        pvh[0][2]  z       }
**
**        pvh[1][0]  xdot    }
**        pvh[1][1]  ydot    } heliocentric velocity, au/d
**        pvh[1][2]  zdot    }
**
**        pvb[0][0]  x       }
**        pvb[0][1]  y       } barycentric position, au
**        pvb[0][2]  z       }
**
**        pvb[1][0]  xdot    }
**        pvb[1][1]  ydot    } barycentric velocity, au/d
**        pvb[1][2]  zdot    }
**
**     The vectors are with respect to the Barycentric Celestial
**     Reference System.  The time unit is one day in TDB.
**
**  3) The function is a SIMPLIFIED SOLUTION from the planetary theory
**     VSOP2000 (X. Moisson, P. Bretagnon, 2001, Celes. Mechanics &
**     Dyn. Astron., 80, 3/4, 205-213) and is an adaptation of original
**     Fortran code supplied by P. Bretagnon (private comm., 2000).
**
**  4) Comparisons over the time span 1900-2100 with this simplified
**     solution and the JPL DE405 ephemeris give the following results:
**
**                                RMS    max
**           Heliocentric:
**              position error    3.7   11.2   km
**              velocity error    1.4    5.0   mm/s
**
**           Barycentric:
**              position error    4.6   13.4   km
**              velocity error    1.4    4.9   mm/s
**
**     Comparisons with the JPL DE406 ephemeris show that by 1800 and
**     2200 the position errors are approximately double their 1900-2100
**     size.  By 1500 and 2500 the deterioration is a factor of 10 and
**     by 1000 and 3000 a factor of 60.  The velocity accuracy falls off
**     at about half that rate.
**
**  5) It is permissible to use the same array for pvh and pvb, which
**     will receive the barycentric values.
**
**  This revision:  2021 May 11
**
**  Copyright (C) 2013-2021, NumFOCUS Foundation.
**  Derived, with permission, from the SOFA library.  See notes at end of file.
*/
{
/*
** Matrix elements for orienting the analytical model to DE405.
**
** The corresponding Euler angles are:
**
**                       d  '  "
**   1st rotation    -  23 26 21.4091 about the x-axis  (obliquity)
**   2nd rotation    +         0.0475 about the z-axis  (RA offset)
**
** These were obtained empirically, by comparisons with DE405 over
** 1900-2100.
*/
   static const double am12 =  0.000000211284,
                       am13 = -0.000000091603,
                       am21 = -0.000000230286,
                       am22 =  0.917482137087,
                       am23 = -0.397776982902,
                       am32 =  0.397776982902,
                       am33 =  0.917482137087;

/*
** ----------------------
** Ephemeris Coefficients
** ----------------------
**
** The ephemeris consists of harmonic terms for predicting (i) the Sun
** to Earth vector and (ii) the Solar-System-barycenter to Sun vector
** respectively.  The coefficients are stored in arrays which, although
** 1-demensional, contain groups of three.  Each triplet of
** coefficients is the amplitude, phase and frequency for one term in
** the model, and each array contains the number of terms called for by
** the model.
**
** There are eighteen such arrays, named as follows:
**
**     array         model      power of T      component
**
**      e0x      Sun-to-Earth        0              x
**      e0y      Sun-to-Earth        0              y
**      e0z      Sun-to-Earth        0              z
**
**      e1x      Sun-to-Earth        1              x
**      e1y      Sun-to-Earth        1              y
**      e1z      Sun-to-Earth        1              z
**
**      e2x      Sun-to-Earth        2              x
**      e2y      Sun-to-Earth        2              y
**      e2z      Sun-to-Earth        2              z
**
**      s0x      SSB-to-Sun          0              x
**      s0y      SSB-to-Sun          0              y
**      s0z      SSB-to-Sun          0              z
**
**      s1x      SSB-to-Sun          1              x
**      s1y      SSB-to-Sun          1              y
**      s1z      SSB-to-Sun          1              z
**
**      s2x      SSB-to-Sun          2              x
**      s2y      SSB-to-Sun          2              y
**      s2z      SSB-to-Sun          2              z
*/

/* Sun-to-Earth, T^0, X */
   static const double e0x[] = {
      0.9998292878132e+00, 0.1753485171504e+01, 0.6283075850446e+01,
      0.8352579567414e-02, 0.1710344404582e+01, 0.1256615170089e+02,
      0.5611445335148e-02, 0.0000000000000e+00, 0.0000000000000e+00,
      0.1046664295572e-03, 0.1667225416770e+01, 0.1884922755134e+02,
      0.3110842534677e-04, 0.6687513390251e+00, 0.8399684731857e+02,
      0.2552413503550e-04, 0.5830637358413e+00, 0.5296909721118e+00,
      0.2137207845781e-04, 0.1092330954011e+01, 0.1577343543434e+01,
      0.1680240182951e-04, 0.4955366134987e+00, 0.6279552690824e+01,
      0.1679012370795e-04, 0.6153014091901e+01, 0.6286599010068e+01,
      0.1445526946777e-04, 0.3472744100492e+01, 0.2352866153506e+01,

      0.1091038246184e-04, 0.3689845786119e+01, 0.5223693906222e+01,
      0.9344399733932e-05, 0.6073934645672e+01, 0.1203646072878e+02,
      0.8993182910652e-05, 0.3175705249069e+01, 0.1021328554739e+02,
      0.5665546034116e-05, 0.2152484672246e+01, 0.1059381944224e+01,
      0.6844146703035e-05, 0.1306964099750e+01, 0.5753384878334e+01,
      0.7346610905565e-05, 0.4354980070466e+01, 0.3981490189893e+00,
      0.6815396474414e-05, 0.2218229211267e+01, 0.4705732307012e+01,
      0.6112787253053e-05, 0.5384788425458e+01, 0.6812766822558e+01,
      0.4518120711239e-05, 0.6087604012291e+01, 0.5884926831456e+01,
      0.4521963430706e-05, 0.1279424524906e+01, 0.6256777527156e+01,

      0.4497426764085e-05, 0.5369129144266e+01, 0.6309374173736e+01,
      0.4062190566959e-05, 0.5436473303367e+00, 0.6681224869435e+01,
      0.5412193480192e-05, 0.7867838528395e+00, 0.7755226100720e+00,
      0.5469839049386e-05, 0.1461440311134e+01, 0.1414349524433e+02,
      0.5205264083477e-05, 0.4432944696116e+01, 0.7860419393880e+01,
      0.2149759935455e-05, 0.4502237496846e+01, 0.1150676975667e+02,
      0.2279109618501e-05, 0.1239441308815e+01, 0.7058598460518e+01,
      0.2259282939683e-05, 0.3272430985331e+01, 0.4694002934110e+01,
      0.2558950271319e-05, 0.2265471086404e+01, 0.1216800268190e+02,
      0.2561581447555e-05, 0.1454740653245e+01, 0.7099330490126e+00,

      0.1781441115440e-05, 0.2962068630206e+01, 0.7962980379786e+00,
      0.1612005874644e-05, 0.1473255041006e+01, 0.5486777812467e+01,
      0.1818630667105e-05, 0.3743903293447e+00, 0.6283008715021e+01,
      0.1818601377529e-05, 0.6274174354554e+01, 0.6283142985870e+01,
      0.1554475925257e-05, 0.1624110906816e+01, 0.2513230340178e+02,
      0.2090948029241e-05, 0.5852052276256e+01, 0.1179062909082e+02,
      0.2000176345460e-05, 0.4072093298513e+01, 0.1778984560711e+02,
      0.1289535917759e-05, 0.5217019331069e+01, 0.7079373888424e+01,
      0.1281135307881e-05, 0.4802054538934e+01, 0.3738761453707e+01,
      0.1518229005692e-05, 0.8691914742502e+00, 0.2132990797783e+00,

      0.9450128579027e-06, 0.4601859529950e+01, 0.1097707878456e+02,
      0.7781119494996e-06, 0.1844352816694e+01, 0.8827390247185e+01,
      0.7733407759912e-06, 0.3582790154750e+01, 0.5507553240374e+01,
      0.7350644318120e-06, 0.2695277788230e+01, 0.1589072916335e+01,
      0.6535928827023e-06, 0.3651327986142e+01, 0.1176985366291e+02,
      0.6324624183656e-06, 0.2241302375862e+01, 0.6262300422539e+01,
      0.6298565300557e-06, 0.4407122406081e+01, 0.6303851278352e+01,
      0.8587037089179e-06, 0.3024307223119e+01, 0.1672837615881e+03,
      0.8299954491035e-06, 0.6192539428237e+01, 0.3340612434717e+01,
      0.6311263503401e-06, 0.2014758795416e+01, 0.7113454667900e-02,

      0.6005646745452e-06, 0.3399500503397e+01, 0.4136910472696e+01,
      0.7917715109929e-06, 0.2493386877837e+01, 0.6069776770667e+01,
      0.7556958099685e-06, 0.4159491740143e+01, 0.6496374930224e+01,
      0.6773228244949e-06, 0.4034162934230e+01, 0.9437762937313e+01,
      0.5370708577847e-06, 0.1562219163734e+01, 0.1194447056968e+01,
      0.5710804266203e-06, 0.2662730803386e+01, 0.6282095334605e+01,
      0.5709824583726e-06, 0.3985828430833e+01, 0.6284056366286e+01,
      0.5143950896447e-06, 0.1308144688689e+01, 0.6290189305114e+01,
      0.5088010604546e-06, 0.5352817214804e+01, 0.6275962395778e+01,
      0.4960369085172e-06, 0.2644267922349e+01, 0.6127655567643e+01,

      0.4803137891183e-06, 0.4008844192080e+01, 0.6438496133249e+01,
      0.5731747768225e-06, 0.3794550174597e+01, 0.3154687086868e+01,
      0.4735947960579e-06, 0.6107118308982e+01, 0.3128388763578e+01,
      0.4808348796625e-06, 0.4771458618163e+01, 0.8018209333619e+00,
      0.4115073743137e-06, 0.3327111335159e+01, 0.8429241228195e+01,
      0.5230575889287e-06, 0.5305708551694e+01, 0.1336797263425e+02,
      0.5133977889215e-06, 0.5784230738814e+01, 0.1235285262111e+02,
      0.5065815825327e-06, 0.2052064793679e+01, 0.1185621865188e+02,
      0.4339831593868e-06, 0.3644994195830e+01, 0.1726015463500e+02,
      0.3952928638953e-06, 0.4930376436758e+01, 0.5481254917084e+01,

      0.4898498111942e-06, 0.4542084219731e+00, 0.9225539266174e+01,
      0.4757490209328e-06, 0.3161126388878e+01, 0.5856477690889e+01,
      0.4727701669749e-06, 0.6214993845446e+00, 0.2544314396739e+01,
      0.3800966681863e-06, 0.3040132339297e+01, 0.4265981595566e+00,
      0.3257301077939e-06, 0.8064977360087e+00, 0.3930209696940e+01,
      0.3255810528674e-06, 0.1974147981034e+01, 0.2146165377750e+01,
      0.3252029748187e-06, 0.2845924913135e+01, 0.4164311961999e+01,
      0.3255505635308e-06, 0.3017900824120e+01, 0.5088628793478e+01,
      0.2801345211990e-06, 0.6109717793179e+01, 0.1256967486051e+02,
      0.3688987740970e-06, 0.2911550235289e+01, 0.1807370494127e+02,

      0.2475153429458e-06, 0.2179146025856e+01, 0.2629832328990e-01,
      0.3033457749150e-06, 0.1994161050744e+01, 0.4535059491685e+01,
      0.2186743763110e-06, 0.5125687237936e+01, 0.1137170464392e+02,
      0.2764777032774e-06, 0.4822646860252e+00, 0.1256262854127e+02,
      0.2199028768592e-06, 0.4637633293831e+01, 0.1255903824622e+02,
      0.2046482824760e-06, 0.1467038733093e+01, 0.7084896783808e+01,
      0.2611209147507e-06, 0.3044718783485e+00, 0.7143069561767e+02,
      0.2286079656818e-06, 0.4764220356805e+01, 0.8031092209206e+01,
      0.1855071202587e-06, 0.3383637774428e+01, 0.1748016358760e+01,
      0.2324669506784e-06, 0.6189088449251e+01, 0.1831953657923e+02,

      0.1709528015688e-06, 0.5874966729774e+00, 0.4933208510675e+01,
      0.2168156875828e-06, 0.4302994009132e+01, 0.1044738781244e+02,
      0.2106675556535e-06, 0.3800475419891e+01, 0.7477522907414e+01,
      0.1430213830465e-06, 0.1294660846502e+01, 0.2942463415728e+01,
      0.1388396901944e-06, 0.4594797202114e+01, 0.8635942003952e+01,
      0.1922258844190e-06, 0.4943044543591e+00, 0.1729818233119e+02,
      0.1888460058292e-06, 0.2426943912028e+01, 0.1561374759853e+03,
      0.1789449386107e-06, 0.1582973303499e+00, 0.1592596075957e+01,
      0.1360803685374e-06, 0.5197240440504e+01, 0.1309584267300e+02,
      0.1504038014709e-06, 0.3120360916217e+01, 0.1649636139783e+02,

      0.1382769533389e-06, 0.6164702888205e+01, 0.7632943190217e+01,
      0.1438059769079e-06, 0.1437423770979e+01, 0.2042657109477e+02,
      0.1326303260037e-06, 0.3609688799679e+01, 0.1213955354133e+02,
      0.1159244950540e-06, 0.5463018167225e+01, 0.5331357529664e+01,
      0.1433118149136e-06, 0.6028909912097e+01, 0.7342457794669e+01,
      0.1234623148594e-06, 0.3109645574997e+01, 0.6279485555400e+01,
      0.1233949875344e-06, 0.3539359332866e+01, 0.6286666145492e+01,
      0.9927196061299e-07, 0.1259321569772e+01, 0.7234794171227e+01,
      0.1242302191316e-06, 0.1065949392609e+01, 0.1511046609763e+02,
      0.1098402195201e-06, 0.2192508743837e+01, 0.1098880815746e+02,

      0.1158191395315e-06, 0.4054411278650e+01, 0.5729506548653e+01,
      0.9048475596241e-07, 0.5429764748518e+01, 0.9623688285163e+01,
      0.8889853269023e-07, 0.5046586206575e+01, 0.6148010737701e+01,
      0.1048694242164e-06, 0.2628858030806e+01, 0.6836645152238e+01,
      0.1112308378646e-06, 0.4177292719907e+01, 0.1572083878776e+02,
      0.8631729709901e-07, 0.1601345232557e+01, 0.6418140963190e+01,
      0.8527816951664e-07, 0.2463888997513e+01, 0.1471231707864e+02,
      0.7892139456991e-07, 0.3154022088718e+01, 0.2118763888447e+01,
      0.1051782905236e-06, 0.4795035816088e+01, 0.1349867339771e+01,
      0.1048219943164e-06, 0.2952983395230e+01, 0.5999216516294e+01,

      0.7435760775143e-07, 0.5420547991464e+01, 0.6040347114260e+01,
      0.9869574106949e-07, 0.3695646753667e+01, 0.6566935184597e+01,
      0.9156886364226e-07, 0.3922675306609e+01, 0.5643178611111e+01,
      0.7006834356188e-07, 0.1233968624861e+01, 0.6525804586632e+01,
      0.9806170182601e-07, 0.1919542280684e+01, 0.2122839202813e+02,
      0.9052289673607e-07, 0.4615902724369e+01, 0.4690479774488e+01,
      0.7554200867893e-07, 0.1236863719072e+01, 0.1253985337760e+02,
      0.8215741286498e-07, 0.3286800101559e+00, 0.1097355562493e+02,
      0.7185178575397e-07, 0.5880942158367e+01, 0.6245048154254e+01,
      0.7130726476180e-07, 0.7674871987661e+00, 0.6321103546637e+01,

      0.6650894461162e-07, 0.6987129150116e+00, 0.5327476111629e+01,
      0.7396888823688e-07, 0.3576824794443e+01, 0.5368044267797e+00,
      0.7420588884775e-07, 0.5033615245369e+01, 0.2354323048545e+02,
      0.6141181642908e-07, 0.9449927045673e+00, 0.1296430071988e+02,
      0.6373557924058e-07, 0.6206342280341e+01, 0.9517183207817e+00,
      0.6359474329261e-07, 0.5036079095757e+01, 0.1990745094947e+01,
      0.5740173582646e-07, 0.6105106371350e+01, 0.9555997388169e+00,
      0.7019864084602e-07, 0.7237747359018e+00, 0.5225775174439e+00,
      0.6398054487042e-07, 0.3976367969666e+01, 0.2407292145756e+02,
      0.7797092650498e-07, 0.4305423910623e+01, 0.2200391463820e+02,

      0.6466760000900e-07, 0.3500136825200e+01, 0.5230807360890e+01,
      0.7529417043890e-07, 0.3514779246100e+01, 0.1842262939178e+02,
      0.6924571140892e-07, 0.2743457928679e+01, 0.1554202828031e+00,
      0.6220798650222e-07, 0.2242598118209e+01, 0.1845107853235e+02,
      0.5870209391853e-07, 0.2332832707527e+01, 0.6398972393349e+00,
      0.6263953473888e-07, 0.2191105358956e+01, 0.6277552955062e+01,
      0.6257781390012e-07, 0.4457559396698e+01, 0.6288598745829e+01,
      0.5697304945123e-07, 0.3499234761404e+01, 0.1551045220144e+01,
      0.6335438746791e-07, 0.6441691079251e+00, 0.5216580451554e+01,
      0.6377258441152e-07, 0.2252599151092e+01, 0.5650292065779e+01,

      0.6484841818165e-07, 0.1992812417646e+01, 0.1030928125552e+00,
      0.4735551485250e-07, 0.3744672082942e+01, 0.1431416805965e+02,
      0.4628595996170e-07, 0.1334226211745e+01, 0.5535693017924e+00,
      0.6258152336933e-07, 0.4395836159154e+01, 0.2608790314060e+02,
      0.6196171366594e-07, 0.2587043007997e+01, 0.8467247584405e+02,
      0.6159556952126e-07, 0.4782499769128e+01, 0.2394243902548e+03,
      0.4987741172394e-07, 0.7312257619924e+00, 0.7771377146812e+02,
      0.5459280703142e-07, 0.3001376372532e+01, 0.6179983037890e+01,
      0.4863461189999e-07, 0.3767222128541e+01, 0.9027992316901e+02,
      0.5349912093158e-07, 0.3663594450273e+01, 0.6386168663001e+01,

      0.5673725607806e-07, 0.4331187919049e+01, 0.6915859635113e+01,
      0.4745485060512e-07, 0.5816195745518e+01, 0.6282970628506e+01,
      0.4745379005326e-07, 0.8323672435672e+00, 0.6283181072386e+01,
      0.4049002796321e-07, 0.3785023976293e+01, 0.6254626709878e+01,
      0.4247084014515e-07, 0.2378220728783e+01, 0.7875671926403e+01,
      0.4026912363055e-07, 0.2864103423269e+01, 0.6311524991013e+01,
      0.4062935011774e-07, 0.2415408595975e+01, 0.3634620989887e+01,
      0.5347771048509e-07, 0.3343479309801e+01, 0.2515860172507e+02,
      0.4829494136505e-07, 0.2821742398262e+01, 0.5760498333002e+01,
      0.4342554404599e-07, 0.5624662458712e+01, 0.7238675589263e+01,

      0.4021599184361e-07, 0.5557250275009e+00, 0.1101510648075e+02,
      0.4104900474558e-07, 0.3296691780005e+01, 0.6709674010002e+01,
      0.4376532905131e-07, 0.3814443999443e+01, 0.6805653367890e+01,
      0.3314590480650e-07, 0.3560229189250e+01, 0.1259245002418e+02,
      0.3232421839643e-07, 0.5185389180568e+01, 0.1066495398892e+01,
      0.3541176318876e-07, 0.3921381909679e+01, 0.9917696840332e+01,
      0.3689831242681e-07, 0.4190658955386e+01, 0.1192625446156e+02,
      0.3890605376774e-07, 0.5546023371097e+01, 0.7478166569050e-01,
      0.3038559339780e-07, 0.6231032794494e+01, 0.1256621883632e+02,
      0.3137083969782e-07, 0.6207063419190e+01, 0.4292330755499e+01,

      0.4024004081854e-07, 0.1195257375713e+01, 0.1334167431096e+02,
      0.3300234879283e-07, 0.1804694240998e+01, 0.1057540660594e+02,
      0.3635399155575e-07, 0.5597811343500e+01, 0.6208294184755e+01,
      0.3032668691356e-07, 0.3191059366530e+01, 0.1805292951336e+02,
      0.2809652069058e-07, 0.4094348032570e+01, 0.3523159621801e-02,
      0.3696955383823e-07, 0.5219282738794e+01, 0.5966683958112e+01,
      0.3562894142503e-07, 0.1037247544554e+01, 0.6357857516136e+01,
      0.3510598524148e-07, 0.1430020816116e+01, 0.6599467742779e+01,
      0.3617736142953e-07, 0.3002911403677e+01, 0.6019991944201e+01,
      0.2624524910730e-07, 0.2437046757292e+01, 0.6702560555334e+01,

      0.2535824204490e-07, 0.1581594689647e+01, 0.3141537925223e+02,
      0.3519787226257e-07, 0.5379863121521e+01, 0.2505706758577e+03,
      0.2578406709982e-07, 0.4904222639329e+01, 0.1673046366289e+02,
      0.3423887981473e-07, 0.3646448997315e+01, 0.6546159756691e+01,
      0.2776083886467e-07, 0.3307829300144e+01, 0.1272157198369e+02,
      0.3379592818379e-07, 0.1747541251125e+01, 0.1494531617769e+02,
      0.3050255426284e-07, 0.1784689432607e-01, 0.4732030630302e+01,
      0.2652378350236e-07, 0.4420055276260e+01, 0.5863591145557e+01,
      0.2374498173768e-07, 0.3629773929208e+01, 0.2388894113936e+01,
      0.2716451255140e-07, 0.3079623706780e+01, 0.1202934727411e+02,

      0.3038583699229e-07, 0.3312487903507e+00, 0.1256608456547e+02,
      0.2220681228760e-07, 0.5265520401774e+01, 0.1336244973887e+02,
      0.3044156540912e-07, 0.4766664081250e+01, 0.2908881142201e+02,
      0.2731859923561e-07, 0.5069146530691e+01, 0.1391601904066e+02,
      0.2285603018171e-07, 0.5954935112271e+01, 0.6076890225335e+01,
      0.2025006454555e-07, 0.4061789589267e+01, 0.4701116388778e+01,
      0.2012597519804e-07, 0.2485047705241e+01, 0.6262720680387e+01,
      0.2003406962258e-07, 0.4163779209320e+01, 0.6303431020504e+01,
      0.2207863441371e-07, 0.6923839133828e+00, 0.6489261475556e+01,
      0.2481374305624e-07, 0.5944173595676e+01, 0.1204357418345e+02,

      0.2130923288870e-07, 0.4641013671967e+01, 0.5746271423666e+01,
      0.2446370543391e-07, 0.6125796518757e+01, 0.1495633313810e+00,
      0.1932492759052e-07, 0.2234572324504e+00, 0.1352175143971e+02,
      0.2600122568049e-07, 0.4281012405440e+01, 0.4590910121555e+01,
      0.2431754047488e-07, 0.1429943874870e+00, 0.1162474756779e+01,
      0.1875902869209e-07, 0.9781803816948e+00, 0.6279194432410e+01,
      0.1874381139426e-07, 0.5670368130173e+01, 0.6286957268481e+01,
      0.2156696047173e-07, 0.2008985006833e+01, 0.1813929450232e+02,
      0.1965076182484e-07, 0.2566186202453e+00, 0.4686889479442e+01,
      0.2334816372359e-07, 0.4408121891493e+01, 0.1002183730415e+02,

      0.1869937408802e-07, 0.5272745038656e+01, 0.2427287361862e+00,
      0.2436236460883e-07, 0.4407720479029e+01, 0.9514313292143e+02,
      0.1761365216611e-07, 0.1943892315074e+00, 0.1351787002167e+02,
      0.2156289480503e-07, 0.1418570924545e+01, 0.6037244212485e+01,
      0.2164748979255e-07, 0.4724603439430e+01, 0.2301353951334e+02,
      0.2222286670853e-07, 0.2400266874598e+01, 0.1266924451345e+02,
      0.2070901414929e-07, 0.5230348028732e+01, 0.6528907488406e+01,
      0.1792745177020e-07, 0.2099190328945e+01, 0.6819880277225e+01,
      0.1841802068445e-07, 0.3467527844848e+00, 0.6514761976723e+02,
      0.1578401631718e-07, 0.7098642356340e+00, 0.2077542790660e-01,

      0.1561690152531e-07, 0.5943349620372e+01, 0.6272439236156e+01,
      0.1558591045463e-07, 0.7040653478980e+00, 0.6293712464735e+01,
      0.1737356469576e-07, 0.4487064760345e+01, 0.1765478049437e+02,
      0.1434755619991e-07, 0.2993391570995e+01, 0.1102062672231e+00,
      0.1482187806654e-07, 0.2278049198251e+01, 0.1052268489556e+01,
      0.1424812827089e-07, 0.1682114725827e+01, 0.1311972100268e+02,
      0.1380282448623e-07, 0.3262668602579e+01, 0.1017725758696e+02,
      0.1811481244566e-07, 0.3187771221777e+01, 0.1887552587463e+02,
      0.1504446185696e-07, 0.5650162308647e+01, 0.7626583626240e-01,
      0.1740776154137e-07, 0.5487068607507e+01, 0.1965104848470e+02,

      0.1374339536251e-07, 0.5745688172201e+01, 0.6016468784579e+01,
      0.1761377477704e-07, 0.5748060203659e+01, 0.2593412433514e+02,
      0.1535138225795e-07, 0.6226848505790e+01, 0.9411464614024e+01,
      0.1788140543676e-07, 0.6189318878563e+01, 0.3301902111895e+02,
      0.1375002807996e-07, 0.5371812884394e+01, 0.6327837846670e+00,
      0.1242115758632e-07, 0.1471687569712e+01, 0.3894181736510e+01,
      0.1450977333938e-07, 0.4143836662127e+01, 0.1277945078067e+02,
      0.1297579575023e-07, 0.9003477661957e+00, 0.6549682916313e+01,
      0.1462667934821e-07, 0.5760505536428e+01, 0.1863592847156e+02,
      0.1381774374799e-07, 0.1085471729463e+01, 0.2379164476796e+01,

      0.1682333169307e-07, 0.5409870870133e+01, 0.1620077269078e+02,
      0.1190812918837e-07, 0.1397205174601e+01, 0.1149965630200e+02,
      0.1221434762106e-07, 0.9001804809095e+00, 0.1257326515556e+02,
      0.1549934644860e-07, 0.4262528275544e+01, 0.1820933031200e+02,
      0.1252138953050e-07, 0.1411642012027e+01, 0.6993008899458e+01,
      0.1237078905387e-07, 0.2844472403615e+01, 0.2435678079171e+02,
      0.1446953389615e-07, 0.5295835522223e+01, 0.3813291813120e-01,
      0.1388446457170e-07, 0.4969428135497e+01, 0.2458316379602e+00,
      0.1019339179228e-07, 0.2491369561806e+01, 0.6112403035119e+01,
      0.1258880815343e-07, 0.4679426248976e+01, 0.5429879531333e+01,

      0.1297768238261e-07, 0.1074509953328e+01, 0.1249137003520e+02,
      0.9913505718094e-08, 0.4735097918224e+01, 0.6247047890016e+01,
      0.9830453155969e-08, 0.4158649187338e+01, 0.6453748665772e+01,
      0.1192615865309e-07, 0.3438208613699e+01, 0.6290122169689e+01,
      0.9835874798277e-08, 0.1913300781229e+01, 0.6319103810876e+01,
      0.9639087569277e-08, 0.9487683644125e+00, 0.8273820945392e+01,
      0.1175716107001e-07, 0.3228141664287e+01, 0.6276029531202e+01,
      0.1018926508678e-07, 0.2216607854300e+01, 0.1254537627298e+02,
      0.9500087869225e-08, 0.2625116459733e+01, 0.1256517118505e+02,
      0.9664192916575e-08, 0.5860562449214e+01, 0.6259197520765e+01,

      0.9612858712203e-08, 0.7885682917381e+00, 0.6306954180126e+01,
      0.1117645675413e-07, 0.3932148831189e+01, 0.1779695906178e+02,
      0.1158864052160e-07, 0.9995605521691e+00, 0.1778273215245e+02,
      0.9021043467028e-08, 0.5263769742673e+01, 0.6172869583223e+01,
      0.8836134773563e-08, 0.1496843220365e+01, 0.1692165728891e+01,
      0.1045872200691e-07, 0.7009039517214e+00, 0.2204125344462e+00,
      0.1211463487798e-07, 0.4041544938511e+01, 0.8257698122054e+02,
      0.8541990804094e-08, 0.1447586692316e+01, 0.6393282117669e+01,
      0.1038720703636e-07, 0.4594249718112e+00, 0.1550861511662e+02,
      0.1126722351445e-07, 0.3925550579036e+01, 0.2061856251104e+00,

      0.8697373859631e-08, 0.4411341856037e+01, 0.9491756770005e+00,
      0.8869380028441e-08, 0.2402659724813e+01, 0.3903911373650e+01,
      0.9247014693258e-08, 0.1401579743423e+01, 0.6267823317922e+01,
      0.9205062930950e-08, 0.5245978000814e+01, 0.6298328382969e+01,
      0.8000745038049e-08, 0.3590803356945e+01, 0.2648454860559e+01,
      0.9168973650819e-08, 0.2470150501679e+01, 0.1498544001348e+03,
      0.1075444949238e-07, 0.1328606161230e+01, 0.3694923081589e+02,
      0.7817298525817e-08, 0.6162256225998e+01, 0.4804209201333e+01,
      0.9541469226356e-08, 0.3942568967039e+01, 0.1256713221673e+02,
      0.9821910122027e-08, 0.2360246287233e+00, 0.1140367694411e+02,

      0.9897822023777e-08, 0.4619805634280e+01, 0.2280573557157e+02,
      0.7737289283765e-08, 0.3784727847451e+01, 0.7834121070590e+01,
      0.9260204034710e-08, 0.2223352487601e+01, 0.2787043132925e+01,
      0.7320252888486e-08, 0.1288694636874e+01, 0.6282655592598e+01,
      0.7319785780946e-08, 0.5359869567774e+01, 0.6283496108294e+01,
      0.7147219933778e-08, 0.5516616675856e+01, 0.1725663147538e+02,
      0.7946502829878e-08, 0.2630459984567e+01, 0.1241073141809e+02,
      0.9001711808932e-08, 0.2849815827227e+01, 0.6281591679874e+01,
      0.8994041507257e-08, 0.3795244450750e+01, 0.6284560021018e+01,
      0.8298582787358e-08, 0.5236413127363e+00, 0.1241658836951e+02,

      0.8526596520710e-08, 0.4794605424426e+01, 0.1098419223922e+02,
      0.8209822103197e-08, 0.1578752370328e+01, 0.1096996532989e+02,
      0.6357049861094e-08, 0.5708926113761e+01, 0.1596186371003e+01,
      0.7370473179049e-08, 0.3842402530241e+01, 0.4061219149443e+01,
      0.7232154664726e-08, 0.3067548981535e+01, 0.1610006857377e+03,
      0.6328765494903e-08, 0.1313930030069e+01, 0.1193336791622e+02,
      0.8030064908595e-08, 0.3488500408886e+01, 0.8460828644453e+00,
      0.6275464259232e-08, 0.1532061626198e+01, 0.8531963191132e+00,
      0.7051897446325e-08, 0.3285859929993e+01, 0.5849364236221e+01,
      0.6161593705428e-08, 0.1477341999464e+01, 0.5573142801433e+01,

      0.7754683957278e-08, 0.1586118663096e+01, 0.8662240327241e+01,
      0.5889928990701e-08, 0.1304887868803e+01, 0.1232342296471e+02,
      0.5705756047075e-08, 0.4555333589350e+01, 0.1258692712880e+02,
      0.5964178808332e-08, 0.3001762842062e+01, 0.5333900173445e+01,
      0.6712446027467e-08, 0.4886780007595e+01, 0.1171295538178e+02,
      0.5941809275464e-08, 0.4701509603824e+01, 0.9779108567966e+01,
      0.5466993627395e-08, 0.4588357817278e+01, 0.1884211409667e+02,
      0.6340512090980e-08, 0.1164543038893e+01, 0.5217580628120e+02,
      0.6325505710045e-08, 0.3919171259645e+01, 0.1041998632314e+02,
      0.6164789509685e-08, 0.2143828253542e+01, 0.6151533897323e+01,

      0.5263330812430e-08, 0.6066564434241e+01, 0.1885275071096e+02,
      0.5597087780221e-08, 0.2926316429472e+01, 0.4337116142245e+00,
      0.5396556236817e-08, 0.3244303591505e+01, 0.6286362197481e+01,
      0.5396615148223e-08, 0.3404304703662e+01, 0.6279789503410e+01,
      0.7091832443341e-08, 0.8532377803192e+00, 0.4907302013889e+01,
      0.6572352589782e-08, 0.4901966774419e+01, 0.1176433076753e+02,
      0.5960236060795e-08, 0.1874672315797e+01, 0.1422690933580e-01,
      0.5125480043511e-08, 0.3735726064334e+01, 0.1245594543367e+02,
      0.5928241866410e-08, 0.4502033899935e+01, 0.6414617803568e+01,
      0.5249600357424e-08, 0.4372334799878e+01, 0.1151388321134e+02,

      0.6059171276087e-08, 0.2581617302908e+01, 0.6062663316000e+01,
      0.5295235081662e-08, 0.2974811513158e+01, 0.3496032717521e+01,
      0.5820561875933e-08, 0.1796073748244e+00, 0.2838593341516e+00,
      0.4754696606440e-08, 0.1981998136973e+01, 0.3104930017775e+01,
      0.6385053548955e-08, 0.2559174171605e+00, 0.6133512519065e+01,
      0.6589828273941e-08, 0.2750967106776e+01, 0.4087944051283e+02,
      0.5383376567189e-08, 0.6325947523578e+00, 0.2248384854122e+02,
      0.5928941683538e-08, 0.1672304519067e+01, 0.1581959461667e+01,
      0.4816060709794e-08, 0.3512566172575e+01, 0.9388005868221e+01,
      0.6003381586512e-08, 0.5610932219189e+01, 0.5326786718777e+01,

      0.5504225393105e-08, 0.4037501131256e+01, 0.6503488384892e+01,
      0.5353772620129e-08, 0.6122774968240e+01, 0.1735668374386e+03,
      0.5786253768544e-08, 0.5527984999515e+01, 0.1350651127443e+00,
      0.5065706702002e-08, 0.9980765573624e+00, 0.1248988586463e+02,
      0.5972838885276e-08, 0.6044489493203e+01, 0.2673594526851e+02,
      0.5323585877961e-08, 0.3924265998147e+01, 0.4171425416666e+01,
      0.5210772682858e-08, 0.6220111376901e+01, 0.2460261242967e+02,
      0.4726549040535e-08, 0.3716043206862e+01, 0.7232251527446e+01,
      0.6029425105059e-08, 0.8548704071116e+00, 0.3227113045244e+03,
      0.4481542826513e-08, 0.1426925072829e+01, 0.5547199253223e+01,

      0.5836024505068e-08, 0.7135651752625e-01, 0.7285056171570e+02,
      0.4137046613272e-08, 0.5330767643283e+01, 0.1087398597200e+02,
      0.5171977473924e-08, 0.4494262335353e+00, 0.1884570439172e+02,
      0.5694429833732e-08, 0.2952369582215e+01, 0.9723862754494e+02,
      0.4009158925298e-08, 0.3500003416535e+01, 0.6244942932314e+01,
      0.4784939596873e-08, 0.6196709413181e+01, 0.2929661536378e+02,
      0.3983725022610e-08, 0.5103690031897e+01, 0.4274518229222e+01,
      0.3870535232462e-08, 0.3187569587401e+01, 0.6321208768577e+01,
      0.5140501213951e-08, 0.1668924357457e+01, 0.1232032006293e+02,
      0.3849034819355e-08, 0.4445722510309e+01, 0.1726726808967e+02,

      0.4002383075060e-08, 0.5226224152423e+01, 0.7018952447668e+01,
      0.3890719543549e-08, 0.4371166550274e+01, 0.1491901785440e+02,
      0.4887084607881e-08, 0.5973556689693e+01, 0.1478866649112e+01,
      0.3739939287592e-08, 0.2089084714600e+01, 0.6922973089781e+01,
      0.5031925918209e-08, 0.4658371936827e+01, 0.1715706182245e+02,
      0.4387748764954e-08, 0.4825580552819e+01, 0.2331413144044e+03,
      0.4147398098865e-08, 0.3739003524998e+01, 0.1376059875786e+02,
      0.3719089993586e-08, 0.1148941386536e+01, 0.6297302759782e+01,
      0.3934238461056e-08, 0.1559893008343e+01, 0.7872148766781e+01,
      0.3672471375622e-08, 0.5516145383612e+01, 0.6268848941110e+01,

      0.3768911277583e-08, 0.6116053700563e+01, 0.4157198507331e+01,
      0.4033388417295e-08, 0.5076821746017e+01, 0.1567108171867e+02,
      0.3764194617832e-08, 0.8164676232075e+00, 0.3185192151914e+01,
      0.4840628226284e-08, 0.1360479453671e+01, 0.1252801878276e+02,
      0.4949443923785e-08, 0.2725622229926e+01, 0.1617106187867e+03,
      0.4117393089971e-08, 0.6054459628492e+00, 0.5642198095270e+01,
      0.3925754020428e-08, 0.8570462135210e+00, 0.2139354194808e+02,
      0.3630551757923e-08, 0.3552067338279e+01, 0.6294805223347e+01,
      0.3627274802357e-08, 0.3096565085313e+01, 0.6271346477544e+01,
      0.3806143885093e-08, 0.6367751709777e+00, 0.1725304118033e+02,

      0.4433254641565e-08, 0.4848461503937e+01, 0.7445550607224e+01,
      0.3712319846576e-08, 0.1331950643655e+01, 0.4194847048887e+00,
      0.3849847534783e-08, 0.4958368297746e+00, 0.9562891316684e+00,
      0.3483955430165e-08, 0.2237215515707e+01, 0.1161697602389e+02,
      0.3961912730982e-08, 0.3332402188575e+01, 0.2277943724828e+02,
      0.3419978244481e-08, 0.5785600576016e+01, 0.1362553364512e+02,
      0.3329417758177e-08, 0.9812676559709e-01, 0.1685848245639e+02,
      0.4207206893193e-08, 0.9494780468236e+00, 0.2986433403208e+02,
      0.3268548976410e-08, 0.1739332095686e+00, 0.5749861718712e+01,
      0.3321880082685e-08, 0.1423354800666e+01, 0.6279143387820e+01,

      0.4503173010852e-08, 0.2314972675293e+00, 0.1385561574497e+01,
      0.4316599090954e-08, 0.1012646782616e+00, 0.4176041334900e+01,
      0.3283493323850e-08, 0.5233306881265e+01, 0.6287008313071e+01,
      0.3164033542343e-08, 0.4005597257511e+01, 0.2099539292909e+02,
      0.4159720956725e-08, 0.5365676242020e+01, 0.5905702259363e+01,
      0.3565176892217e-08, 0.4284440620612e+01, 0.3932462625300e-02,
      0.3514440950221e-08, 0.4270562636575e+01, 0.7335344340001e+01,
      0.3540596871909e-08, 0.5953553201060e+01, 0.1234573916645e+02,
      0.2960769905118e-08, 0.1115180417718e+01, 0.2670964694522e+02,
      0.2962213739684e-08, 0.3863811918186e+01, 0.6408777551755e+00,

      0.3883556700251e-08, 0.1268617928302e+01, 0.6660449441528e+01,
      0.2919225516346e-08, 0.4908605223265e+01, 0.1375773836557e+01,
      0.3115158863370e-08, 0.3744519976885e+01, 0.3802769619140e-01,
      0.4099438144212e-08, 0.4173244670532e+01, 0.4480965020977e+02,
      0.2899531858964e-08, 0.5910601428850e+01, 0.2059724391010e+02,
      0.3289733429855e-08, 0.2488050078239e+01, 0.1081813534213e+02,
      0.3933075612875e-08, 0.1122363652883e+01, 0.3773735910827e+00,
      0.3021403764467e-08, 0.4951973724904e+01, 0.2982630633589e+02,
      0.2798598949757e-08, 0.5117057845513e+01, 0.1937891852345e+02,
      0.3397421302707e-08, 0.6104159180476e+01, 0.6923953605621e+01,

      0.3720398002179e-08, 0.1184933429829e+01, 0.3066615496545e+02,
      0.3598484186267e-08, 0.3505282086105e+01, 0.6147450479709e+01,
      0.3694594027310e-08, 0.2286651088141e+01, 0.2636725487657e+01,
      0.2680444152969e-08, 0.1871816775482e+00, 0.6816289982179e+01,
      0.3497574865641e-08, 0.3143251755431e+01, 0.6418701221183e+01,
      0.3130274129494e-08, 0.2462167316018e+01, 0.1235996607578e+02,
      0.3241119069551e-08, 0.4256374004686e+01, 0.1652265972112e+02,
      0.2601960842061e-08, 0.4970362941425e+01, 0.1045450126711e+02,
      0.2690601527504e-08, 0.2372657824898e+01, 0.3163918923335e+00,
      0.2908688152664e-08, 0.4232652627721e+01, 0.2828699048865e+02,

      0.3120456131875e-08, 0.3925747001137e+00, 0.2195415756911e+02,
      0.3148855423384e-08, 0.3093478330445e+01, 0.1172006883645e+02,
      0.3051044261017e-08, 0.5560948248212e+01, 0.6055599646783e+01,
      0.2826006876660e-08, 0.5072790310072e+01, 0.5120601093667e+01,
      0.3100034191711e-08, 0.4998530231096e+01, 0.1799603123222e+02,
      0.2398771640101e-08, 0.2561739802176e+01, 0.6255674361143e+01,
      0.2384002842728e-08, 0.4087420284111e+01, 0.6310477339748e+01,
      0.2842146517568e-08, 0.2515048217955e+01, 0.5469525544182e+01,
      0.2847674371340e-08, 0.5235326497443e+01, 0.1034429499989e+02,
      0.2903722140764e-08, 0.1088200795797e+01, 0.6510552054109e+01,

      0.3187610710605e-08, 0.4710624424816e+01, 0.1693792562116e+03,
      0.3048869992813e-08, 0.2857975896445e+00, 0.8390110365991e+01,
      0.2860216950984e-08, 0.2241619020815e+01, 0.2243449970715e+00,
      0.2701117683113e-08, 0.6651573305272e-01, 0.6129297044991e+01,
      0.2509891590152e-08, 0.1285135324585e+01, 0.1044027435778e+02,
      0.2623200252223e-08, 0.2981229834530e+00, 0.6436854655901e+01,
      0.2622541669202e-08, 0.6122470726189e+01, 0.9380959548977e+01,
      0.2818435667099e-08, 0.4251087148947e+01, 0.5934151399930e+01,
      0.2365196797465e-08, 0.3465070460790e+01, 0.2470570524223e+02,
      0.2358704646143e-08, 0.5791603815350e+01, 0.8671969964381e+01,

      0.2388299481390e-08, 0.4142483772941e+01, 0.7096626156709e+01,
      0.1996041217224e-08, 0.2101901889496e+01, 0.1727188400790e+02,
      0.2687593060336e-08, 0.1526689456959e+01, 0.7075506709219e+02,
      0.2618913670810e-08, 0.2397684236095e+01, 0.6632000300961e+01,
      0.2571523050364e-08, 0.5751929456787e+00, 0.6206810014183e+01,
      0.2582135006946e-08, 0.5595464352926e+01, 0.4873985990671e+02,
      0.2372530190361e-08, 0.5092689490655e+01, 0.1590676413561e+02,
      0.2357178484712e-08, 0.4444363527851e+01, 0.3097883698531e+01,
      0.2451590394723e-08, 0.3108251687661e+01, 0.6612329252343e+00,
      0.2370045949608e-08, 0.2608133861079e+01, 0.3459636466239e+02,

      0.2268997267358e-08, 0.3639717753384e+01, 0.2844914056730e-01,
      0.1731432137906e-08, 0.1741898445707e+00, 0.2019909489111e+02,
      0.1629869741622e-08, 0.3902225646724e+01, 0.3035599730800e+02,
      0.2206215801974e-08, 0.4971131250731e+01, 0.6281667977667e+01,
      0.2205469554680e-08, 0.1677462357110e+01, 0.6284483723224e+01,
      0.2148792362509e-08, 0.4236259604006e+01, 0.1980482729015e+02,
      0.1873733657847e-08, 0.5926814998687e+01, 0.2876692439167e+02,
      0.2026573758959e-08, 0.4349643351962e+01, 0.2449240616245e+02,
      0.1807770325110e-08, 0.5700940482701e+01, 0.2045286941806e+02,
      0.1881174408581e-08, 0.6601286363430e+00, 0.2358125818164e+02,

      0.1368023671690e-08, 0.2211098592752e+01, 0.2473415438279e+02,
      0.1720017916280e-08, 0.4942488551129e+01, 0.1679593901136e+03,
      0.1702427665131e-08, 0.1452233856386e+01, 0.3338575901272e+03,
      0.1414032510054e-08, 0.5525357721439e+01, 0.1624205518357e+03,
      0.1652626045364e-08, 0.4108794283624e+01, 0.8956999012000e+02,
      0.1642957769686e-08, 0.7344335209984e+00, 0.5267006960365e+02,
      0.1614952403624e-08, 0.3541213951363e+01, 0.3332657872986e+02,
      0.1535988291188e-08, 0.4031094072151e+01, 0.3852657435933e+02,
      0.1593193738177e-08, 0.4185136203609e+01, 0.2282781046519e+03,
      0.1074569126382e-08, 0.1720485636868e+01, 0.8397383534231e+02,

      0.1074408214509e-08, 0.2758613420318e+01, 0.8401985929482e+02,
      0.9700199670465e-09, 0.4216686842097e+01, 0.7826370942180e+02,
      0.1258433517061e-08, 0.2575068876639e+00, 0.3115650189215e+03,
      0.1240303229539e-08, 0.4800844956756e+00, 0.1784300471910e+03,
      0.9018345948127e-09, 0.3896756361552e+00, 0.5886454391678e+02,
      0.1135301432805e-08, 0.3700805023550e+00, 0.7842370451713e+02,
      0.9215887951370e-09, 0.4364579276638e+01, 0.1014262087719e+03,
      0.1055401054147e-08, 0.2156564222111e+01, 0.5660027930059e+02,
      0.1008725979831e-08, 0.5454015785234e+01, 0.4245678405627e+02,
      0.7217398104321e-09, 0.1597772562175e+01, 0.2457074661053e+03,

      0.6912033134447e-09, 0.5824090621461e+01, 0.1679936946371e+03,
      0.6833881523549e-09, 0.3578778482835e+01, 0.6053048899753e+02,
      0.4887304205142e-09, 0.3724362812423e+01, 0.9656299901946e+02,
      0.5173709754788e-09, 0.5422427507933e+01, 0.2442876000072e+03,
      0.4671353097145e-09, 0.2396106924439e+01, 0.1435713242844e+03,
      0.5652608439480e-09, 0.2804028838685e+01, 0.8365903305582e+02,
      0.5604061331253e-09, 0.1638816006247e+01, 0.8433466158131e+02,
      0.4712723365400e-09, 0.8979003224474e+00, 0.3164282286739e+03,
      0.4909967465112e-09, 0.3210426725516e+01, 0.4059982187939e+03,
      0.4771358267658e-09, 0.5308027211629e+01, 0.1805255418145e+03,

      0.3943451445989e-09, 0.2195145341074e+01, 0.2568537517081e+03,
      0.3952109120244e-09, 0.5081189491586e+01, 0.2449975330562e+03,
      0.3788134594789e-09, 0.4345171264441e+01, 0.1568131045107e+03,
      0.3738330190479e-09, 0.2613062847997e+01, 0.3948519331910e+03,
      0.3099866678136e-09, 0.2846760817689e+01, 0.1547176098872e+03,
      0.2002962716768e-09, 0.4921360989412e+01, 0.2268582385539e+03,
      0.2198291338754e-09, 0.1130360117454e+00, 0.1658638954901e+03,
      0.1491958330784e-09, 0.4228195232278e+01, 0.2219950288015e+03,
      0.1475384076173e-09, 0.3005721811604e+00, 0.3052819430710e+03,
      0.1661626624624e-09, 0.7830125621203e+00, 0.2526661704812e+03,

      0.9015823460025e-10, 0.3807792942715e+01, 0.4171445043968e+03 };

/* Sun-to-Earth, T^0, Y */
   static const double e0y[] = {
      0.9998921098898e+00, 0.1826583913846e+00, 0.6283075850446e+01,
     -0.2442700893735e-01, 0.0000000000000e+00, 0.0000000000000e+00,
      0.8352929742915e-02, 0.1395277998680e+00, 0.1256615170089e+02,
      0.1046697300177e-03, 0.9641423109763e-01, 0.1884922755134e+02,
      0.3110841876663e-04, 0.5381140401712e+01, 0.8399684731857e+02,
      0.2570269094593e-04, 0.5301016407128e+01, 0.5296909721118e+00,
      0.2147389623610e-04, 0.2662510869850e+01, 0.1577343543434e+01,
      0.1680344384050e-04, 0.5207904119704e+01, 0.6279552690824e+01,
      0.1679117312193e-04, 0.4582187486968e+01, 0.6286599010068e+01,
      0.1440512068440e-04, 0.1900688517726e+01, 0.2352866153506e+01,

      0.1135139664999e-04, 0.5273108538556e+01, 0.5223693906222e+01,
      0.9345482571018e-05, 0.4503047687738e+01, 0.1203646072878e+02,
      0.9007418719568e-05, 0.1605621059637e+01, 0.1021328554739e+02,
      0.5671536712314e-05, 0.5812849070861e+00, 0.1059381944224e+01,
      0.7451401861666e-05, 0.2807346794836e+01, 0.3981490189893e+00,
      0.6393470057114e-05, 0.6029224133855e+01, 0.5753384878334e+01,
      0.6814275881697e-05, 0.6472990145974e+00, 0.4705732307012e+01,
      0.6113705628887e-05, 0.3813843419700e+01, 0.6812766822558e+01,
      0.4503851367273e-05, 0.4527804370996e+01, 0.5884926831456e+01,
      0.4522249141926e-05, 0.5991783029224e+01, 0.6256777527156e+01,

      0.4501794307018e-05, 0.3798703844397e+01, 0.6309374173736e+01,
      0.5514927480180e-05, 0.3961257833388e+01, 0.5507553240374e+01,
      0.4062862799995e-05, 0.5256247296369e+01, 0.6681224869435e+01,
      0.5414900429712e-05, 0.5499032014097e+01, 0.7755226100720e+00,
      0.5463153987424e-05, 0.6173092454097e+01, 0.1414349524433e+02,
      0.5071611859329e-05, 0.2870244247651e+01, 0.7860419393880e+01,
      0.2195112094455e-05, 0.2952338617201e+01, 0.1150676975667e+02,
      0.2279139233919e-05, 0.5951775132933e+01, 0.7058598460518e+01,
      0.2278386100876e-05, 0.4845456398785e+01, 0.4694002934110e+01,
      0.2559088003308e-05, 0.6945321117311e+00, 0.1216800268190e+02,

      0.2561079286856e-05, 0.6167224608301e+01, 0.7099330490126e+00,
      0.1792755796387e-05, 0.1400122509632e+01, 0.7962980379786e+00,
      0.1818715656502e-05, 0.4703347611830e+01, 0.6283142985870e+01,
      0.1818744924791e-05, 0.5086748900237e+01, 0.6283008715021e+01,
      0.1554518791390e-05, 0.5331008042713e-01, 0.2513230340178e+02,
      0.2063265737239e-05, 0.4283680484178e+01, 0.1179062909082e+02,
      0.1497613520041e-05, 0.6074207826073e+01, 0.5486777812467e+01,
      0.2000617940427e-05, 0.2501426281450e+01, 0.1778984560711e+02,
      0.1289731195580e-05, 0.3646340599536e+01, 0.7079373888424e+01,
      0.1282657998934e-05, 0.3232864804902e+01, 0.3738761453707e+01,

      0.1528915968658e-05, 0.5581433416669e+01, 0.2132990797783e+00,
      0.1187304098432e-05, 0.5453576453694e+01, 0.9437762937313e+01,
      0.7842782928118e-06, 0.2823953922273e+00, 0.8827390247185e+01,
      0.7352892280868e-06, 0.1124369580175e+01, 0.1589072916335e+01,
      0.6570189360797e-06, 0.2089154042840e+01, 0.1176985366291e+02,
      0.6324967590410e-06, 0.6704855581230e+00, 0.6262300422539e+01,
      0.6298289872283e-06, 0.2836414855840e+01, 0.6303851278352e+01,
      0.6476686465855e-06, 0.4852433866467e+00, 0.7113454667900e-02,
      0.8587034651234e-06, 0.1453511005668e+01, 0.1672837615881e+03,
      0.8068948788113e-06, 0.9224087798609e+00, 0.6069776770667e+01,

      0.8353786011661e-06, 0.4631707184895e+01, 0.3340612434717e+01,
      0.6009324532132e-06, 0.1829498827726e+01, 0.4136910472696e+01,
      0.7558158559566e-06, 0.2588596800317e+01, 0.6496374930224e+01,
      0.5809279504503e-06, 0.5516818853476e+00, 0.1097707878456e+02,
      0.5374131950254e-06, 0.6275674734960e+01, 0.1194447056968e+01,
      0.5711160507326e-06, 0.1091905956872e+01, 0.6282095334605e+01,
      0.5710183170746e-06, 0.2415001635090e+01, 0.6284056366286e+01,
      0.5144373590610e-06, 0.6020336443438e+01, 0.6290189305114e+01,
      0.5103108927267e-06, 0.3775634564605e+01, 0.6275962395778e+01,
      0.4960654697891e-06, 0.1073450946756e+01, 0.6127655567643e+01,

      0.4786385689280e-06, 0.2431178012310e+01, 0.6438496133249e+01,
      0.6109911263665e-06, 0.5343356157914e+01, 0.3154687086868e+01,
      0.4839898944024e-06, 0.5830833594047e-01, 0.8018209333619e+00,
      0.4734822623919e-06, 0.4536080134821e+01, 0.3128388763578e+01,
      0.4834741473290e-06, 0.2585090489754e+00, 0.7084896783808e+01,
      0.5134858581156e-06, 0.4213317172603e+01, 0.1235285262111e+02,
      0.5064004264978e-06, 0.4814418806478e+00, 0.1185621865188e+02,
      0.3753476772761e-06, 0.1599953399788e+01, 0.8429241228195e+01,
      0.4935264014283e-06, 0.2157417556873e+01, 0.2544314396739e+01,
      0.3950929600897e-06, 0.3359394184254e+01, 0.5481254917084e+01,

      0.4895849789777e-06, 0.5165704376558e+01, 0.9225539266174e+01,
      0.4215241688886e-06, 0.2065368800993e+01, 0.1726015463500e+02,
      0.3796773731132e-06, 0.1468606346612e+01, 0.4265981595566e+00,
      0.3114178142515e-06, 0.3615638079474e+01, 0.2146165377750e+01,
      0.3260664220838e-06, 0.4417134922435e+01, 0.4164311961999e+01,
      0.3976996123008e-06, 0.4700866883004e+01, 0.5856477690889e+01,
      0.2801459672924e-06, 0.4538902060922e+01, 0.1256967486051e+02,
      0.3638931868861e-06, 0.1334197991475e+01, 0.1807370494127e+02,
      0.2487013269476e-06, 0.3749275558275e+01, 0.2629832328990e-01,
      0.3034165481994e-06, 0.4236622030873e+00, 0.4535059491685e+01,

      0.2676278825586e-06, 0.5970848007811e+01, 0.3930209696940e+01,
      0.2764903818918e-06, 0.5194636754501e+01, 0.1256262854127e+02,
      0.2485149930507e-06, 0.1002434207846e+01, 0.5088628793478e+01,
      0.2199305540941e-06, 0.3066773098403e+01, 0.1255903824622e+02,
      0.2571106500435e-06, 0.7588312459063e+00, 0.1336797263425e+02,
      0.2049751817158e-06, 0.3444977434856e+01, 0.1137170464392e+02,
      0.2599707296297e-06, 0.1873128542205e+01, 0.7143069561767e+02,
      0.1785018072217e-06, 0.5015891306615e+01, 0.1748016358760e+01,
      0.2324833891115e-06, 0.4618271239730e+01, 0.1831953657923e+02,
      0.1709711119545e-06, 0.5300003455669e+01, 0.4933208510675e+01,

      0.2107159351716e-06, 0.2229819815115e+01, 0.7477522907414e+01,
      0.1750333080295e-06, 0.6161485880008e+01, 0.1044738781244e+02,
      0.2000598210339e-06, 0.2967357299999e+01, 0.8031092209206e+01,
      0.1380920248681e-06, 0.3027007923917e+01, 0.8635942003952e+01,
      0.1412460470299e-06, 0.6037597163798e+01, 0.2942463415728e+01,
      0.1888459803001e-06, 0.8561476243374e+00, 0.1561374759853e+03,
      0.1788370542585e-06, 0.4869736290209e+01, 0.1592596075957e+01,
      0.1360893296167e-06, 0.3626411886436e+01, 0.1309584267300e+02,
      0.1506846530160e-06, 0.1550975377427e+01, 0.1649636139783e+02,
      0.1800913376176e-06, 0.2075826033190e+01, 0.1729818233119e+02,

      0.1436261390649e-06, 0.6148876420255e+01, 0.2042657109477e+02,
      0.1220227114151e-06, 0.4382583879906e+01, 0.7632943190217e+01,
      0.1337883603592e-06, 0.2036644327361e+01, 0.1213955354133e+02,
      0.1159326650738e-06, 0.3892276994687e+01, 0.5331357529664e+01,
      0.1352853128569e-06, 0.1447950649744e+01, 0.1673046366289e+02,
      0.1433408296083e-06, 0.4457854692961e+01, 0.7342457794669e+01,
      0.1234701666518e-06, 0.1538818147151e+01, 0.6279485555400e+01,
      0.1234027192007e-06, 0.1968523220760e+01, 0.6286666145492e+01,
      0.1244024091797e-06, 0.5779803499985e+01, 0.1511046609763e+02,
      0.1097934945516e-06, 0.6210975221388e+00, 0.1098880815746e+02,

      0.1254611329856e-06, 0.2591963807998e+01, 0.1572083878776e+02,
      0.1158247286784e-06, 0.2483612812670e+01, 0.5729506548653e+01,
      0.9039078252960e-07, 0.3857554579796e+01, 0.9623688285163e+01,
      0.9108024978836e-07, 0.5826368512984e+01, 0.7234794171227e+01,
      0.8887068108436e-07, 0.3475694573987e+01, 0.6148010737701e+01,
      0.8632374035438e-07, 0.3059070488983e-01, 0.6418140963190e+01,
      0.7893186992967e-07, 0.1583194837728e+01, 0.2118763888447e+01,
      0.8297650201172e-07, 0.8519770534637e+00, 0.1471231707864e+02,
      0.1019759578988e-06, 0.1319598738732e+00, 0.1349867339771e+01,
      0.1010037696236e-06, 0.9937860115618e+00, 0.6836645152238e+01,

      0.1047727548266e-06, 0.1382138405399e+01, 0.5999216516294e+01,
      0.7351993881086e-07, 0.3833397851735e+01, 0.6040347114260e+01,
      0.9868771092341e-07, 0.2124913814390e+01, 0.6566935184597e+01,
      0.7007321959390e-07, 0.5946305343763e+01, 0.6525804586632e+01,
      0.6861411679709e-07, 0.4574654977089e+01, 0.7238675589263e+01,
      0.7554519809614e-07, 0.5949232686844e+01, 0.1253985337760e+02,
      0.9541880448335e-07, 0.3495242990564e+01, 0.2122839202813e+02,
      0.7185606722155e-07, 0.4310113471661e+01, 0.6245048154254e+01,
      0.7131360871710e-07, 0.5480309323650e+01, 0.6321103546637e+01,
      0.6651142021039e-07, 0.5411097713654e+01, 0.5327476111629e+01,

      0.8538618213667e-07, 0.1827849973951e+01, 0.1101510648075e+02,
      0.8634954288044e-07, 0.5443584943349e+01, 0.5643178611111e+01,
      0.7449415051484e-07, 0.2011535459060e+01, 0.5368044267797e+00,
      0.7421047599169e-07, 0.3464562529249e+01, 0.2354323048545e+02,
      0.6140694354424e-07, 0.5657556228815e+01, 0.1296430071988e+02,
      0.6353525143033e-07, 0.3463816593821e+01, 0.1990745094947e+01,
      0.6221964013447e-07, 0.1532259498697e+01, 0.9517183207817e+00,
      0.5852480257244e-07, 0.1375396598875e+01, 0.9555997388169e+00,
      0.6398637498911e-07, 0.2405645801972e+01, 0.2407292145756e+02,
      0.7039744069878e-07, 0.5397541799027e+01, 0.5225775174439e+00,

      0.6977997694382e-07, 0.4762347105419e+01, 0.1097355562493e+02,
      0.7460629558396e-07, 0.2711944692164e+01, 0.2200391463820e+02,
      0.5376577536101e-07, 0.2352980430239e+01, 0.1431416805965e+02,
      0.7530607893556e-07, 0.1943940180699e+01, 0.1842262939178e+02,
      0.6822928971605e-07, 0.4337651846959e+01, 0.1554202828031e+00,
      0.6220772380094e-07, 0.6716871369278e+00, 0.1845107853235e+02,
      0.6586950799043e-07, 0.2229714460505e+01, 0.5216580451554e+01,
      0.5873800565771e-07, 0.7627013920580e+00, 0.6398972393349e+00,
      0.6264346929745e-07, 0.6202785478961e+00, 0.6277552955062e+01,
      0.6257929115669e-07, 0.2886775596668e+01, 0.6288598745829e+01,

      0.5343536033409e-07, 0.1977241012051e+01, 0.4690479774488e+01,
      0.5587849781714e-07, 0.1922923484825e+01, 0.1551045220144e+01,
      0.6905100845603e-07, 0.3570757164631e+01, 0.1030928125552e+00,
      0.6178957066649e-07, 0.5197558947765e+01, 0.5230807360890e+01,
      0.6187270224331e-07, 0.8193497368922e+00, 0.5650292065779e+01,
      0.5385664291426e-07, 0.5406336665586e+01, 0.7771377146812e+02,
      0.6329363917926e-07, 0.2837760654536e+01, 0.2608790314060e+02,
      0.4546018761604e-07, 0.2933580297050e+01, 0.5535693017924e+00,
      0.6196091049375e-07, 0.4157871494377e+01, 0.8467247584405e+02,
      0.6159555108218e-07, 0.3211703561703e+01, 0.2394243902548e+03,

      0.4995340539317e-07, 0.1459098102922e+01, 0.4732030630302e+01,
      0.5457031243572e-07, 0.1430457676136e+01, 0.6179983037890e+01,
      0.4863461418397e-07, 0.2196425916730e+01, 0.9027992316901e+02,
      0.5342947626870e-07, 0.2086612890268e+01, 0.6386168663001e+01,
      0.5674296648439e-07, 0.2760204966535e+01, 0.6915859635113e+01,
      0.4745783120161e-07, 0.4245368971862e+01, 0.6282970628506e+01,
      0.4745676961198e-07, 0.5544725787016e+01, 0.6283181072386e+01,
      0.4049796869973e-07, 0.2213984363586e+01, 0.6254626709878e+01,
      0.4248333596940e-07, 0.8075781952896e+00, 0.7875671926403e+01,
      0.4027178070205e-07, 0.1293268540378e+01, 0.6311524991013e+01,

      0.4066543943476e-07, 0.3986141175804e+01, 0.3634620989887e+01,
      0.4858863787880e-07, 0.1276112738231e+01, 0.5760498333002e+01,
      0.5277398263530e-07, 0.4916111741527e+01, 0.2515860172507e+02,
      0.4105635656559e-07, 0.1725805864426e+01, 0.6709674010002e+01,
      0.4376781925772e-07, 0.2243642442106e+01, 0.6805653367890e+01,
      0.3235827894693e-07, 0.3614135118271e+01, 0.1066495398892e+01,
      0.3073244740308e-07, 0.2460873393460e+01, 0.5863591145557e+01,
      0.3088609271373e-07, 0.5678431771790e+01, 0.9917696840332e+01,
      0.3393022279836e-07, 0.3814017477291e+01, 0.1391601904066e+02,
      0.3038686508802e-07, 0.4660216229171e+01, 0.1256621883632e+02,

      0.4019677752497e-07, 0.5906906243735e+01, 0.1334167431096e+02,
      0.3288834998232e-07, 0.9536146445882e+00, 0.1620077269078e+02,
      0.3889973794631e-07, 0.3942205097644e+01, 0.7478166569050e-01,
      0.3050438987141e-07, 0.1624810271286e+01, 0.1805292951336e+02,
      0.3601142564638e-07, 0.4030467142575e+01, 0.6208294184755e+01,
      0.3689015557141e-07, 0.3648878818694e+01, 0.5966683958112e+01,
      0.3563471893565e-07, 0.5749584017096e+01, 0.6357857516136e+01,
      0.2776183170667e-07, 0.2630124187070e+01, 0.3523159621801e-02,
      0.2922350530341e-07, 0.1790346403629e+01, 0.1272157198369e+02,
      0.3511076917302e-07, 0.6142198301611e+01, 0.6599467742779e+01,

      0.3619351007632e-07, 0.1432421386492e+01, 0.6019991944201e+01,
      0.2561254711098e-07, 0.2302822475792e+01, 0.1259245002418e+02,
      0.2626903942920e-07, 0.8660470994571e+00, 0.6702560555334e+01,
      0.2550187397083e-07, 0.6069721995383e+01, 0.1057540660594e+02,
      0.2535873526138e-07, 0.1079020331795e-01, 0.3141537925223e+02,
      0.3519786153847e-07, 0.3809066902283e+01, 0.2505706758577e+03,
      0.3424651492873e-07, 0.2075435114417e+01, 0.6546159756691e+01,
      0.2372676630861e-07, 0.2057803120154e+01, 0.2388894113936e+01,
      0.2710980779541e-07, 0.1510068488010e+01, 0.1202934727411e+02,
      0.3038710889704e-07, 0.5043617528901e+01, 0.1256608456547e+02,

      0.2220364130585e-07, 0.3694793218205e+01, 0.1336244973887e+02,
      0.3025880825460e-07, 0.5450618999049e-01, 0.2908881142201e+02,
      0.2784493486864e-07, 0.3381164084502e+01, 0.1494531617769e+02,
      0.2294414142438e-07, 0.4382309025210e+01, 0.6076890225335e+01,
      0.2012723294724e-07, 0.9142212256518e+00, 0.6262720680387e+01,
      0.2036357831958e-07, 0.5676172293154e+01, 0.4701116388778e+01,
      0.2003474823288e-07, 0.2592767977625e+01, 0.6303431020504e+01,
      0.2207144900109e-07, 0.5404976271180e+01, 0.6489261475556e+01,
      0.2481664905135e-07, 0.4373284587027e+01, 0.1204357418345e+02,
      0.2674949182295e-07, 0.5859182188482e+01, 0.4590910121555e+01,

      0.2450554720322e-07, 0.4555381557451e+01, 0.1495633313810e+00,
      0.2601975986457e-07, 0.3933165584959e+01, 0.1965104848470e+02,
      0.2199860022848e-07, 0.5227977189087e+01, 0.1351787002167e+02,
      0.2448121172316e-07, 0.4858060353949e+01, 0.1162474756779e+01,
      0.1876014864049e-07, 0.5690546553605e+01, 0.6279194432410e+01,
      0.1874513219396e-07, 0.4099539297446e+01, 0.6286957268481e+01,
      0.2156380842559e-07, 0.4382594769913e+00, 0.1813929450232e+02,
      0.1981691240061e-07, 0.1829784152444e+01, 0.4686889479442e+01,
      0.2329992648539e-07, 0.2836254278973e+01, 0.1002183730415e+02,
      0.1765184135302e-07, 0.2803494925833e+01, 0.4292330755499e+01,

      0.2436368366085e-07, 0.2836897959677e+01, 0.9514313292143e+02,
      0.2164089203889e-07, 0.6127522446024e+01, 0.6037244212485e+01,
      0.1847755034221e-07, 0.3683163635008e+01, 0.2427287361862e+00,
      0.1674798769966e-07, 0.3316993867246e+00, 0.1311972100268e+02,
      0.2222542124356e-07, 0.8294097805480e+00, 0.1266924451345e+02,
      0.2071074505925e-07, 0.3659492220261e+01, 0.6528907488406e+01,
      0.1608224471835e-07, 0.4774492067182e+01, 0.1352175143971e+02,
      0.1857583439071e-07, 0.2873120597682e+01, 0.8662240327241e+01,
      0.1793018836159e-07, 0.5282441177929e+00, 0.6819880277225e+01,
      0.1575391221692e-07, 0.1320789654258e+01, 0.1102062672231e+00,

      0.1840132009557e-07, 0.1917110916256e+01, 0.6514761976723e+02,
      0.1760917288281e-07, 0.2972635937132e+01, 0.5746271423666e+01,
      0.1561779518516e-07, 0.4372569261981e+01, 0.6272439236156e+01,
      0.1558687885205e-07, 0.5416424926425e+01, 0.6293712464735e+01,
      0.1951359382579e-07, 0.3094448898752e+01, 0.2301353951334e+02,
      0.1569144275614e-07, 0.2802103689808e+01, 0.1765478049437e+02,
      0.1479130389462e-07, 0.2136435020467e+01, 0.2077542790660e-01,
      0.1467828510764e-07, 0.7072627435674e+00, 0.1052268489556e+01,
      0.1627627337440e-07, 0.3947607143237e+01, 0.6327837846670e+00,
      0.1503498479758e-07, 0.4079248909190e+01, 0.7626583626240e-01,

      0.1297967708237e-07, 0.6269637122840e+01, 0.1149965630200e+02,
      0.1374416896634e-07, 0.4175657970702e+01, 0.6016468784579e+01,
      0.1783812325219e-07, 0.1476540547560e+01, 0.3301902111895e+02,
      0.1525884228756e-07, 0.4653477715241e+01, 0.9411464614024e+01,
      0.1451067396763e-07, 0.2573001128225e+01, 0.1277945078067e+02,
      0.1297713111950e-07, 0.5612799618771e+01, 0.6549682916313e+01,
      0.1462784012820e-07, 0.4189661623870e+01, 0.1863592847156e+02,
      0.1384185980007e-07, 0.2656915472196e+01, 0.2379164476796e+01,
      0.1221497599801e-07, 0.5612515760138e+01, 0.1257326515556e+02,
      0.1560574525896e-07, 0.4783414317919e+01, 0.1887552587463e+02,

      0.1544598372036e-07, 0.2694431138063e+01, 0.1820933031200e+02,
      0.1531678928696e-07, 0.4105103489666e+01, 0.2593412433514e+02,
      0.1349321503795e-07, 0.3082437194015e+00, 0.5120601093667e+01,
      0.1252030290917e-07, 0.6124072334087e+01, 0.6993008899458e+01,
      0.1459243816687e-07, 0.3733103981697e+01, 0.3813291813120e-01,
      0.1226103625262e-07, 0.1267127706817e+01, 0.2435678079171e+02,
      0.1019449641504e-07, 0.4367790112269e+01, 0.1725663147538e+02,
      0.1380789433607e-07, 0.3387201768700e+01, 0.2458316379602e+00,
      0.1019453421658e-07, 0.9204143073737e+00, 0.6112403035119e+01,
      0.1297929434405e-07, 0.5786874896426e+01, 0.1249137003520e+02,

      0.9912677786097e-08, 0.3164232870746e+01, 0.6247047890016e+01,
      0.9829386098599e-08, 0.2586762413351e+01, 0.6453748665772e+01,
      0.1226807746104e-07, 0.6239068436607e+01, 0.5429879531333e+01,
      0.1192691755997e-07, 0.1867380051424e+01, 0.6290122169689e+01,
      0.9836499227081e-08, 0.3424716293727e+00, 0.6319103810876e+01,
      0.9642862564285e-08, 0.5661372990657e+01, 0.8273820945392e+01,
      0.1165184404862e-07, 0.5768367239093e+01, 0.1778273215245e+02,
      0.1175794418818e-07, 0.1657351222943e+01, 0.6276029531202e+01,
      0.1018948635601e-07, 0.6458292350865e+00, 0.1254537627298e+02,
      0.9500383606676e-08, 0.1054306140741e+01, 0.1256517118505e+02,

      0.1227512202906e-07, 0.2505278379114e+01, 0.2248384854122e+02,
      0.9664792009993e-08, 0.4289737277000e+01, 0.6259197520765e+01,
      0.9613285666331e-08, 0.5500597673141e+01, 0.6306954180126e+01,
      0.1117906736211e-07, 0.2361405953468e+01, 0.1779695906178e+02,
      0.9611378640782e-08, 0.2851310576269e+01, 0.2061856251104e+00,
      0.8845354852370e-08, 0.6208777705343e+01, 0.1692165728891e+01,
      0.1054046966600e-07, 0.5413091423934e+01, 0.2204125344462e+00,
      0.1215539124483e-07, 0.5613969479755e+01, 0.8257698122054e+02,
      0.9932460955209e-08, 0.1106124877015e+01, 0.1017725758696e+02,
      0.8785804715043e-08, 0.2869224476477e+01, 0.9491756770005e+00,

      0.8538084097562e-08, 0.6159640899344e+01, 0.6393282117669e+01,
      0.8648994369529e-08, 0.1374901198784e+01, 0.4804209201333e+01,
      0.1039063219067e-07, 0.5171080641327e+01, 0.1550861511662e+02,
      0.8867983926439e-08, 0.8317320304902e+00, 0.3903911373650e+01,
      0.8327495955244e-08, 0.3605591969180e+01, 0.6172869583223e+01,
      0.9243088356133e-08, 0.6114299196843e+01, 0.6267823317922e+01,
      0.9205657357835e-08, 0.3675153683737e+01, 0.6298328382969e+01,
      0.1033269714606e-07, 0.3313328813024e+01, 0.5573142801433e+01,
      0.8001706275552e-08, 0.2019980960053e+01, 0.2648454860559e+01,
      0.9171858254191e-08, 0.8992015524177e+00, 0.1498544001348e+03,

      0.1075327150242e-07, 0.2898669963648e+01, 0.3694923081589e+02,
      0.9884866689828e-08, 0.4946715904478e+01, 0.1140367694411e+02,
      0.9541835576677e-08, 0.2371787888469e+01, 0.1256713221673e+02,
      0.7739903376237e-08, 0.2213775190612e+01, 0.7834121070590e+01,
      0.7311962684106e-08, 0.3429378787739e+01, 0.1192625446156e+02,
      0.9724904869624e-08, 0.6195878564404e+01, 0.2280573557157e+02,
      0.9251628983612e-08, 0.6511509527390e+00, 0.2787043132925e+01,
      0.7320763787842e-08, 0.6001083639421e+01, 0.6282655592598e+01,
      0.7320296650962e-08, 0.3789073265087e+01, 0.6283496108294e+01,
      0.7947032271039e-08, 0.1059659582204e+01, 0.1241073141809e+02,

      0.9005277053115e-08, 0.1280315624361e+01, 0.6281591679874e+01,
      0.8995601652048e-08, 0.2224439106766e+01, 0.6284560021018e+01,
      0.8288040568796e-08, 0.5234914433867e+01, 0.1241658836951e+02,
      0.6359381347255e-08, 0.4137989441490e+01, 0.1596186371003e+01,
      0.8699572228626e-08, 0.1758411009497e+01, 0.6133512519065e+01,
      0.6456797542736e-08, 0.5919285089994e+01, 0.1685848245639e+02,
      0.7424573475452e-08, 0.5414616938827e+01, 0.4061219149443e+01,
      0.7235671196168e-08, 0.1496516557134e+01, 0.1610006857377e+03,
      0.8104015182733e-08, 0.1919918242764e+01, 0.8460828644453e+00,
      0.8098576535937e-08, 0.3819615855458e+01, 0.3894181736510e+01,

      0.6275292346625e-08, 0.6244264115141e+01, 0.8531963191132e+00,
      0.6052432989112e-08, 0.5037731872610e+00, 0.1567108171867e+02,
      0.5705651535817e-08, 0.2984557271995e+01, 0.1258692712880e+02,
      0.5789650115138e-08, 0.6087038140697e+01, 0.1193336791622e+02,
      0.5512132153377e-08, 0.5855668994076e+01, 0.1232342296471e+02,
      0.7388890819102e-08, 0.2443128574740e+01, 0.4907302013889e+01,
      0.5467593991798e-08, 0.3017561234194e+01, 0.1884211409667e+02,
      0.6388519802999e-08, 0.5887386712935e+01, 0.5217580628120e+02,
      0.6106777149944e-08, 0.3483461059895e+00, 0.1422690933580e-01,
      0.7383420275489e-08, 0.5417387056707e+01, 0.2358125818164e+02,

      0.5505208141738e-08, 0.2848193644783e+01, 0.1151388321134e+02,
      0.6310757462877e-08, 0.2349882520828e+01, 0.1041998632314e+02,
      0.6166904929691e-08, 0.5728575944077e+00, 0.6151533897323e+01,
      0.5263442042754e-08, 0.4495796125937e+01, 0.1885275071096e+02,
      0.5591828082629e-08, 0.1355441967677e+01, 0.4337116142245e+00,
      0.5397051680497e-08, 0.1673422864307e+01, 0.6286362197481e+01,
      0.5396992745159e-08, 0.1833502206373e+01, 0.6279789503410e+01,
      0.6572913000726e-08, 0.3331122065824e+01, 0.1176433076753e+02,
      0.5123421866413e-08, 0.2165327142679e+01, 0.1245594543367e+02,
      0.5930495725999e-08, 0.2931146089284e+01, 0.6414617803568e+01,

      0.6431797403933e-08, 0.4134407994088e+01, 0.1350651127443e+00,
      0.5003182207604e-08, 0.3805420303749e+01, 0.1096996532989e+02,
      0.5587731032504e-08, 0.1082469260599e+01, 0.6062663316000e+01,
      0.5935263407816e-08, 0.8384333678401e+00, 0.5326786718777e+01,
      0.4756019827760e-08, 0.3552588749309e+01, 0.3104930017775e+01,
      0.6599951172637e-08, 0.4320826409528e+01, 0.4087944051283e+02,
      0.5902606868464e-08, 0.4811879454445e+01, 0.5849364236221e+01,
      0.5921147809031e-08, 0.9942628922396e-01, 0.1581959461667e+01,
      0.5505382581266e-08, 0.2466557607764e+01, 0.6503488384892e+01,
      0.5353771071862e-08, 0.4551978748683e+01, 0.1735668374386e+03,

      0.5063282210946e-08, 0.5710812312425e+01, 0.1248988586463e+02,
      0.5926120403383e-08, 0.1333998428358e+01, 0.2673594526851e+02,
      0.5211016176149e-08, 0.4649315360760e+01, 0.2460261242967e+02,
      0.5347075084894e-08, 0.5512754081205e+01, 0.4171425416666e+01,
      0.4872609773574e-08, 0.1308025299938e+01, 0.5333900173445e+01,
      0.4727711321420e-08, 0.2144908368062e+01, 0.7232251527446e+01,
      0.6029426018652e-08, 0.5567259412084e+01, 0.3227113045244e+03,
      0.4321485284369e-08, 0.5230667156451e+01, 0.9388005868221e+01,
      0.4476406760553e-08, 0.6134081115303e+01, 0.5547199253223e+01,
      0.5835268277420e-08, 0.4783808492071e+01, 0.7285056171570e+02,

      0.5172183602748e-08, 0.5161817911099e+01, 0.1884570439172e+02,
      0.5693571465184e-08, 0.1381646203111e+01, 0.9723862754494e+02,
      0.4060634965349e-08, 0.3876705259495e+00, 0.4274518229222e+01,
      0.3967398770473e-08, 0.5029491776223e+01, 0.3496032717521e+01,
      0.3943754005255e-08, 0.1923162955490e+01, 0.6244942932314e+01,
      0.4781323427824e-08, 0.4633332586423e+01, 0.2929661536378e+02,
      0.3871483781204e-08, 0.1616650009743e+01, 0.6321208768577e+01,
      0.5141741733997e-08, 0.9817316704659e-01, 0.1232032006293e+02,
      0.4002385978497e-08, 0.3656161212139e+01, 0.7018952447668e+01,
      0.4901092604097e-08, 0.4404098713092e+01, 0.1478866649112e+01,

      0.3740932630345e-08, 0.5181188732639e+00, 0.6922973089781e+01,
      0.4387283718538e-08, 0.3254859566869e+01, 0.2331413144044e+03,
      0.5019197802033e-08, 0.3086773224677e+01, 0.1715706182245e+02,
      0.3834931695175e-08, 0.2797882673542e+01, 0.1491901785440e+02,
      0.3760413942497e-08, 0.2892676280217e+01, 0.1726726808967e+02,
      0.3719717204628e-08, 0.5861046025739e+01, 0.6297302759782e+01,
      0.4145623530149e-08, 0.2168239627033e+01, 0.1376059875786e+02,
      0.3932788425380e-08, 0.6271811124181e+01, 0.7872148766781e+01,
      0.3686377476857e-08, 0.3936853151404e+01, 0.6268848941110e+01,
      0.3779077950339e-08, 0.1404148734043e+01, 0.4157198507331e+01,

      0.4091334550598e-08, 0.2452436180854e+01, 0.9779108567966e+01,
      0.3926694536146e-08, 0.6102292739040e+01, 0.1098419223922e+02,
      0.4841000253289e-08, 0.6072760457276e+01, 0.1252801878276e+02,
      0.4949340130240e-08, 0.1154832815171e+01, 0.1617106187867e+03,
      0.3761557737360e-08, 0.5527545321897e+01, 0.3185192151914e+01,
      0.3647396268188e-08, 0.1525035688629e+01, 0.6271346477544e+01,
      0.3932405074189e-08, 0.5570681040569e+01, 0.2139354194808e+02,
      0.3631322501141e-08, 0.1981240601160e+01, 0.6294805223347e+01,
      0.4130007425139e-08, 0.2050060880201e+01, 0.2195415756911e+02,
      0.4433905965176e-08, 0.3277477970321e+01, 0.7445550607224e+01,

      0.3851814176947e-08, 0.5210690074886e+01, 0.9562891316684e+00,
      0.3485807052785e-08, 0.6653274904611e+00, 0.1161697602389e+02,
      0.3979772816991e-08, 0.1767941436148e+01, 0.2277943724828e+02,
      0.3402607460500e-08, 0.3421746306465e+01, 0.1087398597200e+02,
      0.4049993000926e-08, 0.1127144787547e+01, 0.3163918923335e+00,
      0.3420511182382e-08, 0.4214794779161e+01, 0.1362553364512e+02,
      0.3640772365012e-08, 0.5324905497687e+01, 0.1725304118033e+02,
      0.3323037987501e-08, 0.6135761838271e+01, 0.6279143387820e+01,
      0.4503141663637e-08, 0.1802305450666e+01, 0.1385561574497e+01,
      0.4314560055588e-08, 0.4812299731574e+01, 0.4176041334900e+01,

      0.3294226949110e-08, 0.3657547059723e+01, 0.6287008313071e+01,
      0.3215657197281e-08, 0.4866676894425e+01, 0.5749861718712e+01,
      0.4129362656266e-08, 0.3809342558906e+01, 0.5905702259363e+01,
      0.3137762976388e-08, 0.2494635174443e+01, 0.2099539292909e+02,
      0.3514010952384e-08, 0.2699961831678e+01, 0.7335344340001e+01,
      0.3327607571530e-08, 0.3318457714816e+01, 0.5436992986000e+01,
      0.3541066946675e-08, 0.4382703582466e+01, 0.1234573916645e+02,
      0.3216179847052e-08, 0.5271066317054e+01, 0.3802769619140e-01,
      0.2959045059570e-08, 0.5819591585302e+01, 0.2670964694522e+02,
      0.3884040326665e-08, 0.5980934960428e+01, 0.6660449441528e+01,

      0.2922027539886e-08, 0.3337290282483e+01, 0.1375773836557e+01,
      0.4110846382042e-08, 0.5742978187327e+01, 0.4480965020977e+02,
      0.2934508411032e-08, 0.2278075804200e+01, 0.6408777551755e+00,
      0.3966896193000e-08, 0.5835747858477e+01, 0.3773735910827e+00,
      0.3286695827610e-08, 0.5838898193902e+01, 0.3932462625300e-02,
      0.3720643094196e-08, 0.1122212337858e+01, 0.1646033343740e+02,
      0.3285508906174e-08, 0.9182250996416e+00, 0.1081813534213e+02,
      0.3753880575973e-08, 0.5174761973266e+01, 0.5642198095270e+01,
      0.3022129385587e-08, 0.3381611020639e+01, 0.2982630633589e+02,
      0.2798569205621e-08, 0.3546193723922e+01, 0.1937891852345e+02,

      0.3397872070505e-08, 0.4533203197934e+01, 0.6923953605621e+01,
      0.3708099772977e-08, 0.2756168198616e+01, 0.3066615496545e+02,
      0.3599283541510e-08, 0.1934395469918e+01, 0.6147450479709e+01,
      0.3688702753059e-08, 0.7149920971109e+00, 0.2636725487657e+01,
      0.2681084724003e-08, 0.4899819493154e+01, 0.6816289982179e+01,
      0.3495993460759e-08, 0.1572418915115e+01, 0.6418701221183e+01,
      0.3130770324995e-08, 0.8912190180489e+00, 0.1235996607578e+02,
      0.2744353821941e-08, 0.3800821940055e+01, 0.2059724391010e+02,
      0.2842732906341e-08, 0.2644717440029e+01, 0.2828699048865e+02,
      0.3046882682154e-08, 0.3987793020179e+01, 0.6055599646783e+01,

      0.2399072455143e-08, 0.9908826440764e+00, 0.6255674361143e+01,
      0.2384306274204e-08, 0.2516149752220e+01, 0.6310477339748e+01,
      0.2977324500559e-08, 0.5849195642118e+01, 0.1652265972112e+02,
      0.3062835258972e-08, 0.1681660100162e+01, 0.1172006883645e+02,
      0.3109682589231e-08, 0.5804143987737e+00, 0.2751146787858e+02,
      0.2903920355299e-08, 0.5800768280123e+01, 0.6510552054109e+01,
      0.2823221989212e-08, 0.9241118370216e+00, 0.5469525544182e+01,
      0.3187949696649e-08, 0.3139776445735e+01, 0.1693792562116e+03,
      0.2922559771655e-08, 0.3549440782984e+01, 0.2630839062450e+00,
      0.2436302066603e-08, 0.4735540696319e+01, 0.3946258593675e+00,

      0.3049473043606e-08, 0.4998289124561e+01, 0.8390110365991e+01,
      0.2863682575784e-08, 0.6709515671102e+00, 0.2243449970715e+00,
      0.2641750517966e-08, 0.5410978257284e+01, 0.2986433403208e+02,
      0.2704093466243e-08, 0.4778317207821e+01, 0.6129297044991e+01,
      0.2445522177011e-08, 0.6009020662222e+01, 0.1171295538178e+02,
      0.2623608810230e-08, 0.5010449777147e+01, 0.6436854655901e+01,
      0.2079259704053e-08, 0.5980943768809e+01, 0.2019909489111e+02,
      0.2820225596771e-08, 0.2679965110468e+01, 0.5934151399930e+01,
      0.2365221950927e-08, 0.1894231148810e+01, 0.2470570524223e+02,
      0.2359682077149e-08, 0.4220752950780e+01, 0.8671969964381e+01,

      0.2387577137206e-08, 0.2571783940617e+01, 0.7096626156709e+01,
      0.1982102089816e-08, 0.5169765997119e+00, 0.1727188400790e+02,
      0.2687502389925e-08, 0.6239078264579e+01, 0.7075506709219e+02,
      0.2207751669135e-08, 0.2031184412677e+01, 0.4377611041777e+01,
      0.2618370214274e-08, 0.8266079985979e+00, 0.6632000300961e+01,
      0.2591951887361e-08, 0.8819350522008e+00, 0.4873985990671e+02,
      0.2375055656248e-08, 0.3520944177789e+01, 0.1590676413561e+02,
      0.2472019978911e-08, 0.1551431908671e+01, 0.6612329252343e+00,
      0.2368157127199e-08, 0.4178610147412e+01, 0.3459636466239e+02,
      0.1764846605693e-08, 0.1506764000157e+01, 0.1980094587212e+02,

      0.2291769608798e-08, 0.2118250611782e+01, 0.2844914056730e-01,
      0.2209997316943e-08, 0.3363255261678e+01, 0.2666070658668e+00,
      0.2292699097923e-08, 0.4200423956460e+00, 0.1484170571900e-02,
      0.1629683015329e-08, 0.2331362582487e+01, 0.3035599730800e+02,
      0.2206492862426e-08, 0.3400274026992e+01, 0.6281667977667e+01,
      0.2205746568257e-08, 0.1066051230724e+00, 0.6284483723224e+01,
      0.2026310767991e-08, 0.2779066487979e+01, 0.2449240616245e+02,
      0.1762977622163e-08, 0.9951450691840e+00, 0.2045286941806e+02,
      0.1368535049606e-08, 0.6402447365817e+00, 0.2473415438279e+02,
      0.1720598775450e-08, 0.2303524214705e+00, 0.1679593901136e+03,

      0.1702429015449e-08, 0.6164622655048e+01, 0.3338575901272e+03,
      0.1414033197685e-08, 0.3954561185580e+01, 0.1624205518357e+03,
      0.1573768958043e-08, 0.2028286308984e+01, 0.3144167757552e+02,
      0.1650705184447e-08, 0.2304040666128e+01, 0.5267006960365e+02,
      0.1651087618855e-08, 0.2538461057280e+01, 0.8956999012000e+02,
      0.1616409518983e-08, 0.5111054348152e+01, 0.3332657872986e+02,
      0.1537175173581e-08, 0.5601130666603e+01, 0.3852657435933e+02,
      0.1593191980553e-08, 0.2614340453411e+01, 0.2282781046519e+03,
      0.1499480170643e-08, 0.3624721577264e+01, 0.2823723341956e+02,
      0.1493807843235e-08, 0.4214569879008e+01, 0.2876692439167e+02,

      0.1074571199328e-08, 0.1496911744704e+00, 0.8397383534231e+02,
      0.1074406983417e-08, 0.1187817671922e+01, 0.8401985929482e+02,
      0.9757576855851e-09, 0.2655703035858e+01, 0.7826370942180e+02,
      0.1258432887565e-08, 0.4969896184844e+01, 0.3115650189215e+03,
      0.1240336343282e-08, 0.5192460776926e+01, 0.1784300471910e+03,
      0.9016107005164e-09, 0.1960356923057e+01, 0.5886454391678e+02,
      0.1135392360918e-08, 0.5082427809068e+01, 0.7842370451713e+02,
      0.9216046089565e-09, 0.2793775037273e+01, 0.1014262087719e+03,
      0.1061276615030e-08, 0.3726144311409e+01, 0.5660027930059e+02,
      0.1010110596263e-08, 0.7404080708937e+00, 0.4245678405627e+02,

      0.7217424756199e-09, 0.2697449980577e-01, 0.2457074661053e+03,
      0.6912003846756e-09, 0.4253296276335e+01, 0.1679936946371e+03,
      0.6871814664847e-09, 0.5148072412354e+01, 0.6053048899753e+02,
      0.4887158016343e-09, 0.2153581148294e+01, 0.9656299901946e+02,
      0.5161802866314e-09, 0.3852750634351e+01, 0.2442876000072e+03,
      0.5652599559057e-09, 0.1233233356270e+01, 0.8365903305582e+02,
      0.4710812608586e-09, 0.5610486976767e+01, 0.3164282286739e+03,
      0.4909977500324e-09, 0.1639629524123e+01, 0.4059982187939e+03,
      0.4772641839378e-09, 0.3737100368583e+01, 0.1805255418145e+03,
      0.4487562567153e-09, 0.1158417054478e+00, 0.8433466158131e+02,

      0.3943441230497e-09, 0.6243502862796e+00, 0.2568537517081e+03,
      0.3952236913598e-09, 0.3510377382385e+01, 0.2449975330562e+03,
      0.3788898363417e-09, 0.5916128302299e+01, 0.1568131045107e+03,
      0.3738329328831e-09, 0.1042266763456e+01, 0.3948519331910e+03,
      0.2451199165151e-09, 0.1166788435700e+01, 0.1435713242844e+03,
      0.2436734402904e-09, 0.3254726114901e+01, 0.2268582385539e+03,
      0.2213605274325e-09, 0.1687210598530e+01, 0.1658638954901e+03,
      0.1491521204829e-09, 0.2657541786794e+01, 0.2219950288015e+03,
      0.1474995329744e-09, 0.5013089805819e+01, 0.3052819430710e+03,
      0.1661939475656e-09, 0.5495315428418e+01, 0.2526661704812e+03,

      0.9015946748003e-10, 0.2236989966505e+01, 0.4171445043968e+03 };

/* Sun-to-Earth, T^0, Z */
   static const double e0z[] = {
      0.2796207639075e-05, 0.3198701560209e+01, 0.8433466158131e+02,
      0.1016042198142e-05, 0.5422360395913e+01, 0.5507553240374e+01,
      0.8044305033647e-06, 0.3880222866652e+01, 0.5223693906222e+01,
      0.4385347909274e-06, 0.3704369937468e+01, 0.2352866153506e+01,
      0.3186156414906e-06, 0.3999639363235e+01, 0.1577343543434e+01,
      0.2272412285792e-06, 0.3984738315952e+01, 0.1047747311755e+01,
      0.1645620103007e-06, 0.3565412516841e+01, 0.5856477690889e+01,
      0.1815836921166e-06, 0.4984507059020e+01, 0.6283075850446e+01,
      0.1447461676364e-06, 0.3702753570108e+01, 0.9437762937313e+01,
      0.1430760876382e-06, 0.3409658712357e+01, 0.1021328554739e+02,

      0.1120445753226e-06, 0.4829561570246e+01, 0.1414349524433e+02,
      0.1090232840797e-06, 0.2080729178066e+01, 0.6812766822558e+01,
      0.9715727346551e-07, 0.3476295881948e+01, 0.4694002934110e+01,
      0.1036267136217e-06, 0.4056639536648e+01, 0.7109288135493e+02,
      0.8752665271340e-07, 0.4448159519911e+01, 0.5753384878334e+01,
      0.8331864956004e-07, 0.4991704044208e+01, 0.7084896783808e+01,
      0.6901658670245e-07, 0.4325358994219e+01, 0.6275962395778e+01,
      0.9144536848998e-07, 0.1141826375363e+01, 0.6620890113188e+01,
      0.7205085037435e-07, 0.3624344170143e+01, 0.5296909721118e+00,
      0.7697874654176e-07, 0.5554257458998e+01, 0.1676215758509e+03,

      0.5197545738384e-07, 0.6251760961735e+01, 0.1807370494127e+02,
      0.5031345378608e-07, 0.2497341091913e+01, 0.4705732307012e+01,
      0.4527110205840e-07, 0.2335079920992e+01, 0.6309374173736e+01,
      0.4753355798089e-07, 0.7094148987474e+00, 0.5884926831456e+01,
      0.4296951977516e-07, 0.1101916352091e+01, 0.6681224869435e+01,
      0.3855341568387e-07, 0.1825495405486e+01, 0.5486777812467e+01,
      0.5253930970990e-07, 0.4424740687208e+01, 0.7860419393880e+01,
      0.4024630496471e-07, 0.5120498157053e+01, 0.1336797263425e+02,
      0.4061069791453e-07, 0.6029771435451e+01, 0.3930209696940e+01,
      0.3797883804205e-07, 0.4435193600836e+00, 0.3154687086868e+01,

      0.2933033225587e-07, 0.5124157356507e+01, 0.1059381944224e+01,
      0.3503000930426e-07, 0.5421830162065e+01, 0.6069776770667e+01,
      0.3670096214050e-07, 0.4582101667297e+01, 0.1219403291462e+02,
      0.2905609437008e-07, 0.1926566420072e+01, 0.1097707878456e+02,
      0.2466827821713e-07, 0.6090174539834e+00, 0.6496374930224e+01,
      0.2691647295332e-07, 0.1393432595077e+01, 0.2200391463820e+02,
      0.2150554667946e-07, 0.4308671715951e+01, 0.5643178611111e+01,
      0.2237481922680e-07, 0.8133968269414e+00, 0.8635942003952e+01,
      0.1817741038157e-07, 0.3755205127454e+01, 0.3340612434717e+01,
      0.2227820762132e-07, 0.2759558596664e+01, 0.1203646072878e+02,

      0.1944713772307e-07, 0.5699645869121e+01, 0.1179062909082e+02,
      0.1527340520662e-07, 0.1986749091746e+01, 0.3981490189893e+00,
      0.1577282574914e-07, 0.3205017217983e+01, 0.5088628793478e+01,
      0.1424738825424e-07, 0.6256747903666e+01, 0.2544314396739e+01,
      0.1616563121701e-07, 0.2601671259394e+00, 0.1729818233119e+02,
      0.1401210391692e-07, 0.4686939173506e+01, 0.7058598460518e+01,
      0.1488726974214e-07, 0.2815862451372e+01, 0.2593412433514e+02,
      0.1692626442388e-07, 0.4956894109797e+01, 0.1564752902480e+03,
      0.1123571582910e-07, 0.2381192697696e+01, 0.3738761453707e+01,
      0.9903308606317e-08, 0.4294851657684e+01, 0.9225539266174e+01,

      0.9174533187191e-08, 0.3075171510642e+01, 0.4164311961999e+01,
      0.8645985631457e-08, 0.5477534821633e+00, 0.8429241228195e+01,
     -0.1085876492688e-07, 0.0000000000000e+00, 0.0000000000000e+00,
      0.9264309077815e-08, 0.5968571670097e+01, 0.7079373888424e+01,
      0.8243116984954e-08, 0.1489098777643e+01, 0.1044738781244e+02,
      0.8268102113708e-08, 0.3512977691983e+01, 0.1150676975667e+02,
      0.9043613988227e-08, 0.1290704408221e+00, 0.1101510648075e+02,
      0.7432912038789e-08, 0.1991086893337e+01, 0.2608790314060e+02,
      0.8586233727285e-08, 0.4238357924414e+01, 0.2986433403208e+02,
      0.7612230060131e-08, 0.2911090150166e+01, 0.4732030630302e+01,

      0.7097787751408e-08, 0.1908938392390e+01, 0.8031092209206e+01,
      0.7640237040175e-08, 0.6129219000168e+00, 0.7962980379786e+00,
      0.7070445688081e-08, 0.1380417036651e+01, 0.2146165377750e+01,
      0.7690770957702e-08, 0.1680504249084e+01, 0.2122839202813e+02,
      0.8051292542594e-08, 0.5127423484511e+01, 0.2942463415728e+01,
      0.5902709104515e-08, 0.2020274190917e+01, 0.7755226100720e+00,
      0.5134567496462e-08, 0.2606778676418e+01, 0.1256615170089e+02,
      0.5525802046102e-08, 0.1613011769663e+01, 0.8018209333619e+00,
      0.5880724784221e-08, 0.4604483417236e+01, 0.4690479774488e+01,
      0.5211699081370e-08, 0.5718964114193e+01, 0.8827390247185e+01,

      0.4891849573562e-08, 0.3689658932196e+01, 0.2132990797783e+00,
      0.5150246069997e-08, 0.4099769855122e+01, 0.6480980550449e+02,
      0.5102434319633e-08, 0.5660834602509e+01, 0.3379454372902e+02,
      0.5083405254252e-08, 0.9842221218974e+00, 0.4136910472696e+01,
      0.4206562585682e-08, 0.1341363634163e+00, 0.3128388763578e+01,
      0.4663249683579e-08, 0.8130132735866e+00, 0.5216580451554e+01,
      0.4099474416530e-08, 0.5791497770644e+01, 0.4265981595566e+00,
      0.4628251220767e-08, 0.1249802769331e+01, 0.1572083878776e+02,
      0.5024068728142e-08, 0.4795684802743e+01, 0.6290189305114e+01,
      0.5120234327758e-08, 0.3810420387208e+01, 0.5230807360890e+01,

      0.5524029815280e-08, 0.1029264714351e+01, 0.2397622045175e+03,
      0.4757415718860e-08, 0.3528044781779e+01, 0.1649636139783e+02,
      0.3915786131127e-08, 0.5593889282646e+01, 0.1589072916335e+01,
      0.4869053149991e-08, 0.3299636454433e+01, 0.7632943190217e+01,
      0.3649365703729e-08, 0.1286049002584e+01, 0.6206810014183e+01,
      0.3992493949002e-08, 0.3100307589464e+01, 0.2515860172507e+02,
      0.3320247477418e-08, 0.6212683940807e+01, 0.1216800268190e+02,
      0.3287123739696e-08, 0.4699118445928e+01, 0.7234794171227e+01,
      0.3472776811103e-08, 0.2630507142004e+01, 0.7342457794669e+01,
      0.3423253294767e-08, 0.2946432844305e+01, 0.9623688285163e+01,

      0.3896173898244e-08, 0.1224834179264e+01, 0.6438496133249e+01,
      0.3388455337924e-08, 0.1543807616351e+01, 0.1494531617769e+02,
      0.3062704716523e-08, 0.1191777572310e+01, 0.8662240327241e+01,
      0.3270075600400e-08, 0.5483498767737e+01, 0.1194447056968e+01,
      0.3101209215259e-08, 0.8000833804348e+00, 0.3772475342596e+02,
      0.2780883347311e-08, 0.4077980721888e+00, 0.5863591145557e+01,
      0.2903605931824e-08, 0.2617490302147e+01, 0.1965104848470e+02,
      0.2682014743119e-08, 0.2634703158290e+01, 0.7238675589263e+01,
      0.2534360108492e-08, 0.6102446114873e+01, 0.6836645152238e+01,
      0.2392564882509e-08, 0.3681820208691e+01, 0.5849364236221e+01,

      0.2656667254856e-08, 0.6216045388886e+01, 0.6133512519065e+01,
      0.2331242096773e-08, 0.5864949777744e+01, 0.4535059491685e+01,
      0.2287898363668e-08, 0.4566628532802e+01, 0.7477522907414e+01,
      0.2336944521306e-08, 0.2442722126930e+01, 0.1137170464392e+02,
      0.3156632236269e-08, 0.1626628050682e+01, 0.2509084901204e+03,
      0.2982612402766e-08, 0.2803604512609e+01, 0.1748016358760e+01,
      0.2774031674807e-08, 0.4654002897158e+01, 0.8223916695780e+02,
      0.2295236548638e-08, 0.4326518333253e+01, 0.3378142627421e+00,
      0.2190714699873e-08, 0.4519614578328e+01, 0.2908881142201e+02,
      0.2191495845045e-08, 0.3012626912549e+01, 0.1673046366289e+02,

      0.2492901628386e-08, 0.1290101424052e+00, 0.1543797956245e+03,
      0.1993778064319e-08, 0.3864046799414e+01, 0.1778984560711e+02,
      0.1898146479022e-08, 0.5053777235891e+01, 0.2042657109477e+02,
      0.1918280127634e-08, 0.2222470192548e+01, 0.4165496312290e+02,
      0.1916351061607e-08, 0.8719067257774e+00, 0.7737595720538e+02,
      0.1834720181466e-08, 0.4031491098040e+01, 0.2358125818164e+02,
      0.1249201523806e-08, 0.5938379466835e+01, 0.3301902111895e+02,
      0.1477304050539e-08, 0.6544722606797e+00, 0.9548094718417e+02,
      0.1264316431249e-08, 0.2059072853236e+01, 0.8399684731857e+02,
      0.1203526495039e-08, 0.3644813532605e+01, 0.4558517281984e+02,

      0.9221681059831e-09, 0.3241815055602e+01, 0.7805158573086e+02,
      0.7849278367646e-09, 0.5043812342457e+01, 0.5217580628120e+02,
      0.7983392077387e-09, 0.5000024502753e+01, 0.1501922143975e+03,
      0.7925395431654e-09, 0.1398734871821e-01, 0.9061773743175e+02,
      0.7640473285886e-09, 0.5067111723130e+01, 0.4951538251678e+02,
      0.5398937754482e-09, 0.5597382200075e+01, 0.1613385000004e+03,
      0.5626247550193e-09, 0.2601338209422e+01, 0.7318837597844e+02,
      0.5525197197855e-09, 0.5814832109256e+01, 0.1432335100216e+03,
      0.5407629837898e-09, 0.3384820609076e+01, 0.3230491187871e+03,
      0.3856739119801e-09, 0.1072391840473e+01, 0.2334791286671e+03,

      0.3856425239987e-09, 0.2369540393327e+01, 0.1739046517013e+03,
      0.4350867755983e-09, 0.5255575751082e+01, 0.1620484330494e+03,
      0.3844113924996e-09, 0.5482356246182e+01, 0.9757644180768e+02,
      0.2854869155431e-09, 0.9573634763143e+00, 0.1697170704744e+03,
      0.1719227671416e-09, 0.1887203025202e+01, 0.2265204242912e+03,
      0.1527846879755e-09, 0.3982183931157e+01, 0.3341954043900e+03,
      0.1128229264847e-09, 0.2787457156298e+01, 0.3119028331842e+03 };

/* Sun-to-Earth, T^1, X */
   static const double e1x[] = {
      0.1234046326004e-05, 0.0000000000000e+00, 0.0000000000000e+00,
      0.5150068824701e-06, 0.6002664557501e+01, 0.1256615170089e+02,
      0.1290743923245e-07, 0.5959437664199e+01, 0.1884922755134e+02,
      0.1068615564952e-07, 0.2015529654209e+01, 0.6283075850446e+01,
      0.2079619142538e-08, 0.1732960531432e+01, 0.6279552690824e+01,
      0.2078009243969e-08, 0.4915604476996e+01, 0.6286599010068e+01,
      0.6206330058856e-09, 0.3616457953824e+00, 0.4705732307012e+01,
      0.5989335313746e-09, 0.3802607304474e+01, 0.6256777527156e+01,
      0.5958495663840e-09, 0.2845866560031e+01, 0.6309374173736e+01,
      0.4866923261539e-09, 0.5213203771824e+01, 0.7755226100720e+00,

      0.4267785823142e-09, 0.4368189727818e+00, 0.1059381944224e+01,
      0.4610675141648e-09, 0.1837249181372e-01, 0.7860419393880e+01,
      0.3626989993973e-09, 0.2161590545326e+01, 0.5753384878334e+01,
      0.3563071194389e-09, 0.1452631954746e+01, 0.5884926831456e+01,
      0.3557015642807e-09, 0.4470593393054e+01, 0.6812766822558e+01,
      0.3210412089122e-09, 0.5195926078314e+01, 0.6681224869435e+01,
      0.2875473577986e-09, 0.5916256610193e+01, 0.2513230340178e+02,
      0.2842913681629e-09, 0.1149902426047e+01, 0.6127655567643e+01,
      0.2751248215916e-09, 0.5502088574662e+01, 0.6438496133249e+01,
      0.2481432881127e-09, 0.2921989846637e+01, 0.5486777812467e+01,

      0.2059885976560e-09, 0.3718070376585e+01, 0.7079373888424e+01,
      0.2015522342591e-09, 0.5979395259740e+01, 0.6290189305114e+01,
      0.1995364084253e-09, 0.6772087985494e+00, 0.6275962395778e+01,
      0.1957436436943e-09, 0.2899210654665e+01, 0.5507553240374e+01,
      0.1651609818948e-09, 0.6228206482192e+01, 0.1150676975667e+02,
      0.1822980550699e-09, 0.1469348746179e+01, 0.1179062909082e+02,
      0.1675223159760e-09, 0.3813910555688e+01, 0.7058598460518e+01,
      0.1706491764745e-09, 0.3004380506684e+00, 0.7113454667900e-02,
      0.1392952362615e-09, 0.1440393973406e+01, 0.7962980379786e+00,
      0.1209868266342e-09, 0.4150425791727e+01, 0.4694002934110e+01,

      0.1009827202611e-09, 0.3290040429843e+01, 0.3738761453707e+01,
      0.1047261388602e-09, 0.4229590090227e+01, 0.6282095334605e+01,
      0.1047006652004e-09, 0.2418967680575e+01, 0.6284056366286e+01,
      0.9609993143095e-10, 0.4627943659201e+01, 0.6069776770667e+01,
      0.9590900593873e-10, 0.1894393939924e+01, 0.4136910472696e+01,
      0.9146249188071e-10, 0.2010647519562e+01, 0.6496374930224e+01,
      0.8545274480290e-10, 0.5529846956226e-01, 0.1194447056968e+01,
      0.8224377881194e-10, 0.1254304102174e+01, 0.1589072916335e+01,
      0.6183529510410e-10, 0.3360862168815e+01, 0.8827390247185e+01,
      0.6259255147141e-10, 0.4755628243179e+01, 0.8429241228195e+01,

      0.5539291694151e-10, 0.5371746955142e+01, 0.4933208510675e+01,
      0.7328259466314e-10, 0.4927699613906e+00, 0.4535059491685e+01,
      0.6017835843560e-10, 0.5776682001734e-01, 0.1255903824622e+02,
      0.7079827775243e-10, 0.4395059432251e+01, 0.5088628793478e+01,
      0.5170358878213e-10, 0.5154062619954e+01, 0.1176985366291e+02,
      0.4872301838682e-10, 0.6289611648973e+00, 0.6040347114260e+01,
      0.5249869411058e-10, 0.5617272046949e+01, 0.3154687086868e+01,
      0.4716172354411e-10, 0.3965901800877e+01, 0.5331357529664e+01,
      0.4871214940964e-10, 0.4627507050093e+01, 0.1256967486051e+02,
      0.4598076850751e-10, 0.6023631226459e+01, 0.6525804586632e+01,

      0.4562196089485e-10, 0.4138562084068e+01, 0.3930209696940e+01,
      0.4325493872224e-10, 0.1330845906564e+01, 0.7632943190217e+01,
      0.5673781176748e-10, 0.2558752615657e+01, 0.5729506548653e+01,
      0.3961436642503e-10, 0.2728071734630e+01, 0.7234794171227e+01,
      0.5101868209058e-10, 0.4113444965144e+01, 0.6836645152238e+01,
      0.5257043167676e-10, 0.6195089830590e+01, 0.8031092209206e+01,
      0.5076613989393e-10, 0.2305124132918e+01, 0.7477522907414e+01,
      0.3342169352778e-10, 0.5415998155071e+01, 0.1097707878456e+02,
      0.3545881983591e-10, 0.3727160564574e+01, 0.4164311961999e+01,
      0.3364063738599e-10, 0.2901121049204e+00, 0.1137170464392e+02,

      0.3357039670776e-10, 0.1652229354331e+01, 0.5223693906222e+01,
      0.4307412268687e-10, 0.4938909587445e+01, 0.1592596075957e+01,
      0.3405769115435e-10, 0.2408890766511e+01, 0.3128388763578e+01,
      0.3001926198480e-10, 0.4862239006386e+01, 0.1748016358760e+01,
      0.2778264787325e-10, 0.5241168661353e+01, 0.7342457794669e+01,
      0.2676159480666e-10, 0.3423593942199e+01, 0.2146165377750e+01,
      0.2954273399939e-10, 0.1881721265406e+01, 0.5368044267797e+00,
      0.3309362888795e-10, 0.1931525677349e+01, 0.8018209333619e+00,
      0.2810283608438e-10, 0.2414659495050e+01, 0.5225775174439e+00,
      0.3378045637764e-10, 0.4238019163430e+01, 0.1554202828031e+00,

      0.2558134979840e-10, 0.1828225235805e+01, 0.5230807360890e+01,
      0.2273755578447e-10, 0.5858184283998e+01, 0.7084896783808e+01,
      0.2294176037690e-10, 0.4514589779057e+01, 0.1726015463500e+02,
      0.2533506099435e-10, 0.2355717851551e+01, 0.5216580451554e+01,
      0.2716685375812e-10, 0.2221003625100e+01, 0.8635942003952e+01,
      0.2419043435198e-10, 0.5955704951635e+01, 0.4690479774488e+01,
      0.2521232544812e-10, 0.1395676848521e+01, 0.5481254917084e+01,
      0.2630195021491e-10, 0.5727468918743e+01, 0.2629832328990e-01,
      0.2548395840944e-10, 0.2628351859400e-03, 0.1349867339771e+01 };

/* Sun-to-Earth, T^1, Y */
   static const double e1y[] = {
      0.9304690546528e-06, 0.0000000000000e+00, 0.0000000000000e+00,
      0.5150715570663e-06, 0.4431807116294e+01, 0.1256615170089e+02,
      0.1290825411056e-07, 0.4388610039678e+01, 0.1884922755134e+02,
      0.4645466665386e-08, 0.5827263376034e+01, 0.6283075850446e+01,
      0.2079625310718e-08, 0.1621698662282e+00, 0.6279552690824e+01,
      0.2078189850907e-08, 0.3344713435140e+01, 0.6286599010068e+01,
      0.6207190138027e-09, 0.5074049319576e+01, 0.4705732307012e+01,
      0.5989826532569e-09, 0.2231842216620e+01, 0.6256777527156e+01,
      0.5961360812618e-09, 0.1274975769045e+01, 0.6309374173736e+01,
      0.4874165471016e-09, 0.3642277426779e+01, 0.7755226100720e+00,

      0.4283834034360e-09, 0.5148765510106e+01, 0.1059381944224e+01,
      0.4652389287529e-09, 0.4715794792175e+01, 0.7860419393880e+01,
      0.3751707476401e-09, 0.6617207370325e+00, 0.5753384878334e+01,
      0.3559998806198e-09, 0.6155548875404e+01, 0.5884926831456e+01,
      0.3558447558857e-09, 0.2898827297664e+01, 0.6812766822558e+01,
      0.3211116927106e-09, 0.3625813502509e+01, 0.6681224869435e+01,
      0.2875609914672e-09, 0.4345435813134e+01, 0.2513230340178e+02,
      0.2843109704069e-09, 0.5862263940038e+01, 0.6127655567643e+01,
      0.2744676468427e-09, 0.3926419475089e+01, 0.6438496133249e+01,
      0.2481285237789e-09, 0.1351976572828e+01, 0.5486777812467e+01,

      0.2060338481033e-09, 0.2147556998591e+01, 0.7079373888424e+01,
      0.2015822358331e-09, 0.4408358972216e+01, 0.6290189305114e+01,
      0.2001195944195e-09, 0.5385829822531e+01, 0.6275962395778e+01,
      0.1953667642377e-09, 0.1304933746120e+01, 0.5507553240374e+01,
      0.1839744078713e-09, 0.6173567228835e+01, 0.1179062909082e+02,
      0.1643334294845e-09, 0.4635942997523e+01, 0.1150676975667e+02,
      0.1768051018652e-09, 0.5086283558874e+01, 0.7113454667900e-02,
      0.1674874205489e-09, 0.2243332137241e+01, 0.7058598460518e+01,
      0.1421445397609e-09, 0.6186899771515e+01, 0.7962980379786e+00,
      0.1255163958267e-09, 0.5730238465658e+01, 0.4694002934110e+01,

      0.1013945281961e-09, 0.1726055228402e+01, 0.3738761453707e+01,
      0.1047294335852e-09, 0.2658801228129e+01, 0.6282095334605e+01,
      0.1047103879392e-09, 0.8481047835035e+00, 0.6284056366286e+01,
      0.9530343962826e-10, 0.3079267149859e+01, 0.6069776770667e+01,
      0.9604637611690e-10, 0.3258679792918e+00, 0.4136910472696e+01,
      0.9153518537177e-10, 0.4398599886584e+00, 0.6496374930224e+01,
      0.8562458214922e-10, 0.4772686794145e+01, 0.1194447056968e+01,
      0.8232525360654e-10, 0.5966220721679e+01, 0.1589072916335e+01,
      0.6150223411438e-10, 0.1780985591923e+01, 0.8827390247185e+01,
      0.6272087858000e-10, 0.3184305429012e+01, 0.8429241228195e+01,

      0.5540476311040e-10, 0.3801260595433e+01, 0.4933208510675e+01,
      0.7331901699361e-10, 0.5205948591865e+01, 0.4535059491685e+01,
      0.6018528702791e-10, 0.4770139083623e+01, 0.1255903824622e+02,
      0.5150530724804e-10, 0.3574796899585e+01, 0.1176985366291e+02,
      0.6471933741811e-10, 0.2679787266521e+01, 0.5088628793478e+01,
      0.5317460644174e-10, 0.9528763345494e+00, 0.3154687086868e+01,
      0.4832187748783e-10, 0.5329322498232e+01, 0.6040347114260e+01,
      0.4716763555110e-10, 0.2395235316466e+01, 0.5331357529664e+01,
      0.4871509139861e-10, 0.3056663648823e+01, 0.1256967486051e+02,
      0.4598417696768e-10, 0.4452762609019e+01, 0.6525804586632e+01,

      0.5674189533175e-10, 0.9879680872193e+00, 0.5729506548653e+01,
      0.4073560328195e-10, 0.5939127696986e+01, 0.7632943190217e+01,
      0.5040994945359e-10, 0.4549875824510e+01, 0.8031092209206e+01,
      0.5078185134679e-10, 0.7346659893982e+00, 0.7477522907414e+01,
      0.3769343537061e-10, 0.1071317188367e+01, 0.7234794171227e+01,
      0.4980331365299e-10, 0.2500345341784e+01, 0.6836645152238e+01,
      0.3458236594757e-10, 0.3825159450711e+01, 0.1097707878456e+02,
      0.3578859493602e-10, 0.5299664791549e+01, 0.4164311961999e+01,
      0.3370504646419e-10, 0.5002316301593e+01, 0.1137170464392e+02,
      0.3299873338428e-10, 0.2526123275282e+01, 0.3930209696940e+01,

      0.4304917318409e-10, 0.3368078557132e+01, 0.1592596075957e+01,
      0.3402418753455e-10, 0.8385495425800e+00, 0.3128388763578e+01,
      0.2778460572146e-10, 0.3669905203240e+01, 0.7342457794669e+01,
      0.2782710128902e-10, 0.2691664812170e+00, 0.1748016358760e+01,
      0.2711725179646e-10, 0.4707487217718e+01, 0.5296909721118e+00,
      0.2981760946340e-10, 0.3190260867816e+00, 0.5368044267797e+00,
      0.2811672977772e-10, 0.3196532315372e+01, 0.7084896783808e+01,
      0.2863454474467e-10, 0.2263240324780e+00, 0.5223693906222e+01,
      0.3333464634051e-10, 0.3498451685065e+01, 0.8018209333619e+00,
      0.3312991747609e-10, 0.5839154477412e+01, 0.1554202828031e+00,

      0.2813255564006e-10, 0.8268044346621e+00, 0.5225775174439e+00,
      0.2665098083966e-10, 0.3934021725360e+01, 0.5216580451554e+01,
      0.2349795705216e-10, 0.5197620913779e+01, 0.2146165377750e+01,
      0.2330352293961e-10, 0.2984999231807e+01, 0.1726015463500e+02,
      0.2728001683419e-10, 0.6521679638544e+00, 0.8635942003952e+01,
      0.2484061007669e-10, 0.3468955561097e+01, 0.5230807360890e+01,
      0.2646328768427e-10, 0.1013724533516e+01, 0.2629832328990e-01,
      0.2518630264831e-10, 0.6108081057122e+01, 0.5481254917084e+01,
      0.2421901455384e-10, 0.1651097776260e+01, 0.1349867339771e+01,
      0.6348533267831e-11, 0.3220226560321e+01, 0.8433466158131e+02 };

/* Sun-to-Earth, T^1, Z */
   static const double e1z[] = {
      0.2278290449966e-05, 0.3413716033863e+01, 0.6283075850446e+01,
      0.5429458209830e-07, 0.0000000000000e+00, 0.0000000000000e+00,
      0.1903240492525e-07, 0.3370592358297e+01, 0.1256615170089e+02,
      0.2385409276743e-09, 0.3327914718416e+01, 0.1884922755134e+02,
      0.8676928342573e-10, 0.1824006811264e+01, 0.5223693906222e+01,
      0.7765442593544e-10, 0.3888564279247e+01, 0.5507553240374e+01,
      0.7066158332715e-10, 0.5194267231944e+01, 0.2352866153506e+01,
      0.7092175288657e-10, 0.2333246960021e+01, 0.8399684731857e+02,
      0.5357582213535e-10, 0.2224031176619e+01, 0.5296909721118e+00,
      0.3828035865021e-10, 0.2156710933584e+01, 0.6279552690824e+01,

      0.3824857220427e-10, 0.1529755219915e+01, 0.6286599010068e+01,
      0.3286995181628e-10, 0.4879512900483e+01, 0.1021328554739e+02 };

/* Sun-to-Earth, T^2, X */
   static const double e2x[] = {
     -0.4143818297913e-10, 0.0000000000000e+00, 0.0000000000000e+00,
      0.2171497694435e-10, 0.4398225628264e+01, 0.1256615170089e+02,
      0.9845398442516e-11, 0.2079720838384e+00, 0.6283075850446e+01,
      0.9256833552682e-12, 0.4191264694361e+01, 0.1884922755134e+02,
      0.1022049384115e-12, 0.5381133195658e+01, 0.8399684731857e+02 };

/* Sun-to-Earth, T^2, Y */
   static const double e2y[] = {
      0.5063375872532e-10, 0.0000000000000e+00, 0.0000000000000e+00,
      0.2173815785980e-10, 0.2827805833053e+01, 0.1256615170089e+02,
      0.1010231999920e-10, 0.4634612377133e+01, 0.6283075850446e+01,
      0.9259745317636e-12, 0.2620612076189e+01, 0.1884922755134e+02,
      0.1022202095812e-12, 0.3809562326066e+01, 0.8399684731857e+02 };

/* Sun-to-Earth, T^2, Z */
   static const double e2z[] = {
      0.9722666114891e-10, 0.5152219582658e+01, 0.6283075850446e+01,
     -0.3494819171909e-11, 0.0000000000000e+00, 0.0000000000000e+00,
      0.6713034376076e-12, 0.6440188750495e+00, 0.1256615170089e+02 };

/* SSB-to-Sun, T^0, X */
   static const double s0x[] = {
      0.4956757536410e-02, 0.3741073751789e+01, 0.5296909721118e+00,
      0.2718490072522e-02, 0.4016011511425e+01, 0.2132990797783e+00,
      0.1546493974344e-02, 0.2170528330642e+01, 0.3813291813120e-01,
      0.8366855276341e-03, 0.2339614075294e+01, 0.7478166569050e-01,
      0.2936777942117e-03, 0.0000000000000e+00, 0.0000000000000e+00,
      0.1201317439469e-03, 0.4090736353305e+01, 0.1059381944224e+01,
      0.7578550887230e-04, 0.3241518088140e+01, 0.4265981595566e+00,
      0.1941787367773e-04, 0.1012202064330e+01, 0.2061856251104e+00,
      0.1889227765991e-04, 0.3892520416440e+01, 0.2204125344462e+00,
      0.1937896968613e-04, 0.4797779441161e+01, 0.1495633313810e+00,

      0.1434506110873e-04, 0.3868960697933e+01, 0.5225775174439e+00,
      0.1406659911580e-04, 0.4759766557397e+00, 0.5368044267797e+00,
      0.1179022300202e-04, 0.7774961520598e+00, 0.7626583626240e-01,
      0.8085864460959e-05, 0.3254654471465e+01, 0.3664874755930e-01,
      0.7622752967615e-05, 0.4227633103489e+01, 0.3961708870310e-01,
      0.6209171139066e-05, 0.2791828325711e+00, 0.7329749511860e-01,
      0.4366435633970e-05, 0.4440454875925e+01, 0.1589072916335e+01,
      0.3792124889348e-05, 0.5156393842356e+01, 0.7113454667900e-02,
      0.3154548963402e-05, 0.6157005730093e+01, 0.4194847048887e+00,
      0.3088359882942e-05, 0.2494567553163e+01, 0.6398972393349e+00,

      0.2788440902136e-05, 0.4934318747989e+01, 0.1102062672231e+00,
      0.3039928456376e-05, 0.4895077702640e+01, 0.6283075850446e+01,
      0.2272258457679e-05, 0.5278394064764e+01, 0.1030928125552e+00,
      0.2162007057957e-05, 0.5802978019099e+01, 0.3163918923335e+00,
      0.1767632855737e-05, 0.3415346595193e-01, 0.1021328554739e+02,
      0.1349413459362e-05, 0.2001643230755e+01, 0.1484170571900e-02,
      0.1170141900476e-05, 0.2424750491620e+01, 0.6327837846670e+00,
      0.1054355266820e-05, 0.3123311487576e+01, 0.4337116142245e+00,
      0.9800822461610e-06, 0.3026258088130e+01, 0.1052268489556e+01,
      0.1091203749931e-05, 0.3157811670347e+01, 0.1162474756779e+01,

      0.6960236715913e-06, 0.8219570542313e+00, 0.1066495398892e+01,
      0.5689257296909e-06, 0.1323052375236e+01, 0.9491756770005e+00,
      0.6613172135802e-06, 0.2765348881598e+00, 0.8460828644453e+00,
      0.6277702517571e-06, 0.5794064466382e+01, 0.1480791608091e+00,
      0.6304884066699e-06, 0.7323555380787e+00, 0.2243449970715e+00,
      0.4897850467382e-06, 0.3062464235399e+01, 0.3340612434717e+01,
      0.3759148598786e-06, 0.4588290469664e+01, 0.3516457698740e-01,
      0.3110520548195e-06, 0.1374299536572e+01, 0.6373574839730e-01,
      0.3064708359780e-06, 0.4222267485047e+01, 0.1104591729320e-01,
      0.2856347168241e-06, 0.3714202944973e+01, 0.1510475019529e+00,

      0.2840945514288e-06, 0.2847972875882e+01, 0.4110125927500e-01,
      0.2378951599405e-06, 0.3762072563388e+01, 0.2275259891141e+00,
      0.2714229481417e-06, 0.1036049980031e+01, 0.2535050500000e-01,
      0.2323551717307e-06, 0.4682388599076e+00, 0.8582758298370e-01,
      0.1881790512219e-06, 0.4790565425418e+01, 0.2118763888447e+01,
      0.2261353968371e-06, 0.1669144912212e+01, 0.7181332454670e-01,
      0.2214546389848e-06, 0.3937717281614e+01, 0.2968341143800e-02,
      0.2184915594933e-06, 0.1129169845099e+00, 0.7775000683430e-01,
      0.2000164937936e-06, 0.4030009638488e+01, 0.2093666171530e+00,
      0.1966105136719e-06, 0.8745955786834e+00, 0.2172315424036e+00,

      0.1904742332624e-06, 0.5919743598964e+01, 0.2022531624851e+00,
      0.1657399705031e-06, 0.2549141484884e+01, 0.7358765972222e+00,
      0.1574070533987e-06, 0.5277533020230e+01, 0.7429900518901e+00,
      0.1832261651039e-06, 0.3064688127777e+01, 0.3235053470014e+00,
      0.1733615346569e-06, 0.3011432799094e+01, 0.1385174140878e+00,
      0.1549124014496e-06, 0.4005569132359e+01, 0.5154640627760e+00,
      0.1637044713838e-06, 0.1831375966632e+01, 0.8531963191132e+00,
      0.1123420082383e-06, 0.1180270407578e+01, 0.1990721704425e+00,
      0.1083754165740e-06, 0.3414101320863e+00, 0.5439178814476e+00,
      0.1156638012655e-06, 0.6130479452594e+00, 0.5257585094865e+00,

      0.1142548785134e-06, 0.3724761948846e+01, 0.5336234347371e+00,
      0.7921463895965e-07, 0.2435425589361e+01, 0.1478866649112e+01,
      0.7428600285231e-07, 0.3542144398753e+01, 0.2164800718209e+00,
      0.8323211246747e-07, 0.3525058072354e+01, 0.1692165728891e+01,
      0.7257595116312e-07, 0.1364299431982e+01, 0.2101180877357e+00,
      0.7111185833236e-07, 0.2460478875808e+01, 0.4155522422634e+00,
      0.6868090383716e-07, 0.4397327670704e+01, 0.1173197218910e+00,
      0.7226419974175e-07, 0.4042647308905e+01, 0.1265567569334e+01,
      0.6955642383177e-07, 0.2865047906085e+01, 0.9562891316684e+00,
      0.7492139296331e-07, 0.5014278994215e+01, 0.1422690933580e-01,

      0.6598363128857e-07, 0.2376730020492e+01, 0.6470106940028e+00,
      0.7381147293385e-07, 0.3272990384244e+01, 0.1581959461667e+01,
      0.6402909624032e-07, 0.5302290955138e+01, 0.9597935788730e-01,
      0.6237454263857e-07, 0.5444144425332e+01, 0.7084920306520e-01,
      0.5241198544016e-07, 0.4215359579205e+01, 0.5265099800692e+00,
      0.5144463853918e-07, 0.1218916689916e+00, 0.5328719641544e+00,
      0.5868164772299e-07, 0.2369402002213e+01, 0.7871412831580e-01,
      0.6233195669151e-07, 0.1254922242403e+01, 0.2608790314060e+02,
      0.6068463791422e-07, 0.5679713760431e+01, 0.1114304132498e+00,
      0.4359361135065e-07, 0.6097219641646e+00, 0.1375773836557e+01,

      0.4686510366826e-07, 0.4786231041431e+01, 0.1143987543936e+00,
      0.3758977287225e-07, 0.1167368068139e+01, 0.1596186371003e+01,
      0.4282051974778e-07, 0.1519471064319e+01, 0.2770348281756e+00,
      0.5153765386113e-07, 0.1860532322984e+01, 0.2228608264996e+00,
      0.4575129387188e-07, 0.7632857887158e+00, 0.1465949902372e+00,
      0.3326844933286e-07, 0.1298219485285e+01, 0.5070101000000e-01,
      0.3748617450984e-07, 0.1046510321062e+01, 0.4903339079539e+00,
      0.2816756661499e-07, 0.3434522346190e+01, 0.2991266627620e+00,
      0.3412750405039e-07, 0.2523766270318e+01, 0.3518164938661e+00,
      0.2655796761776e-07, 0.2904422260194e+01, 0.6256703299991e+00,

      0.2963597929458e-07, 0.5923900431149e+00, 0.1099462426779e+00,
      0.2539523734781e-07, 0.4851947722567e+01, 0.1256615170089e+02,
      0.2283087914139e-07, 0.3400498595496e+01, 0.6681224869435e+01,
      0.2321309799331e-07, 0.5789099148673e+01, 0.3368040641550e-01,
      0.2549657649750e-07, 0.3991856479792e-01, 0.1169588211447e+01,
      0.2290462303977e-07, 0.2788567577052e+01, 0.1045155034888e+01,
      0.1945398522914e-07, 0.3290896998176e+01, 0.1155361302111e+01,
      0.1849171512638e-07, 0.2698060129367e+01, 0.4452511715700e-02,
      0.1647199834254e-07, 0.3016735644085e+01, 0.4408250688924e+00,
      0.1529530765273e-07, 0.5573043116178e+01, 0.6521991896920e-01,

      0.1433199339978e-07, 0.1481192356147e+01, 0.9420622223326e+00,
      0.1729134193602e-07, 0.1422817538933e+01, 0.2108507877249e+00,
      0.1716463931346e-07, 0.3469468901855e+01, 0.2157473718317e+00,
      0.1391206061378e-07, 0.6122436220547e+01, 0.4123712502208e+00,
      0.1404746661924e-07, 0.1647765641936e+01, 0.4258542984690e-01,
      0.1410452399455e-07, 0.5989729161964e+01, 0.2258291676434e+00,
      0.1089828772168e-07, 0.2833705509371e+01, 0.4226656969313e+00,
      0.1047374564948e-07, 0.5090690007331e+00, 0.3092784376656e+00,
      0.1358279126532e-07, 0.5128990262836e+01, 0.7923417740620e-01,
      0.1020456476148e-07, 0.9632772880808e+00, 0.1456308687557e+00,

      0.1033428735328e-07, 0.3223779318418e+01, 0.1795258541446e+01,
      0.1412435841540e-07, 0.2410271572721e+01, 0.1525316725248e+00,
      0.9722759371574e-08, 0.2333531395690e+01, 0.8434341241180e-01,
      0.9657334084704e-08, 0.6199270974168e+01, 0.1272681024002e+01,
      0.1083641148690e-07, 0.2864222292929e+01, 0.7032915397480e-01,
      0.1067318403838e-07, 0.5833458866568e+00, 0.2123349582968e+00,
      0.1062366201976e-07, 0.4307753989494e+01, 0.2142632012598e+00,
      0.1236364149266e-07, 0.2873917870593e+01, 0.1847279083684e+00,
      0.1092759489593e-07, 0.2959887266733e+01, 0.1370332435159e+00,
      0.8912069362899e-08, 0.5141213702562e+01, 0.2648454860559e+01,

      0.9656467707970e-08, 0.4532182462323e+01, 0.4376440768498e+00,
      0.8098386150135e-08, 0.2268906338379e+01, 0.2880807454688e+00,
      0.7857714675000e-08, 0.4055544260745e+01, 0.2037373330570e+00,
      0.7288455940646e-08, 0.5357901655142e+01, 0.1129145838217e+00,
      0.9450595950552e-08, 0.4264926963939e+01, 0.5272426800584e+00,
      0.9381718247537e-08, 0.7489366976576e-01, 0.5321392641652e+00,
      0.7079052646038e-08, 0.1923311052874e+01, 0.6288513220417e+00,
      0.9259004415344e-08, 0.2970256853438e+01, 0.1606092486742e+00,
      0.8259801499742e-08, 0.3327056314697e+01, 0.8389694097774e+00,
      0.6476334355779e-08, 0.2954925505727e+01, 0.2008557621224e+01,

      0.5984021492007e-08, 0.9138753105829e+00, 0.2042657109477e+02,
      0.5989546863181e-08, 0.3244464082031e+01, 0.2111650433779e+01,
      0.6233108606023e-08, 0.4995232638403e+00, 0.4305306221819e+00,
      0.6877299149965e-08, 0.2834987233449e+01, 0.9561746721300e-02,
      0.8311234227190e-08, 0.2202951835758e+01, 0.3801276407308e+00,
      0.6599472832414e-08, 0.4478581462618e+01, 0.1063314406849e+01,
      0.6160491096549e-08, 0.5145858696411e+01, 0.1368660381889e+01,
      0.6164772043891e-08, 0.3762976697911e+00, 0.4234171675140e+00,
      0.6363248684450e-08, 0.3162246718685e+01, 0.1253008786510e-01,
      0.6448587520999e-08, 0.3442693302119e+01, 0.5287268506303e+00,

      0.6431662283977e-08, 0.8977549136606e+00, 0.5306550935933e+00,
      0.6351223158474e-08, 0.4306447410369e+01, 0.5217580628120e+02,
      0.5476721393451e-08, 0.3888529177855e+01, 0.2221856701002e+01,
      0.5341772572619e-08, 0.2655560662512e+01, 0.7466759693650e-01,
      0.5337055758302e-08, 0.5164990735946e+01, 0.7489573444450e-01,
      0.5373120816787e-08, 0.6041214553456e+01, 0.1274714967946e+00,
      0.5392351705426e-08, 0.9177763485932e+00, 0.1055449481598e+01,
      0.6688495850205e-08, 0.3089608126937e+01, 0.2213766559277e+00,
      0.5072003660362e-08, 0.4311316541553e+01, 0.2132517061319e+00,
      0.5070726650455e-08, 0.5790675464444e+00, 0.2133464534247e+00,

      0.5658012950032e-08, 0.2703945510675e+01, 0.7287631425543e+00,
      0.4835509924854e-08, 0.2975422976065e+01, 0.7160067364790e-01,
      0.6479821978012e-08, 0.1324168733114e+01, 0.2209183458640e-01,
      0.6230636494980e-08, 0.2860103632836e+01, 0.3306188016693e+00,
      0.4649239516213e-08, 0.4832259763403e+01, 0.7796265773310e-01,
      0.6487325792700e-08, 0.2726165825042e+01, 0.3884652414254e+00,
      0.4682823682770e-08, 0.6966602455408e+00, 0.1073608853559e+01,
      0.5704230804976e-08, 0.5669634104606e+01, 0.8731175355560e-01,
      0.6125413585489e-08, 0.1513386538915e+01, 0.7605151500000e-01,
      0.6035825038187e-08, 0.1983509168227e+01, 0.9846002785331e+00,

      0.4331123462303e-08, 0.2782892992807e+01, 0.4297791515992e+00,
      0.4681107685143e-08, 0.5337232886836e+01, 0.2127790306879e+00,
      0.4669105829655e-08, 0.5837133792160e+01, 0.2138191288687e+00,
      0.5138823602365e-08, 0.3080560200507e+01, 0.7233337363710e-01,
      0.4615856664534e-08, 0.1661747897471e+01, 0.8603097737811e+00,
      0.4496916702197e-08, 0.2112508027068e+01, 0.7381754420900e-01,
      0.4278479042945e-08, 0.5716528462627e+01, 0.7574578717200e-01,
      0.3840525503932e-08, 0.6424172726492e+00, 0.3407705765729e+00,
      0.4866636509685e-08, 0.4919244697715e+01, 0.7722995774390e-01,
      0.3526100639296e-08, 0.2550821052734e+01, 0.6225157782540e-01,

      0.3939558488075e-08, 0.3939331491710e+01, 0.5268983110410e-01,
      0.4041268772576e-08, 0.2275337571218e+01, 0.3503323232942e+00,
      0.3948761842853e-08, 0.1999324200790e+01, 0.1451108196653e+00,
      0.3258394550029e-08, 0.9121001378200e+00, 0.5296435984654e+00,
      0.3257897048761e-08, 0.3428428660869e+01, 0.5297383457582e+00,
      0.3842559031298e-08, 0.6132927720035e+01, 0.9098186128426e+00,
      0.3109920095448e-08, 0.7693650193003e+00, 0.3932462625300e-02,
      0.3132237775119e-08, 0.3621293854908e+01, 0.2346394437820e+00,
      0.3942189421510e-08, 0.4841863659733e+01, 0.3180992042600e-02,
      0.3796972285340e-08, 0.1814174994268e+01, 0.1862120789403e+00,

      0.3995640233688e-08, 0.1386990406091e+01, 0.4549093064213e+00,
      0.2875013727414e-08, 0.9178318587177e+00, 0.1905464808669e+01,
      0.3073719932844e-08, 0.2688923811835e+01, 0.3628624111593e+00,
      0.2731016580075e-08, 0.1188259127584e+01, 0.2131850110243e+00,
      0.2729549896546e-08, 0.3702160634273e+01, 0.2134131485323e+00,
      0.3339372892449e-08, 0.7199163960331e+00, 0.2007689919132e+00,
      0.2898833764204e-08, 0.1916709364999e+01, 0.5291709230214e+00,
      0.2894536549362e-08, 0.2424043195547e+01, 0.5302110212022e+00,
      0.3096872473843e-08, 0.4445894977497e+01, 0.2976424921901e+00,
      0.2635672326810e-08, 0.3814366984117e+01, 0.1485980103780e+01,

      0.3649302697001e-08, 0.2924200596084e+01, 0.6044726378023e+00,
      0.3127954585895e-08, 0.1842251648327e+01, 0.1084620721060e+00,
      0.2616040173947e-08, 0.4155841921984e+01, 0.1258454114666e+01,
      0.2597395859860e-08, 0.1158045978874e+00, 0.2103781122809e+00,
      0.2593286172210e-08, 0.4771850408691e+01, 0.2162200472757e+00,
      0.2481823585747e-08, 0.4608842558889e+00, 0.1062562936266e+01,
      0.2742219550725e-08, 0.1538781127028e+01, 0.5651155736444e+00,
      0.3199558469610e-08, 0.3226647822878e+00, 0.7036329877322e+00,
      0.2666088542957e-08, 0.1967991731219e+00, 0.1400015846597e+00,
      0.2397067430580e-08, 0.3707036669873e+01, 0.2125476091956e+00,

      0.2376570772738e-08, 0.1182086628042e+01, 0.2140505503610e+00,
      0.2547228007887e-08, 0.4906256820629e+01, 0.1534957940063e+00,
      0.2265575594114e-08, 0.3414949866857e+01, 0.2235935264888e+00,
      0.2464381430585e-08, 0.4599122275378e+01, 0.2091065926078e+00,
      0.2433408527044e-08, 0.2830751145445e+00, 0.2174915669488e+00,
      0.2443605509076e-08, 0.4212046432538e+01, 0.1739420156204e+00,
      0.2319779262465e-08, 0.9881978408630e+00, 0.7530171478090e-01,
      0.2284622835465e-08, 0.5565347331588e+00, 0.7426161660010e-01,
      0.2467268750783e-08, 0.5655708150766e+00, 0.2526561439362e+00,
      0.2808513492782e-08, 0.1418405053408e+01, 0.5636314030725e+00,

      0.2329528932532e-08, 0.4069557545675e+01, 0.1056200952181e+01,
      0.9698639532817e-09, 0.1074134313634e+01, 0.7826370942180e+02 };

/* SSB-to-Sun, T^0, Y */
   static const double s0y[] = {
      0.4955392320126e-02, 0.2170467313679e+01, 0.5296909721118e+00,
      0.2722325167392e-02, 0.2444433682196e+01, 0.2132990797783e+00,
      0.1546579925346e-02, 0.5992779281546e+00, 0.3813291813120e-01,
      0.8363140252966e-03, 0.7687356310801e+00, 0.7478166569050e-01,
      0.3385792683603e-03, 0.0000000000000e+00, 0.0000000000000e+00,
      0.1201192221613e-03, 0.2520035601514e+01, 0.1059381944224e+01,
      0.7587125720554e-04, 0.1669954006449e+01, 0.4265981595566e+00,
      0.1964155361250e-04, 0.5707743963343e+01, 0.2061856251104e+00,
      0.1891900364909e-04, 0.2320960679937e+01, 0.2204125344462e+00,
      0.1937373433356e-04, 0.3226940689555e+01, 0.1495633313810e+00,

      0.1437139941351e-04, 0.2301626908096e+01, 0.5225775174439e+00,
      0.1406267683099e-04, 0.5188579265542e+01, 0.5368044267797e+00,
      0.1178703080346e-04, 0.5489483248476e+01, 0.7626583626240e-01,
      0.8079835186041e-05, 0.1683751835264e+01, 0.3664874755930e-01,
      0.7623253594652e-05, 0.2656400462961e+01, 0.3961708870310e-01,
      0.6248667483971e-05, 0.4992775362055e+01, 0.7329749511860e-01,
      0.4366353695038e-05, 0.2869706279678e+01, 0.1589072916335e+01,
      0.3829101568895e-05, 0.3572131359950e+01, 0.7113454667900e-02,
      0.3175733773908e-05, 0.4535372530045e+01, 0.4194847048887e+00,
      0.3092437902159e-05, 0.9230153317909e+00, 0.6398972393349e+00,

      0.2874168812154e-05, 0.3363143761101e+01, 0.1102062672231e+00,
      0.3040119321826e-05, 0.3324250895675e+01, 0.6283075850446e+01,
      0.2699723308006e-05, 0.2917882441928e+00, 0.1030928125552e+00,
      0.2134832683534e-05, 0.4220997202487e+01, 0.3163918923335e+00,
      0.1770412139433e-05, 0.4747318496462e+01, 0.1021328554739e+02,
      0.1377264209373e-05, 0.4305058462401e+00, 0.1484170571900e-02,
      0.1127814538960e-05, 0.8538177240740e+00, 0.6327837846670e+00,
      0.1055608090130e-05, 0.1551800742580e+01, 0.4337116142245e+00,
      0.9802673861420e-06, 0.1459646735377e+01, 0.1052268489556e+01,
      0.1090329461951e-05, 0.1587351228711e+01, 0.1162474756779e+01,

      0.6959590025090e-06, 0.5534442628766e+01, 0.1066495398892e+01,
      0.5664914529542e-06, 0.6030673003297e+01, 0.9491756770005e+00,
      0.6607787763599e-06, 0.4989507233927e+01, 0.8460828644453e+00,
      0.6269725742838e-06, 0.4222951804572e+01, 0.1480791608091e+00,
      0.6301889697863e-06, 0.5444316669126e+01, 0.2243449970715e+00,
      0.4891042662861e-06, 0.1490552839784e+01, 0.3340612434717e+01,
      0.3457083123290e-06, 0.3030475486049e+01, 0.3516457698740e-01,
      0.3032559967314e-06, 0.2652038793632e+01, 0.1104591729320e-01,
      0.2841133988903e-06, 0.1276744786829e+01, 0.4110125927500e-01,
      0.2855564444432e-06, 0.2143368674733e+01, 0.1510475019529e+00,

      0.2765157135038e-06, 0.5444186109077e+01, 0.6373574839730e-01,
      0.2382312465034e-06, 0.2190521137593e+01, 0.2275259891141e+00,
      0.2808060365077e-06, 0.5735195064841e+01, 0.2535050500000e-01,
      0.2332175234405e-06, 0.9481985524859e-01, 0.7181332454670e-01,
      0.2322488199659e-06, 0.5180499361533e+01, 0.8582758298370e-01,
      0.1881850258423e-06, 0.3219788273885e+01, 0.2118763888447e+01,
      0.2196111392808e-06, 0.2366941159761e+01, 0.2968341143800e-02,
      0.2183810335519e-06, 0.4825445110915e+01, 0.7775000683430e-01,
      0.2002733093326e-06, 0.2457148995307e+01, 0.2093666171530e+00,
      0.1967111767229e-06, 0.5586291545459e+01, 0.2172315424036e+00,

      0.1568473250543e-06, 0.3708003123320e+01, 0.7429900518901e+00,
      0.1852528314300e-06, 0.4310638151560e+01, 0.2022531624851e+00,
      0.1832111226447e-06, 0.1494665322656e+01, 0.3235053470014e+00,
      0.1746805502310e-06, 0.1451378500784e+01, 0.1385174140878e+00,
      0.1555730966650e-06, 0.1068040418198e+01, 0.7358765972222e+00,
      0.1554883462559e-06, 0.2442579035461e+01, 0.5154640627760e+00,
      0.1638380568746e-06, 0.2597913420625e+00, 0.8531963191132e+00,
      0.1159938593640e-06, 0.5834512021280e+01, 0.1990721704425e+00,
      0.1083427965695e-06, 0.5054033177950e+01, 0.5439178814476e+00,
      0.1156480369431e-06, 0.5325677432457e+01, 0.5257585094865e+00,

      0.1141308860095e-06, 0.2153403923857e+01, 0.5336234347371e+00,
      0.7913146470946e-07, 0.8642846847027e+00, 0.1478866649112e+01,
      0.7439752463733e-07, 0.1970628496213e+01, 0.2164800718209e+00,
      0.7280277104079e-07, 0.6073307250609e+01, 0.2101180877357e+00,
      0.8319567719136e-07, 0.1954371928334e+01, 0.1692165728891e+01,
      0.7137705549290e-07, 0.8904989440909e+00, 0.4155522422634e+00,
      0.6900825396225e-07, 0.2825717714977e+01, 0.1173197218910e+00,
      0.7245757216635e-07, 0.2481677513331e+01, 0.1265567569334e+01,
      0.6961165696255e-07, 0.1292955312978e+01, 0.9562891316684e+00,
      0.7571804456890e-07, 0.3427517575069e+01, 0.1422690933580e-01,

      0.6605425721904e-07, 0.8052192701492e+00, 0.6470106940028e+00,
      0.7375477357248e-07, 0.1705076390088e+01, 0.1581959461667e+01,
      0.7041664951470e-07, 0.4848356967891e+00, 0.9597935788730e-01,
      0.6322199535763e-07, 0.3878069473909e+01, 0.7084920306520e-01,
      0.5244380279191e-07, 0.2645560544125e+01, 0.5265099800692e+00,
      0.5143125704988e-07, 0.4834486101370e+01, 0.5328719641544e+00,
      0.5871866319373e-07, 0.7981472548900e+00, 0.7871412831580e-01,
      0.6300822573871e-07, 0.5979398788281e+01, 0.2608790314060e+02,
      0.6062154271548e-07, 0.4108655402756e+01, 0.1114304132498e+00,
      0.4361912339976e-07, 0.5322624319280e+01, 0.1375773836557e+01,

      0.4417005920067e-07, 0.6240817359284e+01, 0.2770348281756e+00,
      0.4686806749936e-07, 0.3214977301156e+01, 0.1143987543936e+00,
      0.3758892132305e-07, 0.5879809634765e+01, 0.1596186371003e+01,
      0.5151351332319e-07, 0.2893377688007e+00, 0.2228608264996e+00,
      0.4554683578572e-07, 0.5475427144122e+01, 0.1465949902372e+00,
      0.3442381385338e-07, 0.5992034796640e+01, 0.5070101000000e-01,
      0.2831093954933e-07, 0.5367350273914e+01, 0.3092784376656e+00,
      0.3756267090084e-07, 0.5758171285420e+01, 0.4903339079539e+00,
      0.2816374679892e-07, 0.1863718700923e+01, 0.2991266627620e+00,
      0.3419307025569e-07, 0.9524347534130e+00, 0.3518164938661e+00,

      0.2904250494239e-07, 0.5304471615602e+01, 0.1099462426779e+00,
      0.2471734511206e-07, 0.1297069793530e+01, 0.6256703299991e+00,
      0.2539620831872e-07, 0.3281126083375e+01, 0.1256615170089e+02,
      0.2281017868007e-07, 0.1829122133165e+01, 0.6681224869435e+01,
      0.2275319473335e-07, 0.5797198160181e+01, 0.3932462625300e-02,
      0.2547755368442e-07, 0.4752697708330e+01, 0.1169588211447e+01,
      0.2285979669317e-07, 0.1223205292886e+01, 0.1045155034888e+01,
      0.1913386560994e-07, 0.1757532993389e+01, 0.1155361302111e+01,
      0.1809020525147e-07, 0.4246116108791e+01, 0.3368040641550e-01,
      0.1649213300201e-07, 0.1445162890627e+01, 0.4408250688924e+00,

      0.1834972793932e-07, 0.1126917567225e+01, 0.4452511715700e-02,
      0.1439550648138e-07, 0.6160756834764e+01, 0.9420622223326e+00,
      0.1487645457041e-07, 0.4358761931792e+01, 0.4123712502208e+00,
      0.1731729516660e-07, 0.6134456753344e+01, 0.2108507877249e+00,
      0.1717747163567e-07, 0.1898186084455e+01, 0.2157473718317e+00,
      0.1418190430374e-07, 0.4180286741266e+01, 0.6521991896920e-01,
      0.1404844134873e-07, 0.7654053565412e-01, 0.4258542984690e-01,
      0.1409842846538e-07, 0.4418612420312e+01, 0.2258291676434e+00,
      0.1090948346291e-07, 0.1260615686131e+01, 0.4226656969313e+00,
      0.1357577323612e-07, 0.3558248818690e+01, 0.7923417740620e-01,

      0.1018154061960e-07, 0.5676087241256e+01, 0.1456308687557e+00,
      0.1412073972109e-07, 0.8394392632422e+00, 0.1525316725248e+00,
      0.1030938326496e-07, 0.1653593274064e+01, 0.1795258541446e+01,
      0.1180081567104e-07, 0.1285802592036e+01, 0.7032915397480e-01,
      0.9708510575650e-08, 0.7631889488106e+00, 0.8434341241180e-01,
      0.9637689663447e-08, 0.4630642649176e+01, 0.1272681024002e+01,
      0.1068910429389e-07, 0.5294934032165e+01, 0.2123349582968e+00,
      0.1063716179336e-07, 0.2736266800832e+01, 0.2142632012598e+00,
      0.1234858713814e-07, 0.1302891146570e+01, 0.1847279083684e+00,
      0.8912631189738e-08, 0.3570415993621e+01, 0.2648454860559e+01,

      0.1036378285534e-07, 0.4236693440949e+01, 0.1370332435159e+00,
      0.9667798501561e-08, 0.2960768892398e+01, 0.4376440768498e+00,
      0.8108314201902e-08, 0.6987781646841e+00, 0.2880807454688e+00,
      0.7648364324628e-08, 0.2499017863863e+01, 0.2037373330570e+00,
      0.7286136828406e-08, 0.3787426951665e+01, 0.1129145838217e+00,
      0.9448237743913e-08, 0.2694354332983e+01, 0.5272426800584e+00,
      0.9374276106428e-08, 0.4787121277064e+01, 0.5321392641652e+00,
      0.7100226287462e-08, 0.3530238792101e+00, 0.6288513220417e+00,
      0.9253056659571e-08, 0.1399478925664e+01, 0.1606092486742e+00,
      0.6636432145504e-08, 0.3479575438447e+01, 0.1368660381889e+01,

      0.6469975312932e-08, 0.1383669964800e+01, 0.2008557621224e+01,
      0.7335849729765e-08, 0.1243698166898e+01, 0.9561746721300e-02,
      0.8743421205855e-08, 0.3776164289301e+01, 0.3801276407308e+00,
      0.5993635744494e-08, 0.5627122113596e+01, 0.2042657109477e+02,
      0.5981008479693e-08, 0.1674336636752e+01, 0.2111650433779e+01,
      0.6188535145838e-08, 0.5214925208672e+01, 0.4305306221819e+00,
      0.6596074017566e-08, 0.2907653268124e+01, 0.1063314406849e+01,
      0.6630815126226e-08, 0.2127643669658e+01, 0.8389694097774e+00,
      0.6156772830040e-08, 0.5082160803295e+01, 0.4234171675140e+00,
      0.6446960563014e-08, 0.1872100916905e+01, 0.5287268506303e+00,

      0.6429324424668e-08, 0.5610276103577e+01, 0.5306550935933e+00,
      0.6302232396465e-08, 0.1592152049607e+01, 0.1253008786510e-01,
      0.6399244436159e-08, 0.2746214421532e+01, 0.5217580628120e+02,
      0.5474965172558e-08, 0.2317666374383e+01, 0.2221856701002e+01,
      0.5339293190692e-08, 0.1084724961156e+01, 0.7466759693650e-01,
      0.5334733683389e-08, 0.3594106067745e+01, 0.7489573444450e-01,
      0.5392665782110e-08, 0.5630254365606e+01, 0.1055449481598e+01,
      0.6682075673789e-08, 0.1518480041732e+01, 0.2213766559277e+00,
      0.5079130495960e-08, 0.2739765115711e+01, 0.2132517061319e+00,
      0.5077759793261e-08, 0.5290711290094e+01, 0.2133464534247e+00,

      0.4832037368310e-08, 0.1404473217200e+01, 0.7160067364790e-01,
      0.6463279674802e-08, 0.6038381695210e+01, 0.2209183458640e-01,
      0.6240592771560e-08, 0.1290170653666e+01, 0.3306188016693e+00,
      0.4672013521493e-08, 0.3261895939677e+01, 0.7796265773310e-01,
      0.6500650750348e-08, 0.1154522312095e+01, 0.3884652414254e+00,
      0.6344161389053e-08, 0.6206111545062e+01, 0.7605151500000e-01,
      0.4682518370646e-08, 0.5409118796685e+01, 0.1073608853559e+01,
      0.5329460015591e-08, 0.1202985784864e+01, 0.7287631425543e+00,
      0.5701588675898e-08, 0.4098715257064e+01, 0.8731175355560e-01,
      0.6030690867211e-08, 0.4132033218460e+00, 0.9846002785331e+00,

      0.4336256312655e-08, 0.1211415991827e+01, 0.4297791515992e+00,
      0.4688498808975e-08, 0.3765479072409e+01, 0.2127790306879e+00,
      0.4675578609335e-08, 0.4265540037226e+01, 0.2138191288687e+00,
      0.4225578112158e-08, 0.5237566010676e+01, 0.3407705765729e+00,
      0.5139422230028e-08, 0.1507173079513e+01, 0.7233337363710e-01,
      0.4619995093571e-08, 0.9023957449848e-01, 0.8603097737811e+00,
      0.4494776255461e-08, 0.5414930552139e+00, 0.7381754420900e-01,
      0.4274026276788e-08, 0.4145735303659e+01, 0.7574578717200e-01,
      0.5018141789353e-08, 0.3344408829055e+01, 0.3180992042600e-02,
      0.4866163952181e-08, 0.3348534657607e+01, 0.7722995774390e-01,

      0.4111986020501e-08, 0.4198823597220e+00, 0.1451108196653e+00,
      0.3356142784950e-08, 0.5609144747180e+01, 0.1274714967946e+00,
      0.4070575554551e-08, 0.7028411059224e+00, 0.3503323232942e+00,
      0.3257451857278e-08, 0.5624697983086e+01, 0.5296435984654e+00,
      0.3256973703026e-08, 0.1857842076707e+01, 0.5297383457582e+00,
      0.3830771508640e-08, 0.4562887279931e+01, 0.9098186128426e+00,
      0.3725024005962e-08, 0.2358058692652e+00, 0.1084620721060e+00,
      0.3136763921756e-08, 0.2049731526845e+01, 0.2346394437820e+00,
      0.3795147256194e-08, 0.2432356296933e+00, 0.1862120789403e+00,
      0.2877342229911e-08, 0.5631101279387e+01, 0.1905464808669e+01,

      0.3076931798805e-08, 0.1117615737392e+01, 0.3628624111593e+00,
      0.2734765945273e-08, 0.5899826516955e+01, 0.2131850110243e+00,
      0.2733405296885e-08, 0.2130562964070e+01, 0.2134131485323e+00,
      0.2898552353410e-08, 0.3462387048225e+00, 0.5291709230214e+00,
      0.2893736103681e-08, 0.8534352781543e+00, 0.5302110212022e+00,
      0.3095717734137e-08, 0.2875061429041e+01, 0.2976424921901e+00,
      0.2636190425832e-08, 0.2242512846659e+01, 0.1485980103780e+01,
      0.3645512095537e-08, 0.1354016903958e+01, 0.6044726378023e+00,
      0.2808173547723e-08, 0.6705114365631e-01, 0.6225157782540e-01,
      0.2625012866888e-08, 0.4775705748482e+01, 0.5268983110410e-01,

      0.2572233995651e-08, 0.2638924216139e+01, 0.1258454114666e+01,
      0.2604238824792e-08, 0.4826358927373e+01, 0.2103781122809e+00,
      0.2596886385239e-08, 0.3200388483118e+01, 0.2162200472757e+00,
      0.3228057304264e-08, 0.5384848409563e+01, 0.2007689919132e+00,
      0.2481601798252e-08, 0.5173373487744e+01, 0.1062562936266e+01,
      0.2745977498864e-08, 0.6250966149853e+01, 0.5651155736444e+00,
      0.2669878833811e-08, 0.4906001352499e+01, 0.1400015846597e+00,
      0.3203986611711e-08, 0.5034333010005e+01, 0.7036329877322e+00,
      0.3354961227212e-08, 0.6108262423137e+01, 0.4549093064213e+00,
      0.2400407324558e-08, 0.2135399294955e+01, 0.2125476091956e+00,

      0.2379905859802e-08, 0.5893721933961e+01, 0.2140505503610e+00,
      0.2550844302187e-08, 0.3331940762063e+01, 0.1534957940063e+00,
      0.2268824211001e-08, 0.1843418461035e+01, 0.2235935264888e+00,
      0.2464700891204e-08, 0.3029548547230e+01, 0.2091065926078e+00,
      0.2436814726024e-08, 0.4994717970364e+01, 0.2174915669488e+00,
      0.2443623894745e-08, 0.2645102591375e+01, 0.1739420156204e+00,
      0.2318701783838e-08, 0.5700547397897e+01, 0.7530171478090e-01,
      0.2284448700256e-08, 0.5268898905872e+01, 0.7426161660010e-01,
      0.2468848123510e-08, 0.5276280575078e+01, 0.2526561439362e+00,
      0.2814052350303e-08, 0.6130168623475e+01, 0.5636314030725e+00,

      0.2243662755220e-08, 0.6631692457995e+00, 0.8886590321940e-01,
      0.2330795855941e-08, 0.2499435487702e+01, 0.1056200952181e+01,
      0.9757679038404e-09, 0.5796846023126e+01, 0.7826370942180e+02 };

/* SSB-to-Sun, T^0, Z */
   static const double s0z[] = {
      0.1181255122986e-03, 0.4607918989164e+00, 0.2132990797783e+00,
      0.1127777651095e-03, 0.4169146331296e+00, 0.5296909721118e+00,
      0.4777754401806e-04, 0.4582657007130e+01, 0.3813291813120e-01,
      0.1129354285772e-04, 0.5758735142480e+01, 0.7478166569050e-01,
     -0.1149543637123e-04, 0.0000000000000e+00, 0.0000000000000e+00,
      0.3298730512306e-05, 0.5978801994625e+01, 0.4265981595566e+00,
      0.2733376706079e-05, 0.7665413691040e+00, 0.1059381944224e+01,
      0.9426389657270e-06, 0.3710201265838e+01, 0.2061856251104e+00,
      0.8187517749552e-06, 0.3390675605802e+00, 0.2204125344462e+00,
      0.4080447871819e-06, 0.4552296640088e+00, 0.5225775174439e+00,

      0.3169973017028e-06, 0.3445455899321e+01, 0.5368044267797e+00,
      0.2438098615549e-06, 0.5664675150648e+01, 0.3664874755930e-01,
      0.2601897517235e-06, 0.1931894095697e+01, 0.1495633313810e+00,
      0.2314558080079e-06, 0.3666319115574e+00, 0.3961708870310e-01,
      0.1962549548002e-06, 0.3167411699020e+01, 0.7626583626240e-01,
      0.2180518287925e-06, 0.1544420746580e+01, 0.7113454667900e-02,
      0.1451382442868e-06, 0.1583756740070e+01, 0.1102062672231e+00,
      0.1358439007389e-06, 0.5239941758280e+01, 0.6398972393349e+00,
      0.1050585898028e-06, 0.2266958352859e+01, 0.3163918923335e+00,
      0.1050029870186e-06, 0.2711495250354e+01, 0.4194847048887e+00,

      0.9934920679800e-07, 0.1116208151396e+01, 0.1589072916335e+01,
      0.1048395331560e-06, 0.3408619600206e+01, 0.1021328554739e+02,
      0.8370147196668e-07, 0.3810459401087e+01, 0.2535050500000e-01,
      0.7989856510998e-07, 0.3769910473647e+01, 0.7329749511860e-01,
      0.5441221655233e-07, 0.2416994903374e+01, 0.1030928125552e+00,
      0.4610812906784e-07, 0.5858503336994e+01, 0.4337116142245e+00,
      0.3923022803444e-07, 0.3354170010125e+00, 0.1484170571900e-02,
      0.2610725582128e-07, 0.5410600646324e+01, 0.6327837846670e+00,
      0.2455279767721e-07, 0.6120216681403e+01, 0.1162474756779e+01,
      0.2375530706525e-07, 0.6055443426143e+01, 0.1052268489556e+01,

      0.1782967577553e-07, 0.3146108708004e+01, 0.8460828644453e+00,
      0.1581687095238e-07, 0.6255496089819e+00, 0.3340612434717e+01,
      0.1594657672461e-07, 0.3782604300261e+01, 0.1066495398892e+01,
      0.1563448615040e-07, 0.1997775733196e+01, 0.2022531624851e+00,
      0.1463624258525e-07, 0.1736316792088e+00, 0.3516457698740e-01,
      0.1331585056673e-07, 0.4331941830747e+01, 0.9491756770005e+00,
      0.1130634557637e-07, 0.6152017751825e+01, 0.2968341143800e-02,
      0.1028949607145e-07, 0.2101792614637e+00, 0.2275259891141e+00,
      0.1024074971618e-07, 0.4071833211074e+01, 0.5070101000000e-01,
      0.8826956060303e-08, 0.4861633688145e+00, 0.2093666171530e+00,

      0.8572230171541e-08, 0.5268190724302e+01, 0.4110125927500e-01,
      0.7649332643544e-08, 0.5134543417106e+01, 0.2608790314060e+02,
      0.8581673291033e-08, 0.2920218146681e+01, 0.1480791608091e+00,
      0.8430589300938e-08, 0.3604576619108e+01, 0.2172315424036e+00,
      0.7776165501012e-08, 0.3772942249792e+01, 0.6373574839730e-01,
      0.8311070234408e-08, 0.6200412329888e+01, 0.3235053470014e+00,
      0.6927365212582e-08, 0.4543353113437e+01, 0.8531963191132e+00,
      0.6791574208598e-08, 0.2882188406238e+01, 0.7181332454670e-01,
      0.5593100811839e-08, 0.1776646892780e+01, 0.7429900518901e+00,
      0.4553381853021e-08, 0.3949617611240e+01, 0.7775000683430e-01,

      0.5758000450068e-08, 0.3859251775075e+01, 0.1990721704425e+00,
      0.4281283457133e-08, 0.1466294631206e+01, 0.2118763888447e+01,
      0.4206935661097e-08, 0.5421776011706e+01, 0.1104591729320e-01,
      0.4213751641837e-08, 0.3412048993322e+01, 0.2243449970715e+00,
      0.5310506239878e-08, 0.5421641370995e+00, 0.5154640627760e+00,
      0.3827450341320e-08, 0.8887314524995e+00, 0.1510475019529e+00,
      0.4292435241187e-08, 0.1405043757194e+01, 0.1422690933580e-01,
      0.3189780702289e-08, 0.1060049293445e+01, 0.1173197218910e+00,
      0.3226611928069e-08, 0.6270858897442e+01, 0.2164800718209e+00,
      0.2893897608830e-08, 0.5117563223301e+01, 0.6470106940028e+00,

      0.3239852024578e-08, 0.4079092237983e+01, 0.2101180877357e+00,
      0.2956892222200e-08, 0.1594917021704e+01, 0.3092784376656e+00,
      0.2980177912437e-08, 0.5258787667564e+01, 0.4155522422634e+00,
      0.3163725690776e-08, 0.3854589225479e+01, 0.8582758298370e-01,
      0.2662262399118e-08, 0.3561326430187e+01, 0.5257585094865e+00,
      0.2766689135729e-08, 0.3180732086830e+00, 0.1385174140878e+00,
      0.2411600278464e-08, 0.3324798335058e+01, 0.5439178814476e+00,
      0.2483527695131e-08, 0.4169069291947e+00, 0.5336234347371e+00,
      0.7788777276590e-09, 0.1900569908215e+01, 0.5217580628120e+02 };

/* SSB-to-Sun, T^1, X */
   static const double s1x[] = {
     -0.1296310361520e-07, 0.0000000000000e+00, 0.0000000000000e+00,
      0.8975769009438e-08, 0.1128891609250e+01, 0.4265981595566e+00,
      0.7771113441307e-08, 0.2706039877077e+01, 0.2061856251104e+00,
      0.7538303866642e-08, 0.2191281289498e+01, 0.2204125344462e+00,
      0.6061384579336e-08, 0.3248167319958e+01, 0.1059381944224e+01,
      0.5726994235594e-08, 0.5569981398610e+01, 0.5225775174439e+00,
      0.5616492836424e-08, 0.5057386614909e+01, 0.5368044267797e+00,
      0.1010881584769e-08, 0.3473577116095e+01, 0.7113454667900e-02,
      0.7259606157626e-09, 0.3651858593665e+00, 0.6398972393349e+00,
      0.8755095026935e-09, 0.1662835408338e+01, 0.4194847048887e+00,

      0.5370491182812e-09, 0.1327673878077e+01, 0.4337116142245e+00,
      0.5743773887665e-09, 0.4250200846687e+01, 0.2132990797783e+00,
      0.4408103140300e-09, 0.3598752574277e+01, 0.1589072916335e+01,
      0.3101892374445e-09, 0.4887822983319e+01, 0.1052268489556e+01,
      0.3209453713578e-09, 0.9702272295114e+00, 0.5296909721118e+00,
      0.3017228286064e-09, 0.5484462275949e+01, 0.1066495398892e+01,
      0.3200700038601e-09, 0.2846613338643e+01, 0.1495633313810e+00,
      0.2137637279911e-09, 0.5692163292729e+00, 0.3163918923335e+00,
      0.1899686386727e-09, 0.2061077157189e+01, 0.2275259891141e+00,
      0.1401994545308e-09, 0.4177771136967e+01, 0.1102062672231e+00,

      0.1578057810499e-09, 0.5782460597335e+01, 0.7626583626240e-01,
      0.1237713253351e-09, 0.5705900866881e+01, 0.5154640627760e+00,
      0.1313076837395e-09, 0.5163438179576e+01, 0.3664874755930e-01,
      0.1184963304860e-09, 0.3054804427242e+01, 0.6327837846670e+00,
      0.1238130878565e-09, 0.2317292575962e+01, 0.3961708870310e-01,
      0.1015959527736e-09, 0.2194643645526e+01, 0.7329749511860e-01,
      0.9017954423714e-10, 0.2868603545435e+01, 0.1990721704425e+00,
      0.8668024955603e-10, 0.4923849675082e+01, 0.5439178814476e+00,
      0.7756083930103e-10, 0.3014334135200e+01, 0.9491756770005e+00,
      0.7536503401741e-10, 0.2704886279769e+01, 0.1030928125552e+00,

      0.5483308679332e-10, 0.6010983673799e+01, 0.8531963191132e+00,
      0.5184339620428e-10, 0.1952704573291e+01, 0.2093666171530e+00,
      0.5108658712030e-10, 0.2958575786649e+01, 0.2172315424036e+00,
      0.5019424524650e-10, 0.1736317621318e+01, 0.2164800718209e+00,
      0.4909312625978e-10, 0.3167216416257e+01, 0.2101180877357e+00,
      0.4456638901107e-10, 0.7697579923471e+00, 0.3235053470014e+00,
      0.4227030350925e-10, 0.3490910137928e+01, 0.6373574839730e-01,
      0.4095456040093e-10, 0.5178888984491e+00, 0.6470106940028e+00,
      0.4990537041422e-10, 0.3323887668974e+01, 0.1422690933580e-01,
      0.4321170010845e-10, 0.4288484987118e+01, 0.7358765972222e+00,

      0.3544072091802e-10, 0.6021051579251e+01, 0.5265099800692e+00,
      0.3480198638687e-10, 0.4600027054714e+01, 0.5328719641544e+00,
      0.3440287244435e-10, 0.4349525970742e+01, 0.8582758298370e-01,
      0.3330628322713e-10, 0.2347391505082e+01, 0.1104591729320e-01,
      0.2973060707184e-10, 0.4789409286400e+01, 0.5257585094865e+00,
      0.2932606766089e-10, 0.5831693799927e+01, 0.5336234347371e+00,
      0.2876972310953e-10, 0.2692638514771e+01, 0.1173197218910e+00,
      0.2827488278556e-10, 0.2056052487960e+01, 0.2022531624851e+00,
      0.2515028239756e-10, 0.7411863262449e+00, 0.9597935788730e-01,
      0.2853033744415e-10, 0.3948481024894e+01, 0.2118763888447e+01 };

/* SSB-to-Sun, T^1, Y */
   static const double s1y[] = {
      0.8989047573576e-08, 0.5840593672122e+01, 0.4265981595566e+00,
      0.7815938401048e-08, 0.1129664707133e+01, 0.2061856251104e+00,
      0.7550926713280e-08, 0.6196589104845e+00, 0.2204125344462e+00,
      0.6056556925895e-08, 0.1677494667846e+01, 0.1059381944224e+01,
      0.5734142698204e-08, 0.4000920852962e+01, 0.5225775174439e+00,
      0.5614341822459e-08, 0.3486722577328e+01, 0.5368044267797e+00,
      0.1028678147656e-08, 0.1877141024787e+01, 0.7113454667900e-02,
      0.7270792075266e-09, 0.5077167301739e+01, 0.6398972393349e+00,
      0.8734141726040e-09, 0.9069550282609e-01, 0.4194847048887e+00,
      0.5377371402113e-09, 0.6039381844671e+01, 0.4337116142245e+00,

      0.4729719431571e-09, 0.2153086311760e+01, 0.2132990797783e+00,
      0.4458052820973e-09, 0.5059830025565e+01, 0.5296909721118e+00,
      0.4406855467908e-09, 0.2027971692630e+01, 0.1589072916335e+01,
      0.3101659310977e-09, 0.3317677981860e+01, 0.1052268489556e+01,
      0.3016749232545e-09, 0.3913703482532e+01, 0.1066495398892e+01,
      0.3198541352656e-09, 0.1275513098525e+01, 0.1495633313810e+00,
      0.2142065389871e-09, 0.5301351614597e+01, 0.3163918923335e+00,
      0.1902615247592e-09, 0.4894943352736e+00, 0.2275259891141e+00,
      0.1613410990871e-09, 0.2449891130437e+01, 0.1102062672231e+00,
      0.1576992165097e-09, 0.4211421447633e+01, 0.7626583626240e-01,

      0.1241637259894e-09, 0.4140803368133e+01, 0.5154640627760e+00,
      0.1313974830355e-09, 0.3591920305503e+01, 0.3664874755930e-01,
      0.1181697118258e-09, 0.1506314382788e+01, 0.6327837846670e+00,
      0.1238239742779e-09, 0.7461405378404e+00, 0.3961708870310e-01,
      0.1010107068241e-09, 0.6271010795475e+00, 0.7329749511860e-01,
      0.9226316616509e-10, 0.1259158839583e+01, 0.1990721704425e+00,
      0.8664946419555e-10, 0.3353244696934e+01, 0.5439178814476e+00,
      0.7757230468978e-10, 0.1447677295196e+01, 0.9491756770005e+00,
      0.7693168628139e-10, 0.1120509896721e+01, 0.1030928125552e+00,
      0.5487897454612e-10, 0.4439380426795e+01, 0.8531963191132e+00,

      0.5196118677218e-10, 0.3788856619137e+00, 0.2093666171530e+00,
      0.5110853339935e-10, 0.1386879372016e+01, 0.2172315424036e+00,
      0.5027804534813e-10, 0.1647881805466e+00, 0.2164800718209e+00,
      0.4922485922674e-10, 0.1594315079862e+01, 0.2101180877357e+00,
      0.6155599524400e-10, 0.0000000000000e+00, 0.0000000000000e+00,
      0.4447147832161e-10, 0.5480720918976e+01, 0.3235053470014e+00,
      0.4144691276422e-10, 0.1931371033660e+01, 0.6373574839730e-01,
      0.4099950625452e-10, 0.5229611294335e+01, 0.6470106940028e+00,
      0.5060541682953e-10, 0.1731112486298e+01, 0.1422690933580e-01,
      0.4293615946300e-10, 0.2714571038925e+01, 0.7358765972222e+00,

      0.3545659845763e-10, 0.4451041444634e+01, 0.5265099800692e+00,
      0.3479112041196e-10, 0.3029385448081e+01, 0.5328719641544e+00,
      0.3438516493570e-10, 0.2778507143731e+01, 0.8582758298370e-01,
      0.3297341285033e-10, 0.7898709807584e+00, 0.1104591729320e-01,
      0.2972585818015e-10, 0.3218785316973e+01, 0.5257585094865e+00,
      0.2931707295017e-10, 0.4260731012098e+01, 0.5336234347371e+00,
      0.2897198149403e-10, 0.1120753978101e+01, 0.1173197218910e+00,
      0.2832293240878e-10, 0.4597682717827e+00, 0.2022531624851e+00,
      0.2864348326612e-10, 0.2169939928448e+01, 0.9597935788730e-01,
      0.2852714675471e-10, 0.2377659870578e+01, 0.2118763888447e+01 };

/* SSB-to-Sun, T^1, Z */
   static const double s1z[] = {
      0.5444220475678e-08, 0.1803825509310e+01, 0.2132990797783e+00,
      0.3883412695596e-08, 0.4668616389392e+01, 0.5296909721118e+00,
      0.1334341434551e-08, 0.0000000000000e+00, 0.0000000000000e+00,
      0.3730001266883e-09, 0.5401405918943e+01, 0.2061856251104e+00,
      0.2894929197956e-09, 0.4932415609852e+01, 0.2204125344462e+00,
      0.2857950357701e-09, 0.3154625362131e+01, 0.7478166569050e-01,
      0.2499226432292e-09, 0.3657486128988e+01, 0.4265981595566e+00,
      0.1937705443593e-09, 0.5740434679002e+01, 0.1059381944224e+01,
      0.1374894396320e-09, 0.1712857366891e+01, 0.5368044267797e+00,
      0.1217248678408e-09, 0.2312090870932e+01, 0.5225775174439e+00,

      0.7961052740870e-10, 0.5283368554163e+01, 0.3813291813120e-01,
      0.4979225949689e-10, 0.4298290471860e+01, 0.4194847048887e+00,
      0.4388552286597e-10, 0.6145515047406e+01, 0.7113454667900e-02,
      0.2586835212560e-10, 0.3019448001809e+01, 0.6398972393349e+00 };

/* SSB-to-Sun, T^2, X */
   static const double s2x[] = {
      0.1603551636587e-11, 0.4404109410481e+01, 0.2061856251104e+00,
      0.1556935889384e-11, 0.4818040873603e+00, 0.2204125344462e+00,
      0.1182594414915e-11, 0.9935762734472e+00, 0.5225775174439e+00,
      0.1158794583180e-11, 0.3353180966450e+01, 0.5368044267797e+00,
      0.9597358943932e-12, 0.5567045358298e+01, 0.2132990797783e+00,
      0.6511516579605e-12, 0.5630872420788e+01, 0.4265981595566e+00,
      0.7419792747688e-12, 0.2156188581957e+01, 0.5296909721118e+00,
      0.3951972655848e-12, 0.1981022541805e+01, 0.1059381944224e+01,
      0.4478223877045e-12, 0.0000000000000e+00, 0.0000000000000e+00 };

/* SSB-to-Sun, T^2, Y */
   static const double s2y[] = {
      0.1609114495091e-11, 0.2831096993481e+01, 0.2061856251104e+00,
      0.1560330784946e-11, 0.5193058213906e+01, 0.2204125344462e+00,
      0.1183535479202e-11, 0.5707003443890e+01, 0.5225775174439e+00,
      0.1158183066182e-11, 0.1782400404928e+01, 0.5368044267797e+00,
      0.1032868027407e-11, 0.4036925452011e+01, 0.2132990797783e+00,
      0.6540142847741e-12, 0.4058241056717e+01, 0.4265981595566e+00,
      0.7305236491596e-12, 0.6175401942957e+00, 0.5296909721118e+00,
     -0.5580725052968e-12, 0.0000000000000e+00, 0.0000000000000e+00,
      0.3946122651015e-12, 0.4108265279171e+00, 0.1059381944224e+01 };

/* SSB-to-Sun, T^2, Z */
   static const double s2z[] = {
      0.3749920358054e-12, 0.3230285558668e+01, 0.2132990797783e+00,
      0.2735037220939e-12, 0.6154322683046e+01, 0.5296909721118e+00 };

/* Pointers to coefficient arrays, in x,y,z sets */
   static const double *ce0[] = { e0x, e0y, e0z },
                       *ce1[] = { e1x, e1y, e1z },
                       *ce2[] = { e2x, e2y, e2z },
                       *cs0[] = { s0x, s0y, s0z },
                       *cs1[] = { s1x, s1y, s1z },
                       *cs2[] = { s2x, s2y, s2z };
   const double *coeffs;

/* Numbers of terms for each component of the model, in x,y,z sets */
   static const int ne0[3] = {(int)(sizeof e0x / sizeof (double) / 3),
                              (int)(sizeof e0y / sizeof (double) / 3),
                              (int)(sizeof e0z / sizeof (double) / 3) },
                    ne1[3] = {(int)(sizeof e1x / sizeof (double) / 3),
                              (int)(sizeof e1y / sizeof (double) / 3),
                              (int)(sizeof e1z / sizeof (double) / 3) },
                    ne2[3] = {(int)(sizeof e2x / sizeof (double) / 3),
                              (int)(sizeof e2y / sizeof (double) / 3),
                              (int)(sizeof e2z / sizeof (double) / 3) },
                    ns0[3] = {(int)(sizeof s0x / sizeof (double) / 3),
                              (int)(sizeof s0y / sizeof (double) / 3),
                              (int)(sizeof s0z / sizeof (double) / 3) },
                    ns1[3] = {(int)(sizeof s1x / sizeof (double) / 3),
                              (int)(sizeof s1y / sizeof (double) / 3),
                              (int)(sizeof s1z / sizeof (double) / 3) },
                    ns2[3] = {(int)(sizeof s2x / sizeof (double) / 3),
                              (int)(sizeof s2y / sizeof (double) / 3),
                              (int)(sizeof s2z / sizeof (double) / 3) };
   int nterms;

/* Miscellaneous */
   int jstat, i, j;
   double t, t2, xyz, xyzd, a, b, c, ct, p, cp,
          ph[3], vh[3], pb[3], vb[3], x, y, z;

/* ------------------------------------------------------------------ */

/* Time since reference epoch, Julian years. */
   t = ((date1 - ERFA_DJ00) + date2) / ERFA_DJY;
   t2 = t*t;

/* Set status. */
   jstat = fabs(t) <= 100.0 ? 0 : 1;

/* X then Y then Z. */
   for (i = 0; i < 3; i++) {

   /* Initialize position and velocity component. */
      xyz = 0.0;
      xyzd = 0.0;

   /* ------------------------------------------------ */
   /* Obtain component of Sun to Earth ecliptic vector */
   /* ------------------------------------------------ */

   /* Sun to Earth, T^0 terms. */
      coeffs = ce0[i];
      nterms = ne0[i];
      for (j = 0; j < nterms; j++) {
         a = *coeffs++;
         b = *coeffs++;
         c = *coeffs++;
         p = b + c*t;
         xyz  += a*cos(p);
         xyzd -= a*c*sin(p);
      }

   /* Sun to Earth, T^1 terms. */
      coeffs = ce1[i];
      nterms = ne1[i];
      for (j = 0; j < nterms; j++) {
         a = *coeffs++;
         b = *coeffs++;
         c = *coeffs++;
         ct = c*t;
         p = b + ct;
         cp = cos(p);
         xyz  += a*t*cp;
         xyzd += a*( cp - ct*sin(p) );
      }

   /* Sun to Earth, T^2 terms. */
      coeffs = ce2[i];
      nterms = ne2[i];
      for (j = 0; j < nterms; j++) {
         a = *coeffs++;
         b = *coeffs++;
         c = *coeffs++;
         ct = c*t;
         p = b + ct;
         cp = cos(p);
         xyz  += a*t2*cp;
         xyzd += a*t*( 2.0*cp - ct*sin(p) );
      }

   /* Heliocentric Earth position and velocity component. */
      ph[i] = xyz;
      vh[i] = xyzd / ERFA_DJY;

   /* ------------------------------------------------ */
   /* Obtain component of SSB to Earth ecliptic vector */
   /* ------------------------------------------------ */

   /* SSB to Sun, T^0 terms. */
      coeffs = cs0[i];
      nterms = ns0[i];
      for (j = 0; j < nterms; j++) {
         a = *coeffs++;
         b = *coeffs++;
         c = *coeffs++;
         p = b + c*t;
         xyz  += a*cos(p);
         xyzd -= a*c*sin(p);
      }

   /* SSB to Sun, T^1 terms. */
      coeffs = cs1[i];
      nterms = ns1[i];
      for (j = 0; j < nterms; j++) {
         a = *coeffs++;
         b = *coeffs++;
         c = *coeffs++;
         ct = c*t;
         p = b + ct;
         cp = cos(p);
         xyz  += a*t*cp;
         xyzd += a*(cp - ct*sin(p));
      }

   /* SSB to Sun, T^2 terms. */
      coeffs = cs2[i];
      nterms = ns2[i];
      for (j = 0; j < nterms; j++) {
         a = *coeffs++;
         b = *coeffs++;
         c = *coeffs++;
         ct = c*t;
         p = b + ct;
         cp = cos(p);
         xyz  += a*t2*cp;
         xyzd += a*t*(2.0*cp - ct*sin(p));
     }

   /* Barycentric Earth position and velocity component. */
     pb[i] = xyz;
     vb[i] = xyzd / ERFA_DJY;

   /* Next Cartesian component. */
   }

/* Rotate from ecliptic to BCRS coordinates. */

   x = ph[0];
   y = ph[1];
   z = ph[2];
   pvh[0][0] =      x + am12*y + am13*z;
   pvh[0][1] = am21*x + am22*y + am23*z;
   pvh[0][2] =          am32*y + am33*z;

   x = vh[0];
   y = vh[1];
   z = vh[2];
   pvh[1][0] =      x + am12*y + am13*z;
   pvh[1][1] = am21*x + am22*y + am23*z;
   pvh[1][2] =          am32*y + am33*z;

   x = pb[0];
   y = pb[1];
   z = pb[2];
   pvb[0][0] =      x + am12*y + am13*z;
   pvb[0][1] = am21*x + am22*y + am23*z;
   pvb[0][2] =          am32*y + am33*z;

   x = vb[0];
   y = vb[1];
   z = vb[2];
   pvb[1][0] =      x + am12*y + am13*z;
   pvb[1][1] = am21*x + am22*y + am23*z;
   pvb[1][2] =          am32*y + am33*z;

/* Return the status. */
   return jstat;

/* Finished. */

}

