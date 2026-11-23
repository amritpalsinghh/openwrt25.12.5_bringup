/**************************************************************************************************
 *  @file   ctm14xx_simple_regression.c
 *  @brief  OSA (CTM) simple regression API
 *
 *  This file contains OSA (CTM) simple regression API implementation.
 *
**************************************************************************************************/

#include <stdio.h>

int ctm14xxSimpleRegression(float x[], float y[], int n, float out[])
{   /*#function [offset, slope, rr] = ctm14xx_simple_regression(x, y);
    #
    # Fit a line to x,y data. Return offset (b), slope (m), and fitting
    # values in rr, for y = m*x + b;
    #
    # Created:  2019, G. Bartolini - refer to CTM 1450 and OCM 3580 firmware for details
    # Modified: */

    // int n = len(x)
    // if n!=len(y):
    //     print("ERROR(-1, ctm14xx_simple_regression): length of x and y vector do not match")
    //     exit(-1)

    // compute means
    /*
        for(int i=0;i<n;i++)
        {
        printf ("entering x:%f ", x[i]);
        printf ("entering y:%f\n", y[i]);
        }
    */
    float x_ = 0.0;
    float y_ = 0.0;
    float slope = 0;
    //for i in range (0, n):
    for(int i=0; i<n; i++)
    {   x_ += x[i];
        y_ += y[i];
    }

    x_ /= n;
    y_ /= n;

    //printf("X_ and Y_ ,%f, %f, %d\n",x_,y_,n);

// compute sum of squares of x and y and cross product
    float Sxx = 0.0;
    float Syy = 0.0;
    float Sxy = 0.0;
//for i in range (0, n):
    for (int i=0; i<n; i++)
    {   //printf("x[i]:%f, x_:%f, y[i]:%f, y_:%f\n",x[i], x_, y[i], y_);
        Sxx += ((x[i] - x_) * (x[i] - x_));
        Syy += ((y[i] - y_) * (y[i] - y_));
        Sxy += ((x[i] - x_) * (y[i] - y_));
    }
// update the regression coefficients
    out[0] = (Sxy / Sxx);
    out[1] = (y_ - (out[0] * x_));
    out[2] = (Syy - (out[0] * out[0] * Sxx));

    //printf("Syy:%f slope:%f slope:%f Sxx:%f\n", Syy, slope, slope, Sxx);
    //printf("out[0]:%f, out[1]:%f, out[2]:%f\n", out[0], out[1], out[2]);
    //return [offset, slope, rr]
    return 0;
}

#if 0
// code below is for testing
if False:
offset = 11
         slope = 1.2

                 x=[]
                       y=[]

                         for i in range(0, 20):
                                 x.append(-5.+i*.2)
                                 y.append(offset+slope*(-5.+i*.2))

                                 d = ctm14xx_simple_regression(x, y)

                                     print(x)
                                     print(y)

                                     print("offset="+str(d[0]))
                                     print("slope= "+str(d[1]))
                                     print("rr=    "+str(d[2]))
        }
#endif
