
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <slalib.h>

#include "astro.h"
#include "iers_bulletin_a.h"

/*
 * Not strictly correct, because of leap seconds.
 */
double
convert_ctime_to_utc_mjd( double ctime )
{
    return secs2days(ctime) + 40587.0;
}

double
convert_utc_to_tt( double utc )
{
    return utc + secs2days(slaDtt(utc));
}

int
observed_altaz_to_mean_radec( const Site *site, double freq_ghz,
        int n, const double ctime[],
        const float alt[], const float az[],
        float ra[], float dec[] )
{
    assert( n > 0 );
    assert( ctime != NULL );
    assert( alt != NULL );
    assert( az != NULL );
    assert( ra != NULL );
    assert( dec != NULL );

    int stat;
    float dut1, x, y;
    double amprms[21], aoprms[14];

    double utc = convert_ctime_to_utc_mjd( ctime[0] );

    int mjd = (int) floor(utc);
    stat = get_iers_bulletin_a( mjd, &dut1, &x, &y );
    if ( stat != 0 )
        return stat;
    //printf( "mjd,dut1,x,y = %d, %g, %g, %g\n", mjd, dut1, x, y );

    double wavelength_um = 299792.458/freq_ghz;

    slaAoppa( utc, dut1,
            site->east_longitude,
            site->latitude,
            site->elevation_m,
            arcsec2rad(x),
            arcsec2rad(y),
            site->temperature_K,
            site->pressure_mb,
            site->relative_humidity,
            wavelength_um,
            0.0065,     // tropospheric lapse rate [K/m]
            aoprms );

    double tt = convert_utc_to_tt( utc );
    slaMappa( 2000.0, tt, amprms );
    //printf( "freq = %g\n", freq_ghz );

    for ( int i = 0; i < n; i++ )
    {
        double observed_az = az[i];
        double observed_zenith = M_PI/2 - alt[i];
        double apparent_ra, apparent_dec, mean_ra, mean_dec;

        utc = convert_ctime_to_utc_mjd( ctime[i] );
        slaAoppat( utc, aoprms );

        slaOapqk( "A", observed_az, observed_zenith, aoprms,
                &apparent_ra, &apparent_dec );

        //printf( "apparent ra,dec = %g, %g\n", apparent_ra, apparent_dec );

        slaAmpqk( apparent_ra, apparent_dec, amprms, &mean_ra, &mean_dec );

        //printf( "mean ra,dec = %g, %g\n", mean_ra, mean_dec );

        ra[i] = mean_ra;
        dec[i] = mean_dec;
    }

    return 0;
}


/*--------------------------------------------------------------------------------*/
void
ACTSite( Site *p )
{
    p->latitude = deg2rad(-22.9585);
    p->east_longitude = deg2rad(-67.7876);
    p->elevation_m = 5188.;
    p->temperature_K = 273.;
    p->pressure_mb = 550.;
    p->relative_humidity = 0.2;
}






