//=============================================================================================================
/**
* @file     bemsurfacetreeitem.cpp
* @author   Lorenz Esch <Lorenz.Esch@tu-ilmenau.de>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     May, 2016
*
* @section  LICENSE
*
* Copyright (C) 2016, Lorenz Esch and Matti Hamalainen. All rights reserved.
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
* @brief    BemSurfaceTreeItem class definition.
*
*/

//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "bemsurfacetreeitem.h"
#include "../common/metatreeitem.h"
#include "../../3dhelpers/renderable3Dentity.h"
#include "../../materials/pervertexphongalphamaterial.h"

#include <mne/mne_bem.h>


//*************************************************************************************************************
//=============================================================================================================
// QT INCLUDES
//=============================================================================================================


//*************************************************************************************************************
//=============================================================================================================
// Eigen INCLUDES
//=============================================================================================================


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace DISP3DLIB;
using namespace Eigen;
using namespace MNELIB;


//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

BemSurfaceTreeItem::BemSurfaceTreeItem(int iType, const QString& text)
: AbstractSurfaceTreeItem(iType, text)
{
    initItem();
}


//*************************************************************************************************************

void BemSurfaceTreeItem::initItem()
{
    this->setEditable(false);
    this->setCheckable(true);
    this->setCheckState(Qt::Checked);
    this->setToolTip("BEM surface item");
}


//*************************************************************************************************************

void BemSurfaceTreeItem::addData(const MNEBemSurface& tBemSurface, Qt3DCore::QEntity* parent)
{
    //Set parents
    m_pRenderable3DEntity->setParent(parent);
    m_pRenderable3DEntityNormals->setParent(parent);

    QPointer<Qt3DRender::QMaterial> pMaterial = new PerVertexPhongAlphaMaterial(true);
    this->setMaterial(pMaterial);

    //Create color from curvature information with default gyri and sulcus colors
    MatrixX3f matVertColor = createVertColor(tBemSurface.rr);

    //Set renderable 3D entity mesh and color data
    m_pRenderable3DEntity->getCustomMesh()->setMeshData(tBemSurface.rr,
                                                        tBemSurface.nn,
                                                        tBemSurface.tris,
                                                        matVertColor,
                                                        Qt3DRender::QGeometryRenderer::Triangles);

    //Render normals
    if(m_bRenderNormals) {
        m_pRenderable3DEntityNormals->getCustomMesh()->setMeshData(tBemSurface.rr,
                                                                      tBemSurface.nn,
                                                                      tBemSurface.tris,
                                                                      matVertColor,
                                                                      Qt3DRender::QGeometryRenderer::Triangles);
    }

    //Find out BEM layer type and change items name
    this->setText(MNEBemSurface::id_name(tBemSurface.id));

    //Add data which is held by this BemSurfaceTreeItem
    QVariant data;

    data.setValue(tBemSurface.rr);
    this->setData(data, Data3DTreeModelItemRoles::SurfaceVert);
}

