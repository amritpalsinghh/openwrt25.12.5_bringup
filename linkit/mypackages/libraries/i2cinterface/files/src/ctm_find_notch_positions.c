/**************************************************************************************************
 *  @file   ctm_find_notch_positions.c
 *  @brief  OSA (CTM) notch position API
 *
 *  This file contains OSA (CTM) notch position API implementation.
 *
**************************************************************************************************/

#include "ctm_find_notch_positions.h"
#include "savitzky_golay_filtering.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define min(X,Y) ((X) < (Y) ? (X) : (Y))
#define max(X,Y) ((X) > (Y) ? (X) : (Y))


int ctm14xxSimpleRegression(float x[], float y[], int n, float out[]);

/**************************************************************************************************
 *
 * @brief   This function find the notch positions in a given set of frequency, slice_pwr data array
 *
 * @param   i_f_freq input frequency as a array of float
 * @param   i_f_slicePwr input slice power as a array of float
 * @param   i_i_smoothData input smooth data as a interger (Default 1)
 * @param   i_i_method calibration options specified by E_OSA_CAL_OPTION
 * @param   i_f_Data output data as a array of float
 *
 * @return  returns calibration status specified by E_OSA_CAL_STATUS.
 *
 **************************************************************************************************/
int CTMFindNotchPosition(float i_f_freq[], float* i_f_slicePwr, int i_i_smoothData, int i_i_method, float* i_f_Data)
{   int num_passes = 2; //set to 2 to allow multiple passes where second pass uses the center index from the first pass. (set to 1 to use a single pass)
    int linear_fit = 1; // set to 1 to use linear fit to find notch edge mid-points. Set to 0 to skip this step.

    int gNumberOfBins = 785;
    int NUM_SAMP_FROM_MIN = 0;
    int EDGE_KEEP_OUT = 0;
    int AVG_SAMPLES = 12;
    int i_i_status = 0;


    float *SG = (float *)SG6;
    float gFreqPerBin = 0;
    float left_done[2]   = {0, 0};
    float left_edge[2]   = {-1.0, -1.0};
    float left_loc[2]    = {-1.0, -1.0};
    float left_hw_loc[2] = {-1.0, -1.0};
    float left_freq[2]   = {0.0, 0.0};


    float right_done[2] = {0.0, 0.0};
    float right_edge[2] = {-1.0, -1.0};
    float right_loc[2] = {-1.0, -1.0};
    float right_hw_loc[2] = {-1.0, -1.0};
    float right_freq[2] = {0.0, 0.0};
    float offset =0;
    float slope = 0;
    float rr = 0;
    float l_f_slicePwr[785] = {0};
    float FreqPerBinLoc = 0;
    float l_f_out[3] = {0};
    int SGLen = 0;
    do
    {   if (gNumberOfBins > NUM_SAMP_NOTCH_SWEEP) //#OSA raw data
        {   printf("ERROR (-2, ctm14xx_find_notch_positions.py): Instrument OSA raw data not supported");
            break;
            // TODO
            NUM_SAMP_FROM_MIN = NUM_SAMP_FROM_MIN_NOTCH_SWEEP;
            EDGE_KEEP_OUT = EDGE_KEEP_OUT_NOTCH_SWEEP;
            gFreqPerBin = 6.279645756137597e-004; //calculated locally now
            AVG_SAMPLES = AVG_SAMPLES_NOTCH_SWEEP;
        }
        else if (gNumberOfBins > NUM_SAMP_OSA)
        {   printf("ERROR (-3, ctm14xx_find_notch_positions.py): Instrument OSA data not supported");
            break;
            NUM_SAMP_FROM_MIN = NUM_SAMP_FROM_MIN_OSA;
            EDGE_KEEP_OUT = EDGE_KEEP_OUT_OSA;
            gFreqPerBin = 0.00130;
            AVG_SAMPLES = AVG_SAMPLES_OSA;
        }
        else if (gNumberOfBins > NUM_SAMP_3125)
        {   printf("WARNING (-4, ctm14xx_find_notch_positions.py): 14xx data not supported. 1370 datat expected");
            NUM_SAMP_FROM_MIN = NUM_SAMP_FROM_MIN_3125;
            EDGE_KEEP_OUT = EDGE_KEEP_OUT_3125;
            gFreqPerBin = 0.003125;
            SG= (float *)SG3;
            SGLen = sizeof(SG3)/sizeof(SG3[0]);
        }
        else
        {   // these are the expected stetting for the CTM1370
            NUM_SAMP_FROM_MIN = NUM_SAMP_FROM_MIN_625;
            EDGE_KEEP_OUT = EDGE_KEEP_OUT_625; //number of slices at edges of scan to skip
            gFreqPerBin = 0.00625;
            SG=(float *)SG6;
            SGLen = sizeof(SG6)/sizeof(SG6[0]);
        }
        float min_pwr[2] = {1e8, 1e8};
        float min_loc[2] = {-1.0, -1.0};
        int indr = gNumberOfBins-EDGE_KEEP_OUT;
        int half_num_samples = floor((0.5*indr));
        int i, j, k;

        /**For debug*/
        /*
                printf("Power Before Savitzky...\n");
                for(i =0; i < 785; i++)
                {
                    printf("power[%d] : %f\n", i , i_f_slicePwr[i]);
                }
        */
        if (i_i_smoothData == 1)
        {   /* Apply SG filter to the slice power data */
            i_i_status = savitzkyGolayFiltering(i_f_slicePwr, SG, 785, SGLen, l_f_slicePwr);
            memcpy(i_f_slicePwr, l_f_slicePwr, 785);
        }
        /**For debug*/
        /*
                printf("Power After Savitzky...\n");
                for(i =0; i < 785; i++)
                {
                    printf("power[%d] : %f\n", i , i_f_slicePwr[i]);
                }

                printf("EDGE_KEEP_OUT:%d\n", EDGE_KEEP_OUT);
                printf("half_num_samples:%d\n", half_num_samples);
        */
        /* find min power location in each half of the slice data */
        for (int l_i_count=EDGE_KEEP_OUT-1; l_i_count<half_num_samples; l_i_count++)
        {   if (i_f_slicePwr[l_i_count] < min_pwr[0])
            {   min_pwr[0] = i_f_slicePwr[l_i_count];
                min_loc[0] = l_i_count;
            }
            if (i_f_slicePwr[indr] < min_pwr[1])
            {   min_pwr[1] = i_f_slicePwr[indr];
                min_loc[1] = indr;
            }
            indr = indr - 1;
        }
        /*
                printf("min_loc[0]:%f\n", min_loc[0]);
                printf("min_loc[1]:%f\n", min_loc[1]);
                printf("min_pwr[0]:%f\n", min_pwr[0]);
                printf("min_pwr[1]:%f\n", min_pwr[1]);
        */

        for (int l_i_count=0; l_i_count<num_passes; l_i_count++)
        {   //DebugPrint ("Pass: " + str(p))
            if (l_i_count == 1)
            {   min_loc[0] = floor(0.5*(left_hw_loc[0] + right_hw_loc[0]));
                min_loc[1] = floor(0.5*(left_hw_loc[1] + right_hw_loc[1]));
            }

            /*
                        printf("min_loc[0]:%f\n", min_loc[0]);
                        printf("min_loc[1]:%f\n", min_loc[1]);
            */

            float left_mean[2] = {0.0, 0.0};
            //left_mean_ind = [range(0,AVG_SAMPLES), range(0,AVG_SAMPLES)] #TODO: Variable not needed
            //for j in range(0,AVG_SAMPLES):
            for (int l_i_intCount=0; l_i_intCount<AVG_SAMPLES; l_i_intCount++)
            {   //for k in [0, 1]:
                for (k=0; k<2; k++)
                {   //left_mean_ind[i][j] = min_loc(k)-NUM_SAMP_FROM_MIN+j-1
                    /*
                        printf("NUM_SAMP_FROM_MIN:%d\n", NUM_SAMP_FROM_MIN);
                        printf("min_loc[k]:%f\n", min_loc[k]);
                        printf ("(min_loc[k]-NUM_SAMP_FROM_MIN+l_i_intCount):%f\n", ((min_loc[k]-NUM_SAMP_FROM_MIN+l_i_intCount)));
                    */
                    left_mean[k] +=  i_f_slicePwr[(int)(min_loc[k]-NUM_SAMP_FROM_MIN+l_i_intCount)];
                }
            }
            left_mean[0] /= AVG_SAMPLES;
            left_mean[1] /= AVG_SAMPLES;
            /*
                        printf("left_mean[0]:%f\n", left_mean[0]);
                        printf("left_mean[1]:%f\n", left_mean[1]);
            */
            //left_hw_pwr = left_mean - EDGE_DEPTH_LIN*(left_mean - min_pwr)
            float left_hw_pwr[2] = {0.0, 0.0};
            left_hw_pwr[0] = left_mean[0] * (1- EDGE_DEPTH_LIN) + EDGE_DEPTH_LIN * min_pwr[0];
            left_hw_pwr[1] = left_mean[1] * (1- EDGE_DEPTH_LIN) + EDGE_DEPTH_LIN * min_pwr[1];

            /*
                        printf("left_hw_pwr[0]:%f\n", left_hw_pwr[0]);
                        printf("left_hw_pwr[1]:%f\n", left_hw_pwr[1]);
            */
            //for j in range(0, NUM_SAMP_FROM_MIN-AVG_SAMPLES):

            for (j=0; j<(NUM_SAMP_FROM_MIN-AVG_SAMPLES); j++)
            {   //for k in [0, 1]:
                for (k=0; k<2; k++)
                {   int ind = min_loc[k]-NUM_SAMP_FROM_MIN+j-1+AVG_SAMPLES;
                    if ((left_done[k]==0.0f) && (i_f_slicePwr[ind] < left_hw_pwr[k]))
                    {   left_edge[k] = i_f_slicePwr[ind];
                        left_loc[k] = ind;
                        left_done[k] = 1;
                        //linear interpolations
                        left_hw_loc[k] = ind-1. + ((left_hw_pwr[k]-i_f_slicePwr[ind-1])/(i_f_slicePwr[ind]-i_f_slicePwr[ind-1]));
                        FreqPerBinLoc = i_f_freq[(int)(floor(left_hw_loc[k]))]-i_f_freq[(int)(floor(left_hw_loc[k]+1))];
                        left_freq[k] = i_f_freq[(int)(floor(left_hw_loc[k]))] - FreqPerBinLoc*(left_hw_loc[k]-(int)floor(left_hw_loc[k]));
                    }
                }
            }

            /*
                        printf("left_hw_loc[0]:%f\n", left_hw_loc[0]);
                        printf("left_hw_loc[1]:%f\n", left_hw_loc[1]);
                        printf("left_freq[0]:%f\n", left_freq[0]);
                        printf("left_freq[1]:%f\n", left_freq[1]);
            */

            float right_mean[2] = {0.0, 0.0};

            //for j in range(0, AVG_SAMPLES):
            for(j=0; j<AVG_SAMPLES; j++)
            {   //for k in [0, 1]:
                for(k=0; k<2; k++)
                {   right_mean[k] +=  i_f_slicePwr[(int)(min_loc[k]+NUM_SAMP_FROM_MIN-j)];
                }
            }
            right_mean[0] /= AVG_SAMPLES;
            right_mean[1] /= AVG_SAMPLES;
            //right_hw_pwr = right_mean - EDGE_DEPTH_LIN*(right_mean - min_pwr)
            float right_hw_pwr[2] = {0, 0};
            right_hw_pwr[0] = right_mean[0] * (1- EDGE_DEPTH_LIN) + EDGE_DEPTH_LIN * min_pwr[0];
            right_hw_pwr[1] = right_mean[1] * (1- EDGE_DEPTH_LIN) + EDGE_DEPTH_LIN * min_pwr[1];

            // float right_done[2] = {0, 0};
            // float right_edge[2] = {-1.0, -1.0};
            // float right_loc[2] = {-1.0, -1.0};
            // float right_hw_loc[2] = {-1.0, -1.0};
            // float right_freq[2] = {0.0, 0.0};
            right_done[0] = 0;
            right_done[1] = 0;
            //printf("right_done[0]:%f\n", right_done[0]);
            //printf("right_done[1]:%f\n", right_done[1]);
            //for j in range(0, NUM_SAMP_FROM_MIN-AVG_SAMPLES):
            for (j=0; j<(NUM_SAMP_FROM_MIN-AVG_SAMPLES); j++)
            {   //for k in [0, 1]:
                for (k=0; k<2; k++)
                {   int ind = min_loc[k]+NUM_SAMP_FROM_MIN-j+1-AVG_SAMPLES; //TODO: Check +/-j ;
                    //printf("right_done[%d]:%f, i_f_slicePwr[ind]:%f, right_hw_pwr[k]:%f\n", k, right_done[k],
                    //i_f_slicePwr[ind], right_hw_pwr[k]);

                    if ((right_done[k]==0.0f) && (i_f_slicePwr[ind] < right_hw_pwr[k]))
                    {   right_edge[k] = i_f_slicePwr[ind];
                        right_loc[k] = (float)ind;
                        //printf("right_loc[k]:%f\n", right_loc[k]);
                        right_done[k] = 1;
                        //linear interpolation;
                        right_hw_loc[k] = ind + ((right_hw_pwr[k]-i_f_slicePwr[ind])/(i_f_slicePwr[ind+1]-i_f_slicePwr[ind]));
                        FreqPerBinLoc = i_f_freq[(int)(floor(right_hw_loc[k]))]-i_f_freq[(int)(floor(right_hw_loc[k]+1))];
                        right_freq[k] = i_f_freq[(int)(floor(right_hw_loc[k]))] - FreqPerBinLoc*(right_hw_loc[k]-(int)(floor(right_hw_loc[k])));
                    }
                }
            }
            /*
                        printf("right_done[0]:%f\n", right_done[0]);
                        printf("right_done[1]:%f\n", right_done[1]);
                        printf("right_edge[0]:%f\n", right_edge[0]);
                        printf("right_edge[1]:%f\n", right_edge[1]);

                        printf("right_loc[0]:%f\n", right_loc[0]);
                        printf("right_loc[1]:%f\n", right_loc[1]);
                        printf("right_hw_loc[0]:%f\n", right_hw_loc[0]);
                        printf("right_hw_loc[1]:%f\n", right_hw_loc[1]);
                        printf("right_freq[0]:%f\n", right_freq[0]);
                        printf("right_freq[1]:%f\n", right_freq[1]);

                        printf("left_hw_loc:%f\n",left_hw_loc);
                        printf("right_hw_loc:%f\n",right_hw_loc);
            */
            //printf("Linera Fit...\n");

            if (linear_fit == 1)
            {   //Linear fit around the averaging window of the 3dB point, adjust the actual location

                //for k in [0, 1]:
                for(k=0; k<2; k++)
                {   int start_ = (int) (max(1, (int) (round(left_hw_loc[k] - AVG_SAMPLES*0.5))));
                    //DebugPrint("start_:", start_)
                    int end_ = (int) (min(gNumberOfBins, (int )(floor(left_hw_loc[k] + AVG_SAMPLES*0.5+0.5))));
                    //DebugPrint("end_:", end_)
                    int n = end_ - start_ + 1;

                    //printf("end_:%d, n:%d\n", end_, n);

                    float *x, *y;
                    x = (float *)malloc(sizeof(float ) * n);
                    if(!x)
                    {   return -1; //TODO: Error
                    }
                    y = (float *)malloc(sizeof(float) * n);
                    if(!y)
                    {   return -1; //TODO: Error
                    }
                    memset(x, 0, ((sizeof(float)) * n));
                    memset(y, 0, ((sizeof(float)) * n));

                    //TODO: memcpy()


                    for (j=0; j<n; j++)
                    {   x[j] = (float) (j + start_);
                        y[j] = i_f_slicePwr[j + start_];
                        //printf("y[%d]:%f, x[%d] : %f (i_f_slicePwr[%d]:%f\n", j,y[j], j,x[j],j+start_,i_f_slicePwr[j+start_]);
                    }

                    i_i_status = ctm14xxSimpleRegression(y, x, n, l_f_out);

                    if(x)
                    {   free(x);
                        x = NULL;
                    }
                    if(y)
                    {   free(y);
                        y = NULL;
                    }

                    slope = l_f_out[0];
                    offset = l_f_out[1];
                    rr = l_f_out[2];

                    left_hw_loc[k] = slope * left_hw_pwr[k] + offset;
                    FreqPerBinLoc = i_f_freq[(int)(floor(left_hw_loc[k]))] - i_f_freq[(int)(floor(left_hw_loc[k])+1)];

#if 0
                    printf("offset:%f\n", offset);
                    printf("slope:%f\n", slope);
                    printf("rr:%f\n", rr);
                    printf("left_hw_loc[%d]:%f\n", k, left_hw_loc[k]);
                    printf("FreqPerBinLoc:%f\n", FreqPerBinLoc);
#endif

                }

                for(k=0; k<2; k++)
                {   int start_ = (int) (max(1, (int) (round(right_hw_loc[k] - AVG_SAMPLES*0.5))));
                    int end_ = (int) (min(gNumberOfBins, (int )(round(right_hw_loc[k] + AVG_SAMPLES*0.5+0.5))));
                    int n = end_ - start_ + 1;

                    //printf("end_:%d, n:%d\n", end_, n);

                    float *x, *y;
                    x = (float *)malloc(sizeof(float ) * n);
                    if(!x)
                    {   return -1; //TODO: Error
                    }
                    y = (float *)malloc(sizeof(float) * n);
                    if(!y)
                    {   return -1; //TODO: Error
                    }
                    memset(x, 0, ((sizeof(float)) * n));
                    memset(y, 0, ((sizeof(float)) * n));

                    //TODO: memcpy()


                    for (j=0; j<n; j++)
                    {   x[j] = (float) (j + start_);
                        y[j] = i_f_slicePwr[j + start_];
                        //printf("y[%d]:%f, x[%d] : %f (i_f_slicePwr[%d]:%f\n", j,y[j], j,x[j],j+start_,i_f_slicePwr[j+start_]);
                    }

                    i_i_status = ctm14xxSimpleRegression(y, x, n, l_f_out);

                    if(x)
                    {   free(x);
                        x = NULL;
                    }
                    if(y)
                    {   free(y);
                        y = NULL;
                    }

                    slope = l_f_out[0];
                    offset = l_f_out[1];
                    rr = l_f_out[2];

                    right_hw_loc[k] = slope * right_hw_pwr[k] + offset;
                    FreqPerBinLoc = i_f_freq[(int)(floor(right_hw_loc[k]))] - i_f_freq[(int)(floor(right_hw_loc[k])+1)];

                    //printf("offset:%f\n", offset);
                    //printf("slope:%f\n", slope);
                    //printf("rr:%f\n", rr);
                    //printf("right_hw_loc[%d]:%f\n", k, right_hw_loc[k]);
                    //printf("FreqPerBinLoc:%f\n", FreqPerBinLoc);

                }

            }

        }

        char * res_key[] = {"notch_center_bin0", "notch_width0", "notch_center_bin1", "notch_width1"};
        //float res_value[4] = {0};
        for (k = 0, j=0; k < 2, j < 4; k++, j+=2)
        {   //i_f_Data[j] = k;
            *(i_f_Data + j) = (0.5*(left_hw_loc[k] + right_hw_loc[k]) - ML_TO_FW_INDEX_OFFSET);
            //i_f_Data[j+2] = k;
            *(i_f_Data + j+1) = -left_hw_loc[k] + right_hw_loc[k];
            //printf("%s: %f, %s:%f\n",res_key[j], *(i_f_Data + j), res_key[j+1], *(i_f_Data + j+1));
        }
    }
    while(0);

    return i_i_status;
}
