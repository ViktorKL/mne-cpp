//=============================================================================================================
/**
* @file     ssvepbci.cpp
* @author   Viktor Klüber <viktor.klueber@tu-ilmenau.de>;
*           Lorenz Esch <lorenz.esch@tu-ilmenau.de>;
*           Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     May, 2016
*
* @section  LICENSE
*
* Copyright (C) 2013, Lorenz Esch, Christoph Dinh and Matti Hamalainen. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that
* the following conditions are met:
*     * Redistributions of source code must retain the above copyright notice, this list of conditions and the
*       following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
*       the following disclaimer in the documentation and/or other materials provided with the distribution.
*     * Neither the name of the Massachusetts General Hospital nor the names of its contributors may be used
*       to endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MASSACHUSETTS GENERAL HOSPITAL BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*
* @brief    Contains the implementation of the BCI class.
*
*/

//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "ssvepbci.h"
#include <iostream>
#include <Eigen/Dense>


//*************************************************************************************************************
//=============================================================================================================
// QT INCLUDES
//=============================================================================================================

#include <QtCore/QtPlugin>
#include <QtCore/QTextStream>
#include <QDebug>
#include <utils/ioutils.h>


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace ssvepBCIPlugin;
using namespace std;
using namespace UTILSLIB;

//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

ssvepBCI::ssvepBCI()
: m_qStringResourcePath(qApp->applicationDirPath()+"/mne_x_plugins/resources/ssvepbci/")
, m_bProcessData(false)
, m_dAlpha(0.25)
, m_iNumberOfHarmonics(2)
, m_bUseMEC(true)
, m_bRemovePowerLine(false)
, m_iPowerLine(50)
, m_bChangeSSVEPParameterFlag(false)
{
    // Create configuration action bar item/button
    m_pActionBCIConfiguration = new QAction(QIcon(":/images/configuration.png"),tr("BCI configuration feature"),this);
    m_pActionBCIConfiguration->setStatusTip(tr("BCI configuration feature"));
    connect(m_pActionBCIConfiguration, &QAction::triggered, this, &ssvepBCI::showBCIConfiguration);
    addPluginAction(m_pActionBCIConfiguration);

    // Create start Stimuli action bar item/button
    m_pActionSetupStimulus = new QAction(QIcon(":/images/stimulus.png"),tr("setup stimulus feature"),this);
    m_pActionSetupStimulus->setStatusTip(tr("Setup stimulus feature"));
    connect(m_pActionSetupStimulus, &QAction::triggered, this, &ssvepBCI::showSetupStimulus);
    addPluginAction(m_pActionSetupStimulus);


    // Intitalise BCI data
    m_slChosenChannelsSensor << "9Z" << "8Z" << "7Z" << "6Z" << "9L" << "8L" << "9R" << "8R"; //<< "TEST";
    m_lElectrodeNumbers << 33 << 34 << 35 << 36 << 40 << 41 << 42 << 43;
    m_lDesFrequencies << 6.66 << 7.5 <<8.57 << 10 << 12;
    m_lThresholdValues << 0.12 << 0.12 << 0.12 << 0.12 << 0.12;

    setFrequencyList(m_lDesFrequencies);
}


//*************************************************************************************************************

ssvepBCI::~ssvepBCI()
{
    //std::cout << "BCI::~BCI() " << std::endl;

    //If the program is closed while the sampling is in process
    if(this->isRunning())
        this->stop();
}


//*************************************************************************************************************

QSharedPointer<IPlugin> ssvepBCI::clone() const
{
    QSharedPointer<ssvepBCI> pSSVEPClone(new ssvepBCI());
    return pSSVEPClone;
}


//*************************************************************************************************************

