//=============================================================================================================
/**
* @file     mne_proj_op.h
* @author   Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     January, 2017
*
* @section  LICENSE
*
* Copyright (C) 2017, Christoph Dinh and Matti Hamalainen. All rights reserved.
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
* @brief    MNEProjOp class declaration.
*
*/

#ifndef MNEPROJOP_H
#define MNEPROJOP_H

//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "../inverse_global.h"


//*************************************************************************************************************
//=============================================================================================================
// Eigen INCLUDES
//=============================================================================================================

#include <Eigen/Core>


//*************************************************************************************************************
//=============================================================================================================
// Qt INCLUDES
//=============================================================================================================

#include <QSharedPointer>
#include <QList>


//*************************************************************************************************************
//=============================================================================================================
// DEFINE NAMESPACE INVERSELIB
//=============================================================================================================

namespace INVERSELIB
{

//*************************************************************************************************************
//=============================================================================================================
// FORWARD DECLARATIONS
//=============================================================================================================

class MneProjItem;


//=============================================================================================================
/**
* Implements an MNE Projection Operator (Replaces *mneProjOp,mneProjOpRec; struct of MNE-C mne_types.h).
*
* @brief One linear projection item
*/
class INVERSESHARED_EXPORT MneProjOp
{
public:
    typedef QSharedPointer<MneProjOp> SPtr;              /**< Shared pointer type for MneProjOp. */
    typedef QSharedPointer<const MneProjOp> ConstSPtr;   /**< Const shared pointer type for MneProjOp. */

    //=========================================================================================================
    /**
    * Constructs the MNE Projection Operator
    * Refactored: mne_new_proj_op (mne_lin_proj.c)
    */
    MneProjOp();

    //=========================================================================================================
    /**
    * Destroys the MNE Projection Operator
    * Refactored: mne_free_proj_op (mne_lin_proj.c)
    */
    ~MneProjOp();

    //=========================================================================================================
    /**
    * Free Substructure; TODO: Remove later on
    * Refactored: ne_free_proj_op_proj (mne_lin_proj.c)
    */
    static void mne_free_proj_op_proj(MneProjOp* op);

public:
    QList<INVERSELIB::MneProjItem*> items;  /* The projection items */
    int     nitems;                 /* Number of items */
    char    **names;                /* Names of the channels in the final projector */
    int     nch;                    /* Number of channels in the final projector */
    int     nvec;                   /* Number of vectors in the final projector */
    float   **proj_data;            /* The orthogonalized projection vectors picked and orthogonalized from the original data */

//// ### OLD STRUCT ###
//typedef struct {                            /* Collection of projection items and the projector itself */
//    QList<INVERSELIB::MneProjItem*> items;  /* The projection items */
//    int            nitems;                  /* Number of items */
//    char           **names;                 /* Names of the channels in the final projector */
//    int            nch;                     /* Number of channels in the final projector */
//    int            nvec;                    /* Number of vectors in the final projector */
//    float          **proj_data;             /* The orthogonalized projection vectors picked and orthogonalized from the original data */
//} *mneProjOp,mneProjOpRec;
};

//*************************************************************************************************************
//=============================================================================================================
// INLINE DEFINITIONS
//=============================================================================================================

} // NAMESPACE INVERSELIB

#endif // MNEPROJOP_H
