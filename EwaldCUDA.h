#ifndef _EWALD_CUDA_H_
#define _EWALD_CUDA_H_ 

#include "HostCUDA.h"

#define NEWH 80
#define BLOCK_SIZE 128

/** @brief Data for the Ewald h loop in the CUDA kernel
 */
typedef struct {
  cudatype hx, hy, hz; 
  cudatype hCfac, hSfac; 
} EwtData;

/** @brief CUDA version of complete MultipoleMoments for Ewald
 */
typedef struct {
#ifndef HEXADECAPOLE
  cudatype xx, xy, xz, yy, yz, zz; 
#endif
  cudatype totalMass; 
  cudatype cmx, cmy, cmz; 

} MultipoleMomentsData; 

/** @brief CUDA version of MOMC for Ewald
 */
typedef struct {
  cudatype m;
  cudatype xx,yy,xy,xz,yz;
  cudatype xxx,xyy,xxy,yyy,xxz,yyz,xyz;
  cudatype xxxx,xyyy,xxxy,yyyy,xxxz,yyyz,xxyy,xxyz,xyyz;
  cudatype zz;
  cudatype xzz,yzz,zzz;
  cudatype xxzz,xyzz,xzzz,yyzz,yzzz,zzzz;
} MomcData;

/** @brief Parameters and data for Ewald in the CUDA kernel
 */
typedef struct {
  MultipoleMomentsData mm; 
  MomcData momcRoot;
  
  int n, nReps, nEwReps, nEwhLoop;
  cudatype L, fEwCut, alpha, alpha2, k1, ka, fEwCut2, fInner2;

} EwaldReadOnlyData; 

/// @brief structure to hold information specific to GPU Ewald
typedef struct {
  int EwaldRange[2];            /**< First and last particle on the
                                 * GPU; only used for small phase  */
  int *EwaldMarkers;            /**< indices of active particles  */
  EwtData *ewt;                 /**< h-loop table  */
  EwaldReadOnlyData *cachedData; /**< Root moment and other Ewald parameters  */
} EwaldData; 

void EwaldHostMemorySetup(EwaldData *h_idata, int size, int nEwhLoop, int largephase); 
void EwaldHostMemoryFree(EwaldData *h_idata, int largephase); 
void EwaldHost(CompactPartData *d_localParts, VariablePartData *d_localVars,
               EwaldData *h_idata, cudaStream_t stream, void *cb, int myIndex, int largephase);

__global__ void EwaldKernel(CompactPartData *particleCores, VariablePartData *particleVars, int *markers, int largephase, int First, int Last);

#endif