void ssvepBCI::init()
{
    m_bIsRunning = false;

    // Inputs - Source estimates and sensor level
    m_pRTSEInput = PluginInputData<RealTimeSourceEstimate>::create(this, "BCIInSource", "BCI source input data");
    connect(m_pRTSEInput.data(), &PluginInputConnector::notify, this, &ssvepBCI::updateSource, Qt::DirectConnection);
    m_inputConnectors.append(m_pRTSEInput);

    m_pRTMSAInput = PluginInputData<NewRealTimeMultiSampleArray>::create(this, "BCIInSensor", "SourceLab sensor input data");
    connect(m_pRTMSAInput.data(), &PluginInputConnector::notify, this, &ssvepBCI::updateSensor, Qt::DirectConnection);
    m_inputConnectors.append(m_pRTMSAInput);

//    // Output streams
//    m_pBCIOutputOne = PluginOutputData<NewRealTimeSampleArray>::create(this, "ControlSignal", "BCI output data One");
//    m_pBCIOutputOne->data()->setArraySize(1);
//    m_pBCIOutputOne->data()->setName("Boundary");
//    m_outputConnectors.append(m_pBCIOutputOne);

//    m_pBCIOutputTwo = PluginOutputData<NewRealTimeSampleArray>::create(this, "ControlSignal", "BCI output data Two");
//    m_pBCIOutputTwo->data()->setArraySize(1);
//    m_pBCIOutputTwo->data()->setName("Left electrode var");
//    m_outputConnectors.append(m_pBCIOutputTwo);

//    m_pBCIOutputThree = PluginOutputData<NewRealTimeSampleArray>::create(this, "ControlSignal", "BCI output data Three");
//    m_pBCIOutputThree->data()->setArraySize(1);
//    m_pBCIOutputThree->data()->setName("Right electrode var");
//    m_outputConnectors.append(m_pBCIOutputThree);

//    m_pBCIOutputFour = PluginOutputData<NewRealTimeSampleArray>::create(this, "ControlSignal", "BCI output data Four");
//    m_pBCIOutputFour->data()->setArraySize(1);
//    m_pBCIOutputFour->data()->setName("Left electrode");
//    m_outputConnectors.append(m_pBCIOutputFour);

//    m_pBCIOutputFive = PluginOutputData<NewRealTimeSampleArray>::create(this, "ControlSignal", "BCI output data Five");
//    m_pBCIOutputFive->data()->setArraySize(1);
//    m_pBCIOutputFive->data()->setName("Right electrode");
//    m_outputConnectors.append(m_pBCIOutputFive);

    // Delete Buffer - will be initailzed with first incoming data
    m_pBCIBuffer_Sensor = CircularMatrixBuffer<double>::SPtr();
    m_pBCIBuffer_Source = CircularMatrixBuffer<double>::SPtr();

    // Delete fiff info because the initialisation of the fiff info is seen as the first data acquisition from the input stream
    m_pFiffInfo_Sensor = FiffInfo::SPtr();

    // Intitalise GUI stuff
    m_bUseSensorData = true;

//    // Init BCIFeatureWindow for visualization
//    m_BCIFeatureWindow = QSharedPointer<BCIFeatureWindow>(new BCIFeatureWindow(this));
}


//*************************************************************************************************************

void ssvepBCI::unload()
{

}


//*************************************************************************************************************

bool ssvepBCI::start()
{

    // Init debug output stream
    QString path("BCIDebugFile.txt");
    path.prepend(m_qStringResourcePath);
    m_outStreamDebug.open(path.toStdString(), ios::trunc);

    m_pFiffInfo_Sensor = FiffInfo::SPtr();

    m_iWriteIndex   = 0;
    m_iReadIndex    = 0;
    m_iCounter      = 0;
    m_iReadToWriteBuffer = 0;
    m_iDownSampleIndex   = 0;
    m_iFormerDownSampleIndex = 0;
    m_iWindowSize        = 8;
    m_bIsRunning    = true;

    // starting the thread for data processing
    QThread::start();

    return true;
}


//*************************************************************************************************************

bool ssvepBCI::stop()
{
    m_bIsRunning = false;

    // Get data buffers out of idle state if they froze in the acquire or release function
    //In case the semaphore blocks the thread -> Release the QSemaphore and let it exit from the pop function (acquire statement)

    if(m_bProcessData) // Only clear if buffers have been initialised
    {
        m_pBCIBuffer_Sensor->releaseFromPop();
        m_pBCIBuffer_Sensor->releaseFromPush();
//        m_pBCIBuffer_Source->releaseFromPop();
//        m_pBCIBuffer_Source->releaseFromPush();
    }

    // Stop filling buffers with data from the inputs
    m_bProcessData = false;

    // Delete all features and classification results
    clearClassifications();

    return true;
}


//*************************************************************************************************************

IPlugin::PluginType ssvepBCI::getType() const
{
    return _IAlgorithm;
}


