//=============================================================================================================
/**
* @file     histogram.cpp
* @author   Ricky Tjen <ricky270@student.sgu.ac.id>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>;
* @version  1.0
* @date     March, 2016
*
* @section  LICENSE
*
* Copyright (C) 2016, Ricky Tjen and Matti Hamalainen. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that
* the following conditions are met:
*     * Redistributions of source code must retain the above copyright notice, this list of conditions and the
*       following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
*       the following disclaimer in the documentation and/or other materials provided with the distribution.
*     * Neither the name of MNE-CPP authors nor the names of its contributors may be used
*       to endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*
* @brief    Example of reading raw data and presenting the result in histogram form
*
*/


//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include <iostream>
#include <math.h>
#include <ctime>
#include <cstdlib>
#include <Eigen/Dense>
#include <string>
#include <fiff/fiff.h>
#include <mne/mne.h>
#include <utils/mnemath.h>
#include <disp/bar.h>
#include <disp/spline.h>


//*************************************************************************************************************
//=============================================================================================================
// QT INCLUDES
//=============================================================================================================

#include <QVector>
#include <QDebug>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace FIFFLIB;
using namespace MNELIB;
using namespace std;
using namespace DISPLIB;


//*************************************************************************************************************
//=============================================================================================================
// MAIN
//=============================================================================================================

//=============================================================================================================

//sineWaveGenerator function - used to create synthetic data to test histogram functionality
#ifndef M_PI
const double M_PI = 3.14159265358979323846;
#endif

Eigen::VectorXd sineWaveGenerator(double amplitude, double xStep, int xNow, int xEnd)
{
    unsigned int iterateAmount = ceil((xEnd-xNow)/xStep);
    Eigen::VectorXd sineWaveResultOriginal;
    sineWaveResultOriginal.resize(iterateAmount+1);
    Eigen::VectorXd sineWaveResult = sineWaveResultOriginal.transpose();
    double sineResult;
    double omega = 2.0*M_PI;
    int iterateCount = 0;
    for (double step = xNow; step < xEnd; step +=xStep)
    {
        sineResult = amplitude* (sin(omega * step));
        sineWaveResult(iterateCount) = sineResult;
        iterateCount++;
    }
    return sineWaveResult;
}

int main(int argc, char *argv[])
{
    //code to read sample data copied from readRaw example by Christoph Dinh
    QApplication a(argc, argv);
    QFile t_fileRaw("./MNE-sample-data/MEG/sample/sample_audvis_raw.fif");

    float from = 40.0f;
    float to = 46.01f;

    bool in_samples = false;

    bool keep_comp = true;

    //
    //   Setup for reading the raw data
    //
    FiffRawData raw(t_fileRaw);

    //
    //   Set up pick list: MEG + STI 014 - bad channels
    //
    //
    QStringList include;
    include << "STI 014";
    bool want_meg   = true;
    bool want_eeg   = false;
    bool want_stim  = false;

    RowVectorXi picks = raw.info.pick_types(want_meg, want_eeg, want_stim, include, raw.info.bads);

    //
    //   Set up projection
    //
    qint32 k = 0;
    if (raw.info.projs.size() == 0)
        printf("No projector specified for these data\n");
    else
    {
        //
        //   Activate the projection items
        //
        for (k = 0; k < raw.info.projs.size(); ++k)
            raw.info.projs[k].active = true;

        printf("%d projection items activated\n",raw.info.projs.size());
        //
        //   Create the projector
        //
        fiff_int_t nproj = raw.info.make_projector(raw.proj);

        if (nproj == 0)
            printf("The projection vectors do not apply to these channels\n");
        else
            printf("Created an SSP operator (subspace dimension = %d)\n",nproj);
    }

    //
    //   Set up the CTF compensator
    //
    qint32 current_comp = raw.info.get_current_comp();
    qint32 dest_comp = -1;

    if (current_comp > 0)
        printf("Current compensation grade : %d\n",current_comp);

    if (keep_comp)
        dest_comp = current_comp;

    if (current_comp != dest_comp)
    {
        qDebug() << "This part needs to be debugged";
        if(MNE::make_compensator(raw.info, current_comp, dest_comp, raw.comp))
        {
            raw.info.set_current_comp(dest_comp);
            printf("Appropriate compensator added to change to grade %d.\n",dest_comp);
        }
        else
        {
            printf("Could not make the compensator\n");
            return -1;
        }
    }
    //
    //   Read a data segment
    //   times output argument is optional
    //
    bool readSuccessful = false;
    MatrixXd data;
    MatrixXd times;
    if (in_samples)
        readSuccessful = raw.read_raw_segment(data, times, (qint32)from, (qint32)to, picks);
    else
        readSuccessful = raw.read_raw_segment_times(data, times, from, to, picks);

    if (!readSuccessful)
    {
        printf("Could not read raw segment.\n");
        return -1;
    }

    printf("Read %d samples.\n",(qint32)data.cols());

    Eigen::VectorXd dataSine;
    dataSine = sineWaveGenerator(1.0e-30,(1.0/1e6), 0.0, 1.0);  //creates synthetic data using sineWaveGenerator function

    bool bMakeSymmetrical;
    bMakeSymmetrical = false;       //bMakeSymmetrical option: false means data is unchanged, true means histogram x axis is symmetrical to the right and left
    int classAmount = 50;          //initialize the amount of classes and class frequencies
    double inputGlobalMin = 0.0,
           inputGlobalMax = 0.0;
    Eigen::VectorXd resultClassLimit;
    Eigen::VectorXi resultFrequency;
    //start of the histogram calculation, similar to matlab function of the same name
    MNEMath::histcounts(data, bMakeSymmetrical, classAmount, resultClassLimit, resultFrequency, inputGlobalMin, inputGlobalMax);   //user input to normalize and sort the data matrix
    std::cout << "resultClassLimits = " << resultClassLimit << std::endl;
    std::cout << "resultFrequency = " << resultFrequency << std::endl;
    int precision = 2;             //format for the amount digits of coefficient shown in the histogram
    //start of the histogram display function using Qtcharts
    Spline<double>* barObj = new Spline<double>(resultClassLimit, resultFrequency, precision);
    //Bar<double>* barObj = new Bar<double>(resultClassLimit, resultFrequency, precision);
    barObj->setData()
    barObj->resize(400,300);
    barObj->show();

    std::cout << data.block(0,0,10,10);
    return a.exec();
}


