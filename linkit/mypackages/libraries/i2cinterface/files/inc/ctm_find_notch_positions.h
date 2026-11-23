#ifndef CTM_NOTCH_POSITIONS
#define CTM_NOTCH_POSITIONS

#include <math.h>

//#define AVG_SAMPLES  12 //number of samples to average when searching for valley
#define AVG_SAMPLES_OSA  50
#define AVG_SAMPLES_NOTCH_SWEEP  125
#define NUM_SAMP_FROM_MIN_625  34 //number of samples which to start from min location for 6.25 GHz bin processing, 34*6.25GHz
#define NUM_SAMP_FROM_MIN_3125  68 //number of samples which to start from min for 3.125 GHz bin processing
#define NUM_SAMP_FROM_MIN_OSA  150 //number of samples which to start from min location for 1.3 GHz bin processing, 100*1.3GHz
#define NUM_SAMP_FROM_MIN_NOTCH_SWEEP  380 //number of samples which to start from min location for 1.3 GHz bin processing, 100*1.3GHz
#define NUM_SAMP_NOTCH_SWEEP  8000 //OSA raw data, found in files such as: T:\Telecom Development\CTM1400\Optics\SLED\sled_spectrum.mat
#define NUM_SAMP_OSA  2500 //OSA raw data, found in files such as: T:\Telecom Development\CTM1400\Optics\SLED\sled_spectrum.mat
#define NUM_SAMP_3125  1000  //note, when the size of the image_data (or cgsp data) vector is greater than NUM_SAMP_3125, then indicates 3.125 GHz bins
#define EDGE_KEEP_OUT_NOTCH_SWEEP  200 //number of samples at edges of scan to skip for 1.3 GHz bins
#define EDGE_KEEP_OUT_OSA  100 //number of samples at edges of scan to skip for 1.3 GHz bins
#define EDGE_KEEP_OUT_3125  44 //number of slices at edges of scan to skip for 3.125 GHz bins
#define EDGE_KEEP_OUT_625  25 //number of slices at edges of scan to skip for 6.25 GHz bins
#define ML_TO_FW_INDEX_OFFSET  1 // note that to align bins for ML calc with FW, set to 1.
#define EDGE_DEPTH_LIN 0.5
#define SLICE_MIN_PWR_MW pow(10,(-45.0/10))

#endif