//*************************************************************************************************************

QString ssvepBCI::getName() const
{
    return "SSVEP BCI EEG";
}


//*************************************************************************************************************

QWidget* ssvepBCI::setupWidget()
{
    ssvepBCIWidget* setupWidget = new ssvepBCIWidget(this);//widget is later destroyed by CentralWidget - so it has to be created everytime new

    //init properties dialog
    setupWidget->initGui();

    return setupWidget;
}


//*************************************************************************************************************

void ssvepBCI::updateSensor(XMEASLIB::NewMeasurement::SPtr pMeasurement)
{
    // initialize the sample array which will be filled with raw data
    QSharedPointer<NewRealTimeMultiSampleArray> pRTMSA = pMeasurement.dynamicCast<NewRealTimeMultiSampleArray>();
    if(pRTMSA){
        //Check if buffer initialized
        m_qMutex.lock();
        if(!m_pBCIBuffer_Sensor)
            m_pBCIBuffer_Sensor = CircularMatrixBuffer<double>::SPtr(new CircularMatrixBuffer<double>(64, pRTMSA->getNumChannels(), pRTMSA->getMultiSampleArray()[0].cols()));
    }

    //Fiff information
    if(!m_pFiffInfo_Sensor)
    {
        m_pFiffInfo_Sensor = pRTMSA->info();
        //emit fiffInfoAvailable();

        //calculating downsampling parameter for incoming data
        m_iDownSampleIncrement = m_pFiffInfo_Sensor->sfreq/128;
        m_dSampleFrequency = m_pFiffInfo_Sensor->sfreq/m_iDownSampleIncrement;//m_pFiffInfo_Sensor->sfreq;

        // determine sliding time window parameters
        m_iReadSampleSize = 0.1*m_dSampleFrequency;    // about 0.1 second long time segment as basic read increment
        m_iWriteSampleSize = pRTMSA->getMultiSampleArray()[0].cols();
        m_iTimeWindowLength = int(5*m_dSampleFrequency) + int(pRTMSA->getMultiSampleArray()[0].cols()/m_iDownSampleIncrement) + 1 ;
        //m_iTimeWindowSegmentSize  = int(5*m_dSampleFrequency / m_iWriteSampleSize) + 1;   // 4 seconds long maximal sized window
        m_matSlidingTimeWindow.resize(m_lElectrodeNumbers.size(), m_iTimeWindowLength);//m_matSlidingTimeWindow.resize(rows, m_iTimeWindowSegmentSize*pRTMSA->getMultiSampleArray()[0].cols());

        cout << "Down Sample Increment:" << m_iDownSampleIncrement << endl;
        cout << "Read Sample Size:" << m_iReadSampleSize << endl;
        cout << "Downsampled Frequency:" << m_dSampleFrequency << endl;
        cout << "Write Sample SIze :" << m_iWriteSampleSize<< endl;
        cout << "Length of the time window:" << m_iTimeWindowLength << endl;
    }
    m_qMutex.unlock();

    // filling the matrix buffer
    if(m_bProcessData){
        MatrixXd t_mat;
        for(qint32 i = 0; i < pRTMSA->getMultiArraySize(); ++i){
            t_mat = pRTMSA->getMultiSampleArray()[i];
            m_pBCIBuffer_Sensor->push(&t_mat);
        }
    }
}


//*************************************************************************************************************

void ssvepBCI::updateSource(XMEASLIB::NewMeasurement::SPtr pMeasurement)
{
    cout<<"update source"<<endl;
    QSharedPointer<RealTimeSourceEstimate> pRTSE = pMeasurement.dynamicCast<RealTimeSourceEstimate>();
    if(pRTSE)
    {
        //Check if buffer initialized
        if(!m_pBCIBuffer_Source)
            m_pBCIBuffer_Source = CircularMatrixBuffer<double>::SPtr(new CircularMatrixBuffer<double>(64,  pRTSE->getValue()->data.rows(), pRTSE->getValue()->data.cols()));

        if(m_bProcessData)
        {
            MatrixXd t_mat(pRTSE->getValue()->data.rows(), pRTSE->getValue()->data.cols());

            for(unsigned char i = 0; i < pRTSE->getValue()->data.cols(); ++i)
                t_mat.col(i) = pRTSE->getValue()->data.col(i);

            m_pBCIBuffer_Source->push(&t_mat);
        }
    }
}


