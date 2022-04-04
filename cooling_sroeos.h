#ifndef COOLING_SROEOS_HINCLUDED
#define COOLING_SROEOS_HINCLUDED
/*
 * COOLING CODE FOR sroeos adapted from helm
 */

#include "param.h"
#include "physconst.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define CL_Rgascode         RGAS
#define CL_Eerg_gm_degK     CL_Rgascode
#define CL_Eerg_gm_degK3_2  1.5*CL_Eerg_gm_degK
#define CL_OneOverGammaM1  1.5
#define CL_NMAXBYTETABLE   56000
#define CL_SpeedOfLight    2.99792458e10

typedef struct {
    double Total;
} PERBARYON;

typedef struct CoolingParametersStruct {
    char EosFileName[512]; 
    double dSmallT;
    double dTnu; 
    double dLnu;
    int    bIncludeHeating;
} COOLPARAM;

typedef struct CoolingParticleStruct {
  double ye;
} COOLPARTICLE;


typedef struct CoolingPKDStruct {
/* Heating Cooling Context */
    /* Cosmology hold-overs */
    double     z; /* Redshift */
    double     dTime;
    /* Units and conversion constants */
    double     dGmPerCcUnit;
    double     dComovingGmPerCcUnit;
    double     dErgPerGmUnit;
    double     dSecUnit;
    double     dErgPerGmPerSecUnit;
    double     diErgPerGmUnit;
    double     dKpcUnit;
    /* SroEos specific */
    double     dSmallT;
    double     dSroEosEnergyShift;
    double     dSroEosRhoMin;
    double     dSroEosTempMin;
    double     dSroEosYeMax;

    double     heat0, cool0, L0, Tnu;
    int        includeHeating;

} COOL;

/* Per-thread data.  Not used for this cooling algorithm */
typedef struct clDerivsDataStruct {
} clDerivsData;


COOL *CoolInit( );

///Frees memory and deletes cl
void CoolFinalize( COOL *cl );

clDerivsData *CoolDerivsInit(COOL *cl);

void CoolDerivsFinalize(clDerivsData *cld ) ;

void clInitConstants( COOL *cl, double dGMPerCcunit,
                      double dComovingGmPerCcUnit, double dErgPerGmUnit,
                      double dSecUnit, double dKpcUnit, COOLPARAM CoolParam);

/* Doesn't do anything, needed by Sph.C */

void CoolInitRatesTable( COOL *cl, COOLPARAM CoolParam);
void CoolAddParams( COOLPARAM *CoolParam, PRM );

/* Needed by InOutput.h */
#define COOL_ARRAY0_EXT  "ye"
double COOL_ARRAY0(COOL *cl_, COOLPARTICLE *cp,double aa);
#define COOL_ARRAY0( cl_, cp, aa ) ((cp)->ye)
double COOL_SET_ARRAY0(COOL *cl_, COOLPARTICLE *cp,double aa, double bb_val);
#define COOL_SET_ARRAY0( cl_, cp, aa, bb_val ) ((cp)->ye = bb_val)

#define COOL_ARRAY1_EXT  "NA"
double COOL_ARRAY1(COOL *cl_, COOLPARTICLE *cp,double aa);
#define COOL_ARRAY1( cl_, cp, aa ) (0)
double COOL_SET_ARRAY1(COOL *cl_, COOLPARTICLE *cp,double aa, double bb_val);
#define COOL_SET_ARRAY1( cl_, cp, aa, bb_val ) (0)

#define COOL_ARRAY2_EXT  "NA"
double COOL_ARRAY2(COOL *cl_, COOLPARTICLE *cp,double aa);
#define COOL_ARRAY2( cl_, cp, aa ) (0)
double COOL_SET_ARRAY2(COOL *cl_, COOLPARTICLE *cp,double aa, double bb_val);
#define COOL_SET_ARRAY2( cl_, cp, aa, bb_val ) (0)

#define COOL_ARRAY3_EXT  "NA"
double COOL_ARRAY3(COOL *cl, COOLPARTICLE *cp, double aa);
#define COOL_ARRAY3(cl_, cp, aa ) (0)
double COOL_SET_ARRAY3(COOL *cl_, COOLPARTICLE *cp,double aa, double bb_val);
#define COOL_SET_ARRAY3( cl_, cp, aa, bb_val ) (0)

/// Not implemented, but required to keep compiling from crashing
double COOL_EDOT( COOL *cl_, COOLPARTICLE *cp_, double ECode_, double rhoCode_,
                  double T_, double *posCode_ );
