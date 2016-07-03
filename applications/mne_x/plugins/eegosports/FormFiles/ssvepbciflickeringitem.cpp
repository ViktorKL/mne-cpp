//=============================================================================================================
/**
* @file     ssvepBCIFlickeringItem.cpp
* @author   Viktor Klüber <viktor.klueber@tu-ilmenauz.de>;
*           Lorenz Esch <Lorenz.Esch@tu-ilmenau.de>;
*           Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     May 2016
*
* @section  LICENSE
*
* Copyright (C) 2014, Lorenz Esch, Christoph Dinh and Matti Hamalainen. All rights reserved.
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
* @brief    Contains the implementation of the ssvepBCIFlickeringItem class.
*
*/

//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "ssvepbciflickeringitem.h"

//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace EEGoSportsPlugin;


//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

ssvepBCIFlickeringItem::ssvepBCIFlickeringItem()
    : m_dPosX(0)
    , m_dPosY(0)
    , m_dWidth(0.4)
    , m_dHeight(0.4)
    , m_bFlickerState(true)
    , m_bIter(m_bRenderOrder)
    , m_bChessboard(false)
    , m_iRows(5)

{
    m_bRenderOrder << 0 << 0 << 1 << 1; //default
}

//*************************************************************************************************************

ssvepBCIFlickeringItem::~ssvepBCIFlickeringItem(){
}

//*************************************************************************************************************

void ssvepBCIFlickeringItem::setPos(double x, double y){
        m_dPosX = x;
        m_dPosY = y;
}

//*************************************************************************************************************

void ssvepBCIFlickeringItem::setDim(double w, double h){
    m_dWidth    = w;
    m_dHeight   = h;
}

//*************************************************************************************************************

void ssvepBCIFlickeringItem::setRenderOrder(QList<bool> renderOrder, int freqKey){

    //clear the old rendering order list
    m_bRenderOrder.clear();

    //setup the iterator and assign it to the new list
    m_bRenderOrder  = renderOrder;
    m_bIter         = m_bRenderOrder;
    m_iFreqKey      = freqKey;
}

//*************************************************************************************************************

void ssvepBCIFlickeringItem::paint(QPaintDevice *paintDevice)
{
    //setting the nex flicker state (moving iterater to front if necessary)
    if(!m_bIter.hasNext())
        m_bIter.toFront();
    m_bFlickerState = m_bIter.next();


    //painting the itme's shape
    QPainter p(paintDevice);
    if(!m_bChessboard)
        if(m_bFlickerState)
            p.fillRect(m_dPosX*paintDevice->width(),m_dPosY*paintDevice->height(),m_dWidth*paintDevice->width(),m_dHeight*paintDevice->height(),Qt::white);
        else
            p.fillRect(m_dPosX*paintDevice->width(),m_dPosY*paintDevice->height(),m_dWidth*paintDevice->width(),m_dHeight*paintDevice->height(),Qt::black);
    else{
        double _dChessDim = paintDevice->height() / m_iRows;
        int _iClm = paintDevice->width() / _dChessDim + 2;
        bool _bPatternRow = true;

        for(int i = 0; i < m_iRows; i++){

            bool _bPattern = _bPatternRow;
            for(int j = 0; j < _iClm; j++){
                if(m_bFlickerState)
                    if(_bPattern)
                        p.fillRect(j*_dChessDim,i*_dChessDim,_dChessDim,_dChessDim,Qt::white);
                    else
                        p.fillRect(j*_dChessDim,i*_dChessDim,_dChessDim,_dChessDim,Qt::black);
                else
                    if(_bPattern)
                        p.fillRect(j*_dChessDim,i*_dChessDim,_dChessDim,_dChessDim,Qt::black);
                    else
                        p.fillRect(j*_dChessDim,i*_dChessDim,_dChessDim,_dChessDim,Qt::white);

                _bPattern = !_bPattern;
            }

        _bPatternRow = !_bPatternRow;
        }





    }



}

//*************************************************************************************************************

void ssvepBCIFlickeringItem::chessDim(int rows)
{
    m_iRows = rows;
    m_bChessboard = true;



}

//*************************************************************************************************************

int ssvepBCIFlickeringItem::getFreq()
{
    return m_iFreqKey;
}