//*************************************************************************************************************

void ssvepBCI::clearClassifications()
{
    m_qMutex.lock();
        m_lIndexOfClassResultSensor.clear();
    m_qMutex.unlock();
}


//*************************************************************************************************************

void  ssvepBCI::setChangeSSVEPParameterFlag(){
    m_bChangeSSVEPParameterFlag = true;
}

//*************************************************************************************************************

bool ssvepBCI::lookForTrigger(const MatrixXd &data)
{
    // Check if capacitive touch trigger signal was received - Note that there can also be "beep" triggers in the received data, which are only 1 sample wide -> therefore look for 2 samples with a value of 254 each
    for(int i = 0; i<data.cols()-1; i++)
    {
        if(data(0,i) == 254 && data(0,i+1) == 254) // data - corresponds with channel 136 which is the trigger channel
            return true;
    }

    return false;
}


//*************************************************************************************************************

void ssvepBCI::showSetupStimulus()
{
    QDesktopWidget Desktop; // Desktop Widget for getting the number of accessible screens

    if(Desktop.numScreens()> 1){
        // Open setup stimulus widget
        if(m_pssvepBCISetupStimulusWidget == NULL)
            m_pssvepBCISetupStimulusWidget = QSharedPointer<ssvepBCISetupStimulusWidget>(new ssvepBCISetupStimulusWidget(this));

        if(!m_pssvepBCISetupStimulusWidget->isVisible()){

            m_pssvepBCISetupStimulusWidget->setWindowTitle("ssvepBCI - Setup Stimulus");
            //m_pssvepBCISetupStimulusWidget->initGui();
            m_pssvepBCISetupStimulusWidget->show();
            m_pssvepBCISetupStimulusWidget->raise();
        }

        //sets Window to the foreground and activates it for editing
        m_pssvepBCISetupStimulusWidget->activateWindow();
    }
    else{
        QMessageBox msgBox;
        msgBox.setText("Only one screen detected!\nFor stimulus visualization attach one more.");
        msgBox.exec();
        return;
    }

}


//*************************************************************************************************************

void ssvepBCI::showBCIConfiguration()
{
    // Open setup stimulus widget
    if(m_pssvepBCIConfigurationWidget == NULL)
        m_pssvepBCIConfigurationWidget = QSharedPointer<ssvepBCIConfigurationWidget>(new ssvepBCIConfigurationWidget(this));

    if(!m_pssvepBCIConfigurationWidget->isVisible()){

        m_pssvepBCIConfigurationWidget->setWindowTitle("ssvepBCI - Configuration");
        //m_pEEGoSportsSetupStimulusWidget->initGui();
        m_pssvepBCIConfigurationWidget->show();
        m_pssvepBCIConfigurationWidget->raise();
    }

    //sets Window to the foreground and activates it for editing
    m_pssvepBCIConfigurationWidget->activateWindow();
}


//*************************************************************************************************************

void ssvepBCI::removePowerLine(bool removePowerLine){
    m_qMutex.lock();
    m_bRemovePowerLine = removePowerLine;
    m_qMutex.unlock();
}


//*************************************************************************************************************

void ssvepBCI::setPowerLine(int powerLine){
    m_qMutex.lock();
    m_iPowerLine = powerLine;
    m_qMutex.unlock();
}


//*************************************************************************************************************

void ssvepBCI::setFeatureExtractionMethod(bool useMEC){
    m_qMutex.lock();
    m_bUseMEC = useMEC;
    m_qMutex.unlock();
}


//*************************************************************************************************************

