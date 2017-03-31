//=============================================================================================================
/**
* @file     deepcntk.cpp
* @author   Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     February, 2017
*
* @section  LICENSE
*
* Copyright (C) 2017 Christoph Dinh and Matti Hamalainen. All rights reserved.
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
* @brief    Contains the implementation of the DeepCNTK class.
*
*/

//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "deepcntk.h"

#include <deep/deep.h>
#include <deep/deepmodelcreator.h>

#include <dispCharts/lineplot.h>
#include <disp/deepmodelviewer/deepviewer.h>
#include <disp/deepmodelviewer/controls.h>

#include <iostream>
#include <random>


//*************************************************************************************************************
//=============================================================================================================
// QT INCLUDES
//=============================================================================================================

#include <QtConcurrent>
#include <QFutureWatcher>
#include <QProgressDialog>


//*************************************************************************************************************
//=============================================================================================================
// CNTK INCLUDES
//=============================================================================================================

#include <CNTKLibrary.h>


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace DEEPCNTKEXTENSION;
using namespace ANSHAREDLIB;
using namespace Eigen;
using namespace DEEPLIB;
using namespace DISPLIB;
using namespace DISPCHARTSLIB;
//using namespace QtCharts;
using namespace CNTK;


//*************************************************************************************************************
//=============================================================================================================
// STATIC FUNCTIONS
//=============================================================================================================

// Helper function to generate a random data sample
void generateRandomDataSamples(int sample_size, int feature_dim, int num_classes, MatrixXf& X, MatrixXf& Y)
{
    MatrixXi t_Y = MatrixXi::Zero(sample_size, 1);
    for(int i = 0; i < t_Y.rows(); ++i) {
        t_Y(i,0) = rand() % num_classes;
    }

    std::default_random_engine generator;
    std::normal_distribution<float> distribution(0.0,1.0);

    X = MatrixXf::Zero(sample_size, feature_dim);
    for(int i = 0; i < X.rows(); ++i) {
        for(int j = 0; j < X.cols(); ++j) {
            float number = distribution(generator);
            X(i,j) = (number + 3) * (t_Y(i) + 1);
        }
    }

    Y = MatrixXf::Zero(sample_size, num_classes);

    for(int i = 0; i < Y.rows(); ++i) {
        Y(i,t_Y(i)) = 1;
    }
}


//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

DeepCNTK::DeepCNTK()
: m_pControlPanel(Q_NULLPTR)
, m_pControl(Q_NULLPTR)
, m_pDeepViewer(Q_NULLPTR)
{

}


//*************************************************************************************************************

DeepCNTK::~DeepCNTK()
{

}


//*************************************************************************************************************

QSharedPointer<IExtension> DeepCNTK::clone() const
{
    QSharedPointer<DeepCNTK> pDeepCNTKClone(new DeepCNTK);
    return pDeepCNTKClone;
}


//*************************************************************************************************************

void DeepCNTK::init()
{

    setupModel();

    //
    // Init view
    //
    if(!m_pControlPanel) {
        m_pControlPanel = new Controls;
        connect(m_pControlPanel, &Controls::requestTraining_signal, this, &DeepCNTK::trainModel);
    }

    //
    // Create the viewer
    //
    if(!m_pDeepViewer) {
        if(m_pDeep) {
            m_pDeepViewer = new DeepViewer(m_pDeep, false);
        }
        else {
            m_pDeepViewer = new DeepViewer(false);
        }

        m_pControlPanel->setDeepViewer(m_pDeepViewer);
        m_pDeepViewer->setWindowTitle("Deep CNTK");
    }
}


//*************************************************************************************************************

void DeepCNTK::unload()
{

}


//*************************************************************************************************************

QString DeepCNTK::getName() const
{
    return "Deep CNTK";
}


//*************************************************************************************************************

bool DeepCNTK::hasMenu() const
{
    return true;
}


//*************************************************************************************************************

QMenu *DeepCNTK::getMenu()
{
    return Q_NULLPTR;
}


//*************************************************************************************************************

bool DeepCNTK::hasControl() const
{
    return true;
}


//*************************************************************************************************************

QDockWidget *DeepCNTK::getControl()
{
    if(!m_pControl) {
        m_pControl = new QDockWidget(tr("Deep CNTK"));
        m_pControl->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        m_pControl->setMinimumWidth(180);
        m_pControl->setWidget(m_pControlPanel);
    }

    return m_pControl;
}


//*************************************************************************************************************

bool DeepCNTK::hasView() const
{
    return true;
}


//*************************************************************************************************************

// check with owner ship and mdi area for garbage collection
QWidget *DeepCNTK::getView()
{
    return m_pDeepViewer;
}


//*************************************************************************************************************

void DeepCNTK::setupModel()
{
    // Create a deep model
    DeviceDescriptor device = DeviceDescriptor::CPUDevice();

    size_t input_dim = 4;
    size_t num_output_classes = 3;

    m_pDeep = Deep::SPtr(new Deep);
    FunctionPtr model = DeepModelCreator::FFN_1(input_dim, num_output_classes, device);
    m_pDeep->setModel(model);
    m_pDeep->print();
}


//*************************************************************************************************************

void DeepCNTK::trainModel()
{
//    QFutureWatcher<bool> trainFutureWatcher;
//    QProgressDialog progressDialog("Train model...", "Cancel Training", 0, 0, m_pDeepViewer, Qt::Dialog);

    //progress.open(m_pDeep.data(),&Deep::cancelTraining);

    //
    // Training
    //

    DeviceDescriptor device = DeviceDescriptor::CPUDevice();

    qDebug() << "\n Start training \n";

    size_t input_dim = m_pDeep->inputDimensions();
    size_t num_output_classes = m_pDeep->outputDimensions();

    qDebug() << "input_dim" << input_dim;
    qDebug() << "num_output_classes" << num_output_classes;

    // Initialize the parameters for the trainer
    int minibatch_size = 25;
    int num_samples = 20000;

    MatrixXf features, labels;

    QVector<double> vecLoss, vecError;
    generateRandomDataSamples(num_samples, static_cast<int>(input_dim), static_cast<int>(num_output_classes), features, labels);


//    progressDialog.setRange(0,num_samples/minibatch_size);

    //Run the training in seperate thread
//    trainFutureWatcher.setFuture(QtConcurrent::run( m_pDeep.data(),
//                                                    &Deep::trainModel,
//                                                    features, labels, vecLoss, vecError, minibatch_size));
    m_pDeep->trainModel(features, labels, vecLoss, vecError, minibatch_size, device);

//    progressDialog.exec();

//    trainFutureWatcher.waitForFinished();

    qDebug() << "\n Finished training \n";

    //Plot error
    LinePlot *error_chartView = new LinePlot(vecError,"Training Error");
    error_chartView->show();

    //Plot loss
    LinePlot *loss_chartView = new LinePlot(vecLoss,"Loss Error");
    loss_chartView->show();

   // Update Deep Viewer
    if(m_pDeepViewer) {
        m_pDeepViewer->updateModel();
    }

}
