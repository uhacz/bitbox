#pragma once

#ifdef __cplusplus
extern "C" {
#endif

double erand48(unsigned short xseed[3]);
double drand48(void);
long lrand48(void);
long nrand48(unsigned short xseed[3]);
long mrand48(void);
long jrand48(unsigned short xseed[3]);
void srand48(long seed);
unsigned short *seed48(unsigned short xseed[3]);
void lcong48(unsigned short p[7]);

#ifdef __cplusplus
};  /* end of extern "C" */
#endif