void ssvepBCI::changeSSVEPParameter(){

    // update frequency list from setup stimulus widget if activated
    if(m_pssvepBCISetupStimulusWidget)
        setFrequencyList(m_pssvepBCISetupStimulusWidget->getFrequencies());

    if(m_pssvepBCIConfigurationWidget){

        // update number of harmonics of reference signal
        m_iNumberOfHarmonics = 1 + m_pssvepBCIConfigurationWidget->getNumOfHarmonics();

        // update channel select
        QStringList channelSelectSensor =  m_pssvepBCIConfigurationWidget->getSensorChannelSelection();
        if(channelSelectSensor.size() > 0){

            // update the list of selected channels
            m_slChosenChannelsSensor = channelSelectSensor;

            // get new list of electrode numbers
            m_lElectrodeNumbers.clear();
            foreach(const QString &str, m_slChosenChannelsSensor)
                m_lElectrodeNumbers << m_mapElectrodePinningScheme.value(str);

            // reset sliding time window parameter
            m_iWriteIndex   = 0;
            m_iReadIndex    = 0;
            m_iCounter      = 0;
            m_iReadToWriteBuffer = 0;
            m_iDownSampleIndex   = 0;
            m_iFormerDownSampleIndex = 0;

            // resize the time window with new electrode numbers
            m_matSlidingTimeWindow.resize(m_lElectrodeNumbers.size(), m_iTimeWindowLength);
        }
    }

    // reset flag for changing SSVEP parameter
    m_bChangeSSVEPParameterFlag = false;
}


//*************************************************************************************************************

void ssvepBCI::setThresholdValues(MyQList thresholds){
    m_lThresholdValues = thresholds;
}


//*************************************************************************************************************

void ssvepBCI::run(){
    while(m_bIsRunning){

        if(m_bUseSensorData)
            ssvepBCIOnSensor();
        else
            ssvepBCIOnSource();
    }
}


//*************************************************************************************************************

void ssvepBCI::setFrequencyList(QList<double> frequencyList)
{
    if(!frequencyList.isEmpty()){

        // update list of desired frequencies
        m_lDesFrequencies.clear();
        m_lDesFrequencies = frequencyList;

        // update the list of all frequencies
        m_lAllFrequencies.clear();
        m_lAllFrequencies = m_lDesFrequencies;
        for(int i = 0; i < m_lDesFrequencies.size() - 1; i++)
            m_lAllFrequencies.append((m_lDesFrequencies.at(i) + m_lDesFrequencies.at(i + 1) ) / 2);

        // emit novel frequency list
        emit getFrequencyLabels(m_lDesFrequencies);
    }
}


//*************************************************************************************************************

QList<double> ssvepBCI::getCurrentListOfFrequencies(){
    return m_lDesFrequencies;
}


//*************************************************************************************************************

double ssvepBCI::MEC(MatrixXd &Y, MatrixXd &X)
{

    // Remove SSVEP harmonic frequencies
    MatrixXd X_help = X.transpose()*X;
    MatrixXd Ytilde = Y - X*X_help.inverse()*X.transpose()*Y;

    // Find eigenvalues and eigenvectors
    SelfAdjointEigenSolver<MatrixXd> eigensolver(Ytilde.transpose()*Ytilde);    

    // Determine number of channels Ns
    int Ns;
    VectorXd cumsum = eigensolver.eigenvalues();
    for(int j = 1; j < eigensolver.eigenvalues().size(); j++)
        cumsum(j) += cumsum(j - 1);
    for(Ns = 0; Ns < eigensolver.eigenvalues().size() ; Ns++)
        if(cumsum(Ns)/eigensolver.eigenvalues().sum() > 0.1)
            break;
    Ns += 1;

    // Determine spatial filter matrix W
    MatrixXd W = eigensolver.eigenvectors().block(0, 0, eigensolver.eigenvectors().rows(), Ns);   
    for(int k = 0; k < Ns; k++)
        W.col(k) = W.col(k)*(1/sqrt(eigensolver.eigenvalues()(k)));
    // Calcuclate channel signals
    MatrixXd S = Y*W;

    // Calculate signal energy
    MatrixXd P(2, Ns);
    double power = 0;
    for(int k = 0; k < m_iNumberOfHarmonics; k++){
        P = X.block(0, 2*k, X.rows(), 2).transpose()*S;
        P = P.array()*P.array();
        power += 1 / double(m_iNumberOfHarmonics*Ns) * P.sum();
    }

    //cout << "power" << endl << power << endl<< endl;
    return power;
}


//*************************************************************************************************************