//*************************************************************************************************************
//#include <stdlib.h>
//#include <QVector>
//#include <QDebug>
//#include <QtWidgets/QApplication>
//#include <QtWidgets/QMainWindow>
//#include <QtCharts/QChartView>
//#include <QtCharts/QSplineSeries>
//#include <iostream>
//QT_CHARTS_USE_NAMESPACE

//using namespace std;
//int main(int argc, char *argv[])
//  {
//      QApplication a(argc, argv);
//      std::vector<double> xAxisData;
//      std::vector<int> matClassFrequencyData;
//      int iClassAmount = 50;

//     xAxisData = {-3.86, -3.68, -3.50, -3.32, -3.14, -2.96, -2.78, -2.59, -2.41, -2.23, -2.05, -1.87, -1.69, -1.51, -1.33, -1.14, -0.96, -0.78, -0.60, -0.42, -0.24, -0.06, 0.11, 0.30, 0.48, 0.66, 0.84, 1.02, 1.20, 1.38, 1.56, 1.75, 1.93, 2.11, 2.29, 2.47, 2.29, 2.47, 2.65, 2.83, 3.02, 3.20, 3.38, 3.56, 3.74, 3.92, 4.10, 4.28, 4.47, 4.65, 4.83, 5.01} ;
//     matClassFrequencyData = {1, 6, 8, 22, 41, 41, 57, 83, 49, 199, 494, 1192, 2515, 5083, 8362, 9537, 5083, 11451, 13713, 28515, 46887, 58029, 243666, 45902, 17001, 19831, 10600, 9167, 7870, 7056, 4508, 2326, 1111, 319, 407, 142, 129, 99, 85, 72, 57, 19, 27, 28, 11, 4, 4, 2, 4, 1};

//      QSplineSeries *series = new QSplineSeries();
//      series->setName("Histogram");

//      double minAxisX,
//             maxAxisX,
//             classMark;                                                   //class mark is the middle point between lower and upper class limit
//      minAxisX = xAxisData[0];
//      maxAxisX = xAxisData[iClassAmount];
//      int maximumFrequency = 0;                                  //maximumFrequency used to scale the y-axis
//      for (int ir=0; ir < iClassAmount; ir++)
//      {
//          classMark = (xAxisData[ir] + xAxisData[ir+1])/2;
//          series->append(classMark, matClassFrequencyData[ir]);

//          if (matClassFrequencyData[ir] > maximumFrequency)    //used to find the maximum y-axis
//          {
//              maximumFrequency = matClassFrequencyData[ir];
//          }
//      }
//      QChart *chart = new QChart();
//      chart->addSeries(series);
//      chart->legend()->setVisible(true);
//      chart->legend()->setAlignment(Qt::AlignBottom);
//      chart->setTitle("Spline Histogram");
//      chart->createDefaultAxes();
//      chart->axisX()->setRange(minAxisX, maxAxisX);
//      chart->axisY()->setRange(0, maximumFrequency);

//      QChartView *chartView = new QChartView(chart);
//      chartView->setRenderHint(QPainter::Antialiasing);

//      QMainWindow window;
//      window.setCentralWidget(chartView);
//      window.resize(400, 300);
//      window.show();

//      return a.exec();
//}
