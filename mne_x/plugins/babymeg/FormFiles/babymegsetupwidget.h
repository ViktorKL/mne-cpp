//=============================================================================================================
/**
* @file     babymegsetupwidget.h
* @author   Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     February, 2013
*
* @section  LICENSE
*
* Copyright (C) 2013, Christoph Dinh and Matti Hamalainen. All rights reserved.
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
* @brief    Contains the declaration of the BabyMegSetupWidget class.
*
*/

#ifndef BABYMEGSETUPWIDGET_H
#define BABYMEGSETUPWIDGET_H


//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "../ui_babymegsetup.h"


//*************************************************************************************************************
//=============================================================================================================
// QT INCLUDES
//=============================================================================================================

#include <QtWidgets>


//*************************************************************************************************************
//=============================================================================================================
// DEFINE NAMESPACE BabyMegPlugin
//=============================================================================================================

namespace BabyMegPlugin
{


//*************************************************************************************************************
//=============================================================================================================
// FORWARD DECLARATIONS
//=============================================================================================================

class BabyMeg;


//=============================================================================================================
/**
* DECLARE CLASS BabyMegSetupWidget
*
* @brief The BabyMegSetupWidget class provides the ECG configuration window.
*/
class BabyMegSetupWidget : public QWidget
{
    Q_OBJECT

public:

    //=========================================================================================================
    /**
    * Constructs a BabyMegSetupWidget which is a child of parent.
    *
    * @param [in] p_pBabyMeg   a pointer to the corresponding BabyMeg.
    * @param [in] parent        pointer to parent widget; If parent is 0, the new BabyMegSetupWidget becomes a window. If parent is another widget, BabyMegSetupWidget becomes a child window inside parent. BabyMegSetupWidget is deleted when its parent is deleted.
    */
    BabyMegSetupWidget(BabyMeg* p_pBabyMeg, QWidget *parent = 0);

    //=========================================================================================================
    /**
    * Destroys the BabyMegSetupWidget.
    * All BabyMegSetupWidget's children are deleted first. The application exits if BabyMegSetupWidget is the main widget.
    */
    ~BabyMegSetupWidget();

    //=========================================================================================================
    /**
    * Inits the setup widget
    */
    void init();

//slots
    void bufferSizeEdited();        /**< Buffer size edited and set new buffer size.*/

    void checkedRecordDataChanged();    /**< Record Data checkbox changed. */

    //=========================================================================================================
    /**
    * Connector selection index changed
    *
    * @param [in] idx   new connector combo box index
    */
    void connectorIdxChanged(int idx);

    void printToLog(QString message);   /**< Implements printing messages to rtproc log.*/

    void pressedFiffRecordFile();   /**< Triggers file dialog to select record file.*/

    void pressedConnect();          /**< Triggers a connection trial to rt_server.*/

    void pressedSendCLI();          /**< Triggers a send request of a cli command.*/

    void fiffInfoReceived();        /**< Triggered when new fiff info is recieved by producer and stored intor rt_server */


private:
    //=========================================================================================================
    /**
    * Set command connection status
    *
    * @param[in] p_bConnectionStatus    the connection status
    */
    void cmdConnectionChanged(bool p_bConnectionStatus);

    //=========================================================================================================
    /**
    * Shows the About Dialog
    *
    */
    void showAboutDialog();

    BabyMeg*   m_pBabyMeg;    /**< Holds a pointer to corresponding ECGSimulator.*/

    Ui::BabyMegSetupWidgetClass ui;       /**< Holds the user interface for the BabyMegSetupWidget.*/

    bool m_bIsInit;     /**< false when gui is not initialized jet. Prevents gui from already interacting when not initialized */


};

} // NAMESPACE

#endif // BABYMEGSETUPWIDGET_H