double ssvepBCI::CCA(MatrixXd &Y, MatrixXd &X)
{
    // CCA parameter
    int n  = X.rows();
    int p1 = X.cols();
    int p2 = Y.cols();

    // center data sets
    MatrixXd X_center(n, p1);
    MatrixXd Y_center(n, p2);
    for(int i = 0; i < p1; i++)
        X_center.col(i) = X.col(i).array() - X.col(i).mean();
    for(int i = 0; i < p2; i++)
        Y_center.col(i) = Y.col(i).array() - Y.col(i).mean();

    // QR decomposition
    MatrixXd Q1, Q2;
    ColPivHouseholderQR<MatrixXd> qr1(X_center), qr2(Y_center);
    Q1 = qr1.householderQ() * MatrixXd::Identity(n, p1);
    Q2 = qr2.householderQ() * MatrixXd::Identity(n, p2);

    // SVD decomposition, determine max correlation
    JacobiSVD<MatrixXd> svd(Q1.transpose()*Q2); // ComputeThinU | ComputeThinV

    return svd.singularValues().maxCoeff();
}


//*************************************************************************************************************

void ssvepBCI::readFromSlidingTimeWindow(MatrixXd &data)
{
    data.resize(m_matSlidingTimeWindow.rows(), m_iWindowSize*m_iReadSampleSize);
    // consider matrix overflow case
    if(data.cols() > m_iReadIndex + 1){
        //cout << "do splitting" << endl;
        int width = data.cols() - (m_iReadIndex + 1);
        data.block(0, 0, data.rows(), width) = m_matSlidingTimeWindow.block(0, m_matSlidingTimeWindow.cols() - width , data.rows(), width );
        data.block(0, width, data.rows(), m_iReadIndex + 1) = m_matSlidingTimeWindow.block(0, 0, data.rows(), m_iReadIndex + 1);
    }
    else
        data = m_matSlidingTimeWindow.block(0, m_iReadIndex - (data.cols() - 1), data.rows(), data.cols());  // consider case without matrix overflow

    // transpose in the same data space and avoiding aliasing
    data.transposeInPlace();
}


//*************************************************************************************************************

