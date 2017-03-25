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

#include <disp/lineplot.h>
#include <disp/deepmodelviewer/deepviewerwidget.h>
#include <disp/deepmodelviewer/controls.h>

#include <iostream>
#include <random>


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace DEEPCNTKEXTENSION;
using namespace ANSHAREDLIB;
using namespace Eigen;
using namespace DEEPLIB;
using namespace DISPLIB;
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
, m_pView(Q_NULLPTR)
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
    // Create a deep model
    DeviceDescriptor device = DeviceDescriptor::CPUDevice();
    size_t input_dim = 4;
    size_t num_output_classes = 3;
    m_pDeep = Deep::SPtr(new Deep);
    m_pModel = DeepModelCreator::FFN_1(input_dim, num_output_classes, device);
    m_pDeep->setModel(m_pModel);
    m_pDeep->print();

    //
    // Training
    //
    qDebug() << "\n Start training \n";

    // Initialize the parameters for the trainer
    int minibatch_size = 25;
    int num_samples = 20000;

    MatrixXf features, labels;

    QVector<double> vecLoss, vecError;
    generateRandomDataSamples(num_samples, static_cast<int>(input_dim), static_cast<int>(num_output_classes), features, labels);
    m_pDeep->trainModel(features, labels, vecLoss, vecError, minibatch_size, device);

    //
    // Init view
    //
    if(!m_pControlPanel) {
        m_pControlPanel = new Controls();
    }

    //
    // Create the viewer
    //
    if(!m_pView) {
        m_pView = new DeepViewerWidget(m_pModel, m_pControlPanel);
        m_pView->setWindowTitle("Deep CNTK");
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
    if(!m_pView) {
        //
        // Create the viewer
        //
        m_pView = new DeepViewerWidget(m_pModel);
        m_pView->setWindowTitle("Deep CNTK");
    }

    return m_pView;
}