#define COOL_EDOT( cl_, cp_, ECode_, rhoCode_, T_, posCode_) (0)

//// Not implemented, but required to keep compiling from crashing
double COOL_COOLING( COOL *cl_, COOLPARTICLE *cp_, double ECode_,
                     double rhoCode_, double T_, double *posCode_ );
#define COOL_COOLING( cl_, cp_, ECode_, rhoCode_, T_, posCode_) (0)

double CoolCodeEnergyToTemperature( COOL *Cool, COOLPARTICLE *cp, double E, double rhoCode,
                                    double zMetal);

/* Note: nod to cosmology (z parameter) unavoidable unless we want to access
 * cosmo.[ch] from here */
void CoolSetTime( COOL *Cool, double dTime, double z );

/* Unit conversion routines */

double CoolCodeTimeToSeconds( COOL *Cool, double dCodeTime );

#define CoolCodeTimeToSeconds( Cool, dCodeTime ) ((Cool)->dSecUnit*(dCodeTime))

double CoolSecondsToCodeTime( COOL *Cool, double dTime );

#define CoolSecondsToCodeTime( Cool, dTime ) ((dTime)/(Cool)->dSecUnit)

double CoolCodeEnergyToErgPerGm( COOL *Cool, double dCodeEnergy );

#define CoolCodeEnergyToErgPerGm( Cool, dCodeEnergy ) \
    ((Cool)->dErgPerGmUnit*(dCodeEnergy))

double CoolErgPerGmToCodeEnergy( COOL *Cool, double dEnergy );

#define CoolErgPerGmToCodeEnergy( Cool, dEnergy ) \
    ((Cool)->diErgPerGmUnit*(dEnergy))

double CoolCodeWorkToErgPerGmPerSec( COOL *Cool, double dCodeWork );

#define CoolCodeWorkToErgPerGmPerSec( Cool, dCodeWork ) \
    ((Cool)->dErgPerGmPerSecUnit*(dCodeWork))

double CoolErgPerGmPerSecToCodeWork( COOL *Cool, double dWork );

#define CoolErgPerGmPerSecToCodeWork( Cool, dWork ) \
    ((dWork)/(Cool)->dErgPerGmPerSecUnit)

double CodeDensityToComovingGmPerCc( COOL *Cool, double dCodeDensity );

#define CodeDensityToComovingGmPerCc( Cool, dCodeDensity )  \
    ((Cool)->dComovingGmPerCcUnit*(dCodeDensity))

void CoolIntegrateEnergyCode(COOL *cl, clDerivsData *cData, COOLPARTICLE *cp,
                             double *E, double PdV, double rho, double Zmetals,
                             double *r, double tStep );

void CoolDefaultParticleData( COOLPARTICLE *cp );

void CoolInitEnergyAndParticleData( COOL *cl, COOLPARTICLE *cp, double *E,
    double dDensity, double dTemp, double ye);

/* Not implemented, but required to keep compiling from crashing */
double CoolEdotInstantCode(COOL *cl, COOLPARTICLE *cp, double ECode,
                           double rhoCode, double ZMetal, double *posCode );

void CoolPARTICLEtoPERBARYON(COOL *cl_, PERBARYON *Y, COOLPARTICLE *cp);

#define CoolPARTICLEtoPERBARYON(cl_, Y, cp) { \
    0;}

void CoolCodePressureDensity( COOL *cl, COOLPARTICLE *cp,
    double press, double fDensity, double *ie, double *c, double *gamma);

void CoolCodePressureOnDensitySoundSpeed( COOL *cl, COOLPARTICLE *cp,
    double uPred, double fDensity, double gamma, double gammam1,
    double *PoverRho, double *c );

/* Place holder function (not implemented but needed */
void CoolTableReadInfo( COOLPARAM *CoolParam, int cntTable, int *nTableColumns,
                        char *suffix );

/* Place holder function (not implemented but needed */
void CoolTableRead( COOL *Cool, int nData, void *vData);

int CoolClean( COOL *cl, COOLPARTICLE *cp, double gamma, double *rhoCode, double *eCode, double *pCode, double *csCode);

double clThermalEnergy( COOL *cl, double T, double dens, double ye);

void cl_sro_eos_low( int mode, COOL *cl, const double rho, double *p, double *T, double *ener, double *cs);

void cl_sro_eos_rho_T(COOL *cl, const double ye, const double rho, double T, double *press, double *E, double *cs, double *gamma);
void cl_sro_eos_rho_ener(COOL *cl, const double ye, const double rho, const double E, double *press, double *T, double *cs, double *gamma);
#ifdef __cplusplus
}
#endif

#endif