void ssvepBCI::ssvepBCIOnSensor()
{

    // Wait for fiff Info if not yet received - this is needed because we have to wait until the buffers are firstly initiated in the update functions
    while(!m_pFiffInfo_Sensor)
        msleep(10);

    // reset list of classifiaction results
    m_lIndexOfClassResultSensor.clear();
    MatrixXd m_matSSVEPProbabilities(m_lDesFrequencies.size(), 0);

    // Start filling buffers with data from the inputs
    m_bProcessData = true;
    MatrixXd t_mat = m_pBCIBuffer_Sensor->pop();

    // writing selected feature channels to the time window storage and increase the segment index
    int   writtenSamples = 0;
    while(m_iDownSampleIndex >= m_iFormerDownSampleIndex){

        // write from t_mat to the sliding time window while doing channel select and downsampling
        m_iFormerDownSampleIndex = m_iDownSampleIndex;
        for(int i = 0; i < m_lElectrodeNumbers.size(); i++)
            m_matSlidingTimeWindow(i, m_iWriteIndex) = t_mat(m_lElectrodeNumbers.at(i), m_iDownSampleIndex);
        writtenSamples++;

        // update counter variables
        m_iWriteIndex = (m_iWriteIndex + 1) % m_iTimeWindowLength;
        m_iDownSampleIndex = (m_iDownSampleIndex + m_iDownSampleIncrement ) % m_iWriteSampleSize;

    }
    m_iFormerDownSampleIndex = m_iDownSampleIndex;

    // calculate buffer between read- and write index
    m_iReadToWriteBuffer = m_iReadToWriteBuffer + writtenSamples;

    // execute processing loop as long as there is new data to be red from the time window
    while(m_iReadToWriteBuffer >= m_iReadSampleSize)
    {
        if(m_iCounter > 8)
        {
            // determine window size according to former counted miss classifications
            m_iWindowSize = 8;
            if(m_iCounter <= 44 && m_iCounter > 24)
                m_iWindowSize = 20;
            if(m_iCounter > 44)
                m_iWindowSize = 40;
            //cout << "Counter:" << m_iCounter << endl;

            // create current data matrix Y
            MatrixXd Y;
            readFromSlidingTimeWindow(Y);
            //cout << "Y:" << Y <<  endl;

            // create realtive timeline according to Y
            int samples = Y.rows();
            ArrayXd t = 2*M_PI/m_dSampleFrequency * ArrayXd::LinSpaced(samples, 1, samples);
            
            //IOUtils::write_eigen_matrix(Y, "Y_before.txt");
            // Remove 50 Hz Power line signal
            if(m_bRemovePowerLine){
                MatrixXd Zp(samples,2);
                ArrayXd t_PL = t*m_iPowerLine;
                Zp.col(0) = t_PL.sin();
                Zp.col(1) = t_PL.cos();
                MatrixXd Zp_help = Zp.transpose()*Zp;
                Y = Y - Zp*Zp_help.inverse()*Zp.transpose()*Y;
            }

            // apply feature extraction for all frequencies of interest
            VectorXd ssvepProbabilities(m_lAllFrequencies.size());
            for(int i = 0; i < m_lAllFrequencies.size(); i++)
            {
                // create reference signal matrix X
                MatrixXd X(samples, 2*m_iNumberOfHarmonics);
                for(int k = 0; k < m_iNumberOfHarmonics; k++){
                    ArrayXd t_k = t*(k+1)*m_lAllFrequencies.at(i);
                    X.col(2*k)      = t_k.sin();
                    X.col(2*k+1)    = t_k.cos();
                }

                // extracting the features from the data Y with the reference signal X
                if(m_bUseMEC)
                    ssvepProbabilities(i) = MEC(Y, X); // using Minimum Energy Combination as feature-extraction tool
                else
                    ssvepProbabilities(i) = CCA(Y, X); // using Canonical Correlation Analysis as feature-extraction tool
            }

            // normalize features to probabilities and transfering it into a softmax function
            ssvepProbabilities = m_dAlpha / ssvepProbabilities.sum() * ssvepProbabilities;
            ssvepProbabilities = ssvepProbabilities.array().exp();                          // softmax function for better distinguishability between the probabilities
            ssvepProbabilities = 1 / ssvepProbabilities.sum() * ssvepProbabilities;
            //cout << "probabilites:" << endl << ssvepProbabilities << endl;

//            // transfer values to MyQList and emit signal for GUI
//            m_lSSVEPProbabilities.clear();
//            for(int i = 0; i < m_lDesFrequencies.size(); i++)
//                m_lSSVEPProbabilities << ssvepProbabilities(i);
//            if(!m_lSSVEPProbabilities.isEmpty())
//                emit SSVEPprob(m_lSSVEPProbabilities);

            // classify probabilites
            int index = 0;
            double maxProbability = ssvepProbabilities.maxCoeff(&index);
            //cout << "max Probability:" << maxProbability << endl;
            if(index < m_lDesFrequencies.size()){
                if(m_lThresholdValues.at(index) < maxProbability){
                    m_lIndexOfClassResultSensor.append(index+1);
                    m_iCounter = -1;
                }
            }
            else
                m_lIndexOfClassResultSensor.append(0);

            // transfer values to matrix containing all SSVEPProabibilities of desired frequencies of one calculationstep
            m_matSSVEPProbabilities.conservativeResize(m_lDesFrequencies.size(), m_matSSVEPProbabilities.cols() + 1);
            m_matSSVEPProbabilities.col( m_matSSVEPProbabilities.cols() - 1) = ssvepProbabilities.head(m_lDesFrequencies.size());

        }

        // update counter and index variables
        m_iCounter++;
        m_iReadToWriteBuffer = m_iReadToWriteBuffer - m_iReadSampleSize;
        m_iReadIndex = (m_iReadIndex + m_iReadSampleSize) % (m_iTimeWindowLength);

    }

    // emit classifiaction results if any classifiaction has been done
    if(!m_lIndexOfClassResultSensor.isEmpty()){

        m_lIndexOfClassResultSensor.removeAll(0);
        if(!m_lIndexOfClassResultSensor.isEmpty())
            emit classificationResult(m_lDesFrequencies[m_lIndexOfClassResultSensor.last() - 1]);
        else
            emit classificationResult(0);
    }

    // calculate and emit signal of mean probabilities
    if(m_matSSVEPProbabilities.cols() != 0){
        QList<double> meanSSVEPProbabilities;
        for(int i = 0; i < m_lDesFrequencies.size(); i++)
            meanSSVEPProbabilities << m_matSSVEPProbabilities.row(i).mean();
        emit SSVEPprob(meanSSVEPProbabilities);
    }

    // change parameter and reset the time window if the change flag has been set
    if(m_bChangeSSVEPParameterFlag)
        changeSSVEPParameter();

}


//*************************************************************************************************************

void ssvepBCI::ssvepBCIOnSource(){


}