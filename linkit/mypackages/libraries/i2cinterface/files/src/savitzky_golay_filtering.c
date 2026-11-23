/**************************************************************************************************
 *  @file   savitzky_golay_filtering.c
 *  @brief  OSA (CTM) savitzky golay filtering API
 *
 *  This file contains OSA (CTM) notch position API implementation.
 *
**************************************************************************************************/

#include <stdio.h>

int savitzkyGolayFiltering(float Y[], float SG[], int len, int SGLen, float X[]);

int savitzkyGolayFiltering(float Y[], float SG[], int len, int SGLen, float X[])
{   int n = ((SGLen-1)>>1);
    //printf("SGLen:%d\n", n);
    //X=range(0, len)

    //float X[len] = {0}
    X[0]=Y[0];
    X[len-1] = Y[len-1];
    for (int i=1; i<n; i++)
    {   X[i] = ((X[i-1]*i+Y[i])/((i+1.0))); // at the beginning
        X[len-1-i] = (X[len-1-i+1]*i+Y[len-1-i])/(i+1.0); // at the end
    }

    for (int i=n; i<len-n; i++)
    {   //print("i=" + str(i) + " n=" + str(n))
        X[i] = Y[i]*SG[n];
        for (int j=1; j<n+1; j++)
        {   //print("  "+ str(i) + " " + str(i-j) + " " + str(i+j))
            //print("X[" + str(i) + "] +=  Y[" + str(i-j) + "]*SG[" + str(-j+n) + "] + Y[" + str(i+j) + "]*SG[" + str(j+n) + "]")
            X[i] +=  ((Y[i-j]*SG[-j+n]) + (Y[i+j]*SG[j+n]));
        }
    }
    return 0;
}