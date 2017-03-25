//=============================================================================================================
/**
* @file     mne_surface_or_volume.cpp
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
* @brief    Implementation of the MNE Surface or Volume (MneSurfaceOrVolume) Class.
*
*/


//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "mne_surface_or_volume.h"
#include "mne_surface_old.h"
#include "mne_source_space_old.h"
#include "mne_patch_info.h"
//#include "fwd_bem_model.h"
#include <mne/c/mne_triangle.h>
#include "mne_nearest.h"
#include "filter_thread_arg.h"

#include <fiff/fiff_stream.h>


#include <QFile>
#include <QCoreApplication>
#include <QtConcurrent>

#define _USE_MATH_DEFINES
#include <math.h>

#include <Eigen/Dense>
#include <Eigen/Sparse>


//ToDo don't use access and unlink -> use QT stuff instead
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include <io.h>
#else
#include <unistd.h>
#endif



//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace Eigen;
using namespace FIFFLIB;
using namespace MNELIB;


//============================= dot.h =============================

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef FAIL
#define FAIL -1
#endif

#ifndef OK
#define OK 0
#endif



#define X_17 0
#define Y_17 1
#define Z_17 2


#define VEC_DOT_17(x,y) ((x)[X_17]*(y)[X_17] + (x)[Y_17]*(y)[Y_17] + (x)[Z_17]*(y)[Z_17])
#define VEC_LEN_17(x) sqrt(VEC_DOT_17(x,x))


#define VEC_DIFF_17(from,to,diff) {\
    (diff)[X_17] = (to)[X_17] - (from)[X_17];\
    (diff)[Y_17] = (to)[Y_17] - (from)[Y_17];\
    (diff)[Z_17] = (to)[Z_17] - (from)[Z_17];\
    }



#define VEC_COPY_17(to,from) {\
    (to)[X_17] = (from)[X_17];\
    (to)[Y_17] = (from)[Y_17];\
    (to)[Z_17] = (from)[Z_17];\
    }


#define CROSS_PRODUCT_17(x,y,xy) {\
    (xy)[X_17] =   (x)[Y_17]*(y)[Z_17]-(y)[Y_17]*(x)[Z_17];\
    (xy)[Y_17] = -((x)[X_17]*(y)[Z_17]-(y)[X_17]*(x)[Z_17]);\
    (xy)[Z_17] =   (x)[X_17]*(y)[Y_17]-(y)[X_17]*(x)[Y_17];\
    }


#define MALLOC_17(x,t) (t *)malloc((x)*sizeof(t))

#define REALLOC_17(x,y,t) (t *)((x == NULL) ? malloc((y)*sizeof(t)) : realloc((x),(y)*sizeof(t)))

#define ALLOC_INT_17(x) MALLOC_17(x,int)

#define ALLOC_CMATRIX_17(x,y) mne_cmatrix_17((x),(y))


static void matrix_error_17(int kind, int nr, int nc)

{
    if (kind == 1)
        printf("Failed to allocate memory pointers for a %d x %d matrix\n",nr,nc);
    else if (kind == 2)
        printf("Failed to allocate memory for a %d x %d matrix\n",nr,nc);
    else
        printf("Allocation error for a %d x %d matrix\n",nr,nc);
    if (sizeof(void *) == 4) {
        printf("This is probably because you seem to be using a computer with 32-bit architecture.\n");
        printf("Please consider moving to a 64-bit platform.");
    }
    printf("Cannot continue. Sorry.\n");
    exit(1);
}

float **mne_cmatrix_17(int nr,int nc)

{
    int i;
    float **m;
    float *whole;

    m = MALLOC_17(nr,float *);
    if (!m) matrix_error_17(1,nr,nc);
    whole = MALLOC_17(nr*nc,float);
    if (!whole) matrix_error_17(2,nr,nc);

    for(i=0;i<nr;i++)
        m[i] = whole + i*nc;
    return m;
}

#define FREE_17(x) if ((char *)(x) != NULL) free((char *)(x))

#define FREE_CMATRIX_17(m) mne_free_cmatrix_17((m))

#define FREE_ICMATRIX_17(m) mne_free_icmatrix_17((m))



void mne_free_cmatrix_17 (float **m)
{
    if (m) {
        FREE_17(*m);
        FREE_17(m);
    }
}

void mne_free_icmatrix_17 (int **m)

{
    if (m) {
        FREE_17(*m);
        FREE_17(m);
    }
}



#define NNEIGHBORS 26



//============================= mne_mgh_mri_io.c =============================


/*
 * The tag types are private to this module
 */
typedef struct {
    int           tag;
    long long     len;
    unsigned char *data;
} *mneMGHtag,mneMGHtagRec;

typedef struct {
    int        ntags;
    mneMGHtag  *tags;
} *mneMGHtagGroup,mneMGHtagGroupRec;


static void mne_free_mgh_tag(mneMGHtag t)
{
    if (!t)
        return;
    FREE_17(t->data);
    FREE_17(t);
    return;
}

void mne_free_mgh_tag_group(void *gp)

{
    int k;
    mneMGHtagGroup g = (mneMGHtagGroup)gp;

    if (!g)
        return;
    for (k = 0; k < g->ntags; k++)
        mne_free_mgh_tag(g->tags[k]);
    FREE_17(g->tags);
    FREE_17(g);

    return;
}




//float
Eigen::MatrixXf toFloatEigenMatrix_17(float **mat, const int m, const int n)
{
    Eigen::MatrixXf eigen_mat(m,n);

    for ( int i = 0; i < m; ++i)
        for ( int j = 0; j < n; ++j)
            eigen_mat(i,j) = mat[i][j];

    return eigen_mat;
}


void fromFloatEigenMatrix_17(const Eigen::MatrixXf& from_mat, float **& to_mat, const int m, const int n)
{
    for ( int i = 0; i < m; ++i)
        for ( int j = 0; j < n; ++j)
            to_mat[i][j] = from_mat(i,j);
}

void fromFloatEigenMatrix_17(const Eigen::MatrixXf& from_mat, float **& to_mat)
{
    fromFloatEigenMatrix_17(from_mat, to_mat, from_mat.rows(), from_mat.cols());
}



void fromIntEigenMatrix_17(const Eigen::MatrixXi& from_mat, int **&to_mat, const int m, const int n)
{
    for ( int i = 0; i < m; ++i)
        for ( int j = 0; j < n; ++j)
            to_mat[i][j] = from_mat(i,j);
}

void fromIntEigenMatrix_17(const Eigen::MatrixXi& from_mat, int **&to_mat)
{
    fromIntEigenMatrix_17(from_mat, to_mat, from_mat.rows(), from_mat.cols());
}


//============================= make_volume_source_space.c =============================

static FiffCoordTransOld* make_voxel_ras_trans(float *r0,
                                               float *x_ras,
                                               float *y_ras,
                                               float *z_ras,
                                               float *voxel_size)

{
    FiffCoordTransOld* t;
    float rot[3][3],move[3];
    int   j,k;

    VEC_COPY_17(move,r0);

    for (j = 0; j < 3; j++) {
        rot[j][0] = x_ras[j];
        rot[j][1] = y_ras[j];
        rot[j][2] = z_ras[j];
    }

    for (j = 0; j < 3; j++)
        for (k = 0; k < 3; k++)
            rot[j][k]    = voxel_size[k]*rot[j][k];

    t = new FiffCoordTransOld(FIFFV_MNE_COORD_MRI_VOXEL,FIFFV_COORD_MRI,rot,move);

    return t;
}





static int comp_points1(const void *vp1,const void *vp2)

{
  MneNearest* v1 = (MneNearest*)vp1;
  MneNearest* v2 = (MneNearest*)vp2;

  if (v1->nearest > v2->nearest)
    return 1;
  else if (v1->nearest == v2->nearest)
    return 0;
  else
    return -1;
}

static int comp_points2(const void *vp1,const void *vp2)

{
  MneNearest* v1 = (MneNearest*)vp1;
  MneNearest* v2 = (MneNearest*)vp2;

  if (v1->vert > v2->vert)
    return 1;
  else if (v1->vert == v2->vert)
    return 0;
  else
    return -1;
}

void mne_sort_nearest_by_nearest(MneNearest* points, int npoint)

{
  if (npoint > 1 && points != NULL)
    qsort(points,npoint,sizeof(MneNearest),comp_points1);
  return;
}




//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

MneSurfaceOrVolume::MneSurfaceOrVolume()
{

}


//*************************************************************************************************************

MneSurfaceOrVolume::~MneSurfaceOrVolume()
{
    int k;
    FREE_CMATRIX_17(this->rr);
    FREE_CMATRIX_17(this->nn);
    FREE_17(this->inuse);
    FREE_17(this->vertno);
    FREE_17(this->tris);
    FREE_ICMATRIX_17(this->itris);

    FREE_17(this->use_tris);
    FREE_ICMATRIX_17(this->use_itris);
    if (this->neighbor_tri) {
        for (k = 0; k < this->np; k++)
            FREE_17(this->neighbor_tri[k]);
        FREE_17(this->neighbor_tri);
    }
    FREE_17(this->nneighbor_tri);
    FREE_17(this->curv);

    if (this->neighbor_vert) {
        for (k = 0; k < this->np; k++)
            FREE_17(this->neighbor_vert[k]);
        FREE_17(this->neighbor_vert);
    }
    FREE_17(this->nneighbor_vert);
    if (this->vert_dist) {
        for (k = 0; k < this->np; k++)
            FREE_17(this->vert_dist[k]);
        FREE_17(this->vert_dist);
    }
    FREE_17(this->nearest);
    if (this->patches) {
        for (k = 0; k < this->npatch; k++)
            if(this->patches[k])
                delete this->patches[k];
        FREE_17(this->patches);
    }
    if(this->dist)
        delete this->dist;
    FREE_17(this->voxel_surf_RAS_t);
    FREE_17(this->MRI_voxel_surf_RAS_t);
    FREE_17(this->MRI_surf_RAS_RAS_t);
    if(this->interpolator)
        delete this->interpolator;
    this->MRI_volume.clear();

    if(this->vol_geom)
        delete this->vol_geom;
    mne_free_mgh_tag_group(this->mgh_tags);

    if (this->user_data && this->user_data_free)
        this->user_data_free(this->user_data);

}


//*************************************************************************************************************

double MneSurfaceOrVolume::solid_angle(float *from, MneTriangle* tri)	/* ...to this triangle */
/*
     * Compute the solid angle according to van Oosterom's
     * formula
     */
{
    double v1[3],v2[3],v3[3];
    double l1,l2,l3,s,triple;
    double cross[3];

    VEC_DIFF_17 (from,tri->r1,v1);
    VEC_DIFF_17 (from,tri->r2,v2);
    VEC_DIFF_17 (from,tri->r3,v3);

    CROSS_PRODUCT_17(v1,v2,cross);
    triple = VEC_DOT_17(cross,v3);

    l1 = VEC_LEN_17(v1);
    l2 = VEC_LEN_17(v2);
    l3 = VEC_LEN_17(v3);
    s = (l1*l2*l3+VEC_DOT_17(v1,v2)*l3+VEC_DOT_17(v1,v3)*l2+VEC_DOT_17(v2,v3)*l1);

    return (2.0*atan2(triple,s));
}


//*************************************************************************************************************

double MneSurfaceOrVolume::sum_solids(float *from, MneSurfaceOld* surf)
{
    int k;
    double tot_angle, angle;
    for (k = 0, tot_angle = 0.0; k < surf->ntri; k++) {
        angle = solid_angle(from,surf->tris+k);
        tot_angle += angle;
    }
    return tot_angle;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_filter_source_spaces(MneSurfaceOld* surf, float limit, FiffCoordTransOld* mri_head_t, MneSourceSpaceOld* *spaces, int nspace, FILE *filtered)   /* Provide a list of filtered points here */
/*
    * Remove all source space points closer to the surface than a given limit
    */
{
    MneSourceSpaceOld* s;
    int k,p1,p2;
    float r1[3];
    float mindist,dist,diff[3];
    int   minnode;
    int   omit,omit_outside;
    double tot_angle;

    if (surf == NULL)
        return OK;
    if (spaces[0]->coord_frame == FIFFV_COORD_HEAD && mri_head_t == NULL) {
        printf("Source spaces are in head coordinates and no coordinate transform was provided!");
        return FAIL;
    }
    /*
        * How close are the source points to the surface?
        */
    printf("Source spaces are in ");
    if (spaces[0]->coord_frame == FIFFV_COORD_HEAD)
        printf("head coordinates.\n");
    else if (spaces[0]->coord_frame == FIFFV_COORD_MRI)
        printf("MRI coordinates.\n");
    else
        printf("unknown (%d) coordinates.\n",spaces[0]->coord_frame);
    printf("Checking that the sources are inside the bounding surface ");
    if (limit > 0.0)
        printf("and at least %6.1f mm away",1000*limit);
    printf(" (will take a few...)\n");
    omit         = 0;
    omit_outside = 0;
    for (k = 0; k < nspace; k++) {
        s = spaces[k];
        for (p1 = 0; p1 < s->np; p1++)
            if (s->inuse[p1]) {
                VEC_COPY_17(r1,s->rr[p1]);	/* Transform the point to MRI coordinates */
                if (s->coord_frame == FIFFV_COORD_HEAD)
                    FiffCoordTransOld::fiff_coord_trans_inv(r1,mri_head_t,FIFFV_MOVE);
                /*
                * Check that the source is inside the inner skull surface
                */
                tot_angle = sum_solids(r1,surf)/(4*M_PI);
                if (fabs(tot_angle-1.0) > 1e-5) {
                    omit_outside++;
                    s->inuse[p1] = FALSE;
                    s->nuse--;
                    if (filtered)
                        fprintf(filtered,"%10.3f %10.3f %10.3f\n",
                                1000*r1[X_17],1000*r1[Y_17],1000*r1[Z_17]);
                }
                else if (limit > 0.0) {
                    /*
                        * Check the distance limit
                        */
                    mindist = 1.0;
                    minnode = 0;
                    for (p2 = 0; p2 < surf->np; p2++) {
                        VEC_DIFF_17(r1,surf->rr[p2],diff);
                        dist = VEC_LEN_17(diff);
                        if (dist < mindist) {
                            mindist = dist;
                            minnode = p2;
                        }
                    }
                    if (mindist < limit) {
                        omit++;
                        s->inuse[p1] = FALSE;
                        s->nuse--;
                        if (filtered)
                            fprintf(filtered,"%10.3f %10.3f %10.3f\n",
                                    1000*r1[X_17],1000*r1[Y_17],1000*r1[Z_17]);
                    }
                }
            }
    }
    if (omit_outside > 0)
        printf("%d source space points omitted because they are outside the inner skull surface.\n",
               omit_outside);
    if (omit > 0)
        printf("%d source space points omitted because of the %6.1f-mm distance limit.\n",
               omit,1000*limit);
    printf("Thank you for waiting.\n");
    return OK;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_add_patch_stats(MneSourceSpaceOld* s)
{
    MneNearest* nearest = s->nearest;
    MneNearest* this_patch;
    MnePatchInfo* *pinfo = MALLOC_17(s->nuse,MnePatchInfo*);
    int        nave,p,q,k;

    fprintf(stderr,"Computing patch statistics...\n");
    if (!s->neighbor_tri)
        if (mne_source_space_add_geometry_info(s,FALSE) != OK)
            goto bad;

    if (s->nearest == NULL) {
        qCritical("The patch information is not available.");
        goto bad;
    }
    if (s->nuse == 0) {
        FREE_17(s->patches);
        s->patches = NULL;
        s->npatch  = 0;
        return OK;
    }
    /*
       * Calculate the average normals and the patch areas
       */
    fprintf(stderr,"\tareas, average normals, and mean deviations...");
    mne_sort_nearest_by_nearest(nearest,s->np);
    nave = 1;
    for (p = 1, q = 0; p < s->np; p++) {
        if (nearest[p].nearest != nearest[p-1].nearest) {
            if (nave == 0) {
                qCritical("No vertices belong to the patch of vertex %d",nearest[p-1].nearest);
                goto bad;
            }
            if (s->vertno[q] == nearest[p-1].nearest) { /* Some source space points may have been omitted since
                               * the patch information was computed */
                pinfo[q] = new MnePatchInfo();
                pinfo[q]->vert = nearest[p-1].nearest;
                this_patch = nearest+p-nave;
                pinfo[q]->memb_vert = MALLOC_17(nave,int);
                pinfo[q]->nmemb     = nave;
                for (k = 0; k < nave; k++) {
                    pinfo[q]->memb_vert[k] = this_patch[k].vert;
                    this_patch[k].patch    = pinfo[q];
                }
                MnePatchInfo::calculate_patch_area(s,pinfo[q]);
                MnePatchInfo::calculate_normal_stats(s,pinfo[q]);
                q++;
            }
            nave = 0;
        }
        nave++;
    }
    if (nave == 0) {
        qCritical("No vertices belong to the patch of vertex %d",nearest[p-1].nearest);
        goto bad;
    }
    if (s->vertno[q] == nearest[p-1].nearest) {
        pinfo[q]       = new MnePatchInfo;
        pinfo[q]->vert = nearest[p-1].nearest;
        this_patch = nearest+p-nave;
        pinfo[q]->memb_vert = MALLOC_17(nave,int);
        pinfo[q]->nmemb = nave;
        for (k = 0; k < nave; k++) {
            pinfo[q]->memb_vert[k] = this_patch[k].vert;
            this_patch[k].patch = pinfo[q];
        }
        MnePatchInfo::calculate_patch_area(s,pinfo[q]);
        MnePatchInfo::calculate_normal_stats(s,pinfo[q]);
        q++;
    }
    fprintf(stderr," %d/%d [done]\n",q,s->nuse);

    if (s->patches) {
        for (k = 0; k < s->npatch; k++)
            if(s->patches[k])
                delete s->patches[k];
        FREE_17(s->patches);
    }
    s->patches = pinfo;
    s->npatch  = s->nuse;

    return OK;

bad : {
        FREE_17(pinfo);
        return FAIL;
    }
}


//*************************************************************************************************************

void MneSurfaceOrVolume::rearrange_source_space(MneSourceSpaceOld* s)
{
    int k,p;

    for (k = 0, s->nuse = 0; k < s->np; k++)
        if (s->inuse[k])
            s->nuse++;

    if (s->nuse == 0) {
        FREE_17(s->vertno);
        s->vertno = NULL;
    }
    else {
        s->vertno = REALLOC_17(s->vertno,s->nuse,int);
        for (k = 0, p = 0; k < s->np; k++)
            if (s->inuse[k])
                s->vertno[p++] = k;
    }
    if (s->nearest)
        mne_add_patch_stats(s);
    return;
}


//*************************************************************************************************************

void *MneSurfaceOrVolume::filter_source_space(void *arg)
{
    FilterThreadArg* a = (FilterThreadArg*)arg;
    int    p1,p2;
    double tot_angle;
    int    omit,omit_outside;
    float  r1[3];
    float  mindist,dist,diff[3];
    int    minnode;

    omit         = 0;
    omit_outside = 0;

    for (p1 = 0; p1 < a->s->np; p1++) {
        if (a->s->inuse[p1]) {
            VEC_COPY_17(r1,a->s->rr[p1]);	/* Transform the point to MRI coordinates */
            if (a->s->coord_frame == FIFFV_COORD_HEAD)
                FiffCoordTransOld::fiff_coord_trans_inv(r1,a->mri_head_t,FIFFV_MOVE);
            /*
           * Check that the source is inside the inner skull surface
           */
            tot_angle = sum_solids(r1,a->surf)/(4*M_PI);
            if (fabs(tot_angle-1.0) > 1e-5) {
                omit_outside++;
                a->s->inuse[p1] = FALSE;
                a->s->nuse--;
                if (a->filtered)
                    fprintf(a->filtered,"%10.3f %10.3f %10.3f\n",
                            1000*r1[X_17],1000*r1[Y_17],1000*r1[Z_17]);
            }
            else if (a->limit > 0.0) {
                /*
         * Check the distance limit
         */
                mindist = 1.0;
                minnode = 0;
                for (p2 = 0; p2 < a->surf->np; p2++) {
                    VEC_DIFF_17(r1,a->surf->rr[p2],diff);
                    dist = VEC_LEN_17(diff);
                    if (dist < mindist) {
                        mindist = dist;
                        minnode = p2;
                    }
                }
                if (mindist < a->limit) {
                    omit++;
                    a->s->inuse[p1] = FALSE;
                    a->s->nuse--;
                    if (a->filtered)
                        fprintf(a->filtered,"%10.3f %10.3f %10.3f\n",
                                1000*r1[X_17],1000*r1[Y_17],1000*r1[Z_17]);
                }
            }
        }
    }
    if (omit_outside > 0)
        fprintf(stderr,"%d source space points omitted because they are outside the inner skull surface.\n",
                omit_outside);
    if (omit > 0)
        fprintf(stderr,"%d source space points omitted because of the %6.1f-mm distance limit.\n",
                omit,1000*a->limit);
    a->stat = OK;
    return NULL;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::filter_source_spaces(float limit, char *bemfile, FiffCoordTransOld *mri_head_t, MneSourceSpaceOld* *spaces, int nspace, FILE *filtered, bool use_threads)                    /* Use multiple threads if possible? */
/*
          * Remove all source space points closer to the surface than a given limit
          */
{
    MneSurfaceOld*    surf = NULL;
    int             k;
    int             nproc = QThread::idealThreadCount();
    FilterThreadArg* a;

    if (!bemfile)
        return OK;

    if ((surf = MneSurfaceOld::read_bem_surface(bemfile,FIFFV_BEM_SURF_ID_BRAIN,FALSE,NULL)) == NULL) {
        qCritical("BEM model does not have the inner skull triangulation!");
        return FAIL;
    }
    /*
    * How close are the source points to the surface?
    */
    fprintf(stderr,"Source spaces are in ");
    if (spaces[0]->coord_frame == FIFFV_COORD_HEAD)
        fprintf(stderr,"head coordinates.\n");
    else if (spaces[0]->coord_frame == FIFFV_COORD_MRI)
        fprintf(stderr,"MRI coordinates.\n");
    else
        fprintf(stderr,"unknown (%d) coordinates.\n",spaces[0]->coord_frame);
    fprintf(stderr,"Checking that the sources are inside the inner skull ");
    if (limit > 0.0)
        fprintf(stderr,"and at least %6.1f mm away",1000*limit);
    fprintf(stderr," (will take a few...)\n");
    if (nproc < 2 || nspace == 1 || !use_threads) {
        /*
        * This is the conventional calculation
        */
        for (k = 0; k < nspace; k++) {
            a = new FilterThreadArg();
            a->s = spaces[k];
            a->mri_head_t = mri_head_t;
            a->surf = surf;
            a->limit = limit;
            a->filtered = filtered;
            filter_source_space(a);
            if(a)
                delete a;
            rearrange_source_space(spaces[k]);
        }
    }
    else {
        /*
        * Calculate all (both) source spaces simultaneously
        */
        QList<FilterThreadArg*> args;//filterThreadArg *args = MALLOC_17(nspace,filterThreadArg);

        for (k = 0; k < nspace; k++) {
            a = new FilterThreadArg();
            a->s = spaces[k];
            a->mri_head_t = mri_head_t;
            a->surf = surf;
            a->limit = limit;
            a->filtered = filtered;
            args.append(a);
        }
        /*
        * Ready to start the threads & Wait for them to complete
        */
        QtConcurrent::blockingMap(args, filter_source_space);

        for (k = 0; k < nspace; k++) {
            rearrange_source_space(spaces[k]);
            if(args[k])
                delete args[k];
        }
    }
    if(surf)
        delete surf;
    printf("Thank you for waiting.\n\n");

    return OK;
}


//*************************************************************************************************************

MneSourceSpaceOld* MneSurfaceOrVolume::make_volume_source_space(MneSurfaceOld* surf, float grid, float exclude, float mindist)
/*
    * Make a source space which covers the volume bounded by surf
    */
{
    float min[3],max[3],cm[3];
    int   minn[3],maxn[3];
    float *node,maxdist,dist,diff[3];
    int   k,c;
    MneSourceSpaceOld* sp = NULL;
    int np,nplane,nrow;
    int *neigh,nneigh;
    int x,y,z;
    /*
        * Figure out the grid size
        */
    cm[X_17] = cm[Y_17] = cm[Z_17] = 0.0;
    node = surf->rr[0];
    for (c = 0; c < 3; c++)
        min[c] = max[c] = node[c];

    for (k = 0; k < surf->np; k++) {
        node = surf->rr[k];
        for (c = 0; c < 3; c++) {
            cm[c] += node[c];
            if (node[c] < min[c])
                min[c] = node[c];
            if (node[c] > max[c])
                max[c] = node[c];
        }
    }
    for (c = 0; c < 3; c++)
        cm[c] = cm[c]/surf->np;
    /*
       * Define the sphere which fits the surface
       */
    maxdist = 0.0;
    for (k = 0; k < surf->np; k++) {
        VEC_DIFF_17(cm,surf->rr[k],diff);
        dist = VEC_LEN_17(diff);
        if (dist > maxdist)
            maxdist = dist;
    }
    printf("Surface CM = (%6.1f %6.1f %6.1f) mm\n",
           1000*cm[X_17], 1000*cm[Y_17], 1000*cm[Z_17]);
    printf("Surface fits inside a sphere with radius %6.1f mm\n",1000*maxdist);
    printf("Surface extent:\n"
           "\tx = %6.1f ... %6.1f mm\n"
           "\ty = %6.1f ... %6.1f mm\n"
           "\tz = %6.1f ... %6.1f mm\n",
           1000*min[X_17],1000*max[X_17],
           1000*min[Y_17],1000*max[Y_17],
           1000*min[Z_17],1000*max[Z_17]);
    for (c = 0; c < 3; c++) {
        if (max[c] > 0)
            maxn[c] = floor(fabs(max[c])/grid)+1;
        else
            maxn[c] = -floor(fabs(max[c])/grid)-1;
        if (min[c] > 0)
            minn[c] = floor(fabs(min[c])/grid)+1;
        else
            minn[c] = -floor(fabs(min[c])/grid)-1;
    }
    printf("Grid extent:\n"
           "\tx = %6.1f ... %6.1f mm\n"
           "\ty = %6.1f ... %6.1f mm\n"
           "\tz = %6.1f ... %6.1f mm\n",
           1000*(minn[X_17]*grid),1000*(maxn[X_17]*grid),
           1000*(minn[Y_17]*grid),1000*(maxn[Y_17]*grid),
           1000*(minn[Z_17]*grid),1000*(maxn[Z_17]*grid));
    /*
       * Now make the initial grid
       */
    np = 1;
    for (c = 0; c < 3; c++)
        np = np*(maxn[c]-minn[c]+1);
    nplane = (maxn[X_17]-minn[X_17]+1)*(maxn[Y_17]-minn[Y_17]+1);
    nrow   = (maxn[X_17]-minn[X_17]+1);
    sp = MneSurfaceOrVolume::mne_new_source_space(np);
    sp->type = MNE_SOURCE_SPACE_VOLUME;
    sp->nneighbor_vert = MALLOC_17(sp->np,int);
    sp->neighbor_vert = MALLOC_17(sp->np,int *);
    for (k = 0; k < sp->np; k++) {
        sp->inuse[k]  = TRUE;
        sp->vertno[k] = k;
        sp->nn[k][X_17] = sp->nn[k][Y_17] = 0.0; /* Source orientation is immaterial */
        sp->nn[k][Z_17] = 1.0;
        sp->neighbor_vert[k]  = neigh  = MALLOC_17(NNEIGHBORS,int);
        sp->nneighbor_vert[k] = nneigh = NNEIGHBORS;
        for (c = 0; c < nneigh; c++)
            neigh[c] = -1;
        sp->nuse++;
    }
    for (k = 0, z = minn[Z_17]; z <= maxn[Z_17]; z++) {
        for (y = minn[Y_17]; y <= maxn[Y_17]; y++) {
            for (x = minn[X_17]; x <= maxn[X_17]; x++, k++) {
                sp->rr[k][X_17] = x*grid;
                sp->rr[k][Y_17] = y*grid;
                sp->rr[k][Z_17] = z*grid;
                /*
             * Figure out the neighborhood:
             * 6-neighborhood first
             */
                neigh = sp->neighbor_vert[k];
                if (z > minn[Z_17])
                    neigh[0]  = k - nplane;
                if (x < maxn[X_17])
                    neigh[1] = k + 1;
                if (y < maxn[Y_17])
                    neigh[2] = k + nrow;
                if (x > minn[X_17])
                    neigh[3] = k - 1;
                if (y > minn[Y_17])
                    neigh[4] = k - nrow;
                if (z < maxn[Z_17])
                    neigh[5] = k + nplane;
                /*
             * Then the rest to complete the 26-neighborhood
             * First the plane below
             */
                if (z > minn[Z_17]) {
                    if (x < maxn[X_17]) {
                        neigh[6] = k + 1 - nplane;
                        if (y < maxn[Y_17])
                            neigh[7] = k + 1 + nrow - nplane;
                    }
                    if (y < maxn[Y_17])
                        neigh[8] = k + nrow - nplane;
                    if (x > minn[X_17]) {
                        if (y < maxn[Y_17])
                            neigh[9] = k - 1 + nrow - nplane;
                        neigh[10] = k - 1 - nplane;
                        if (y > minn[Y_17])
                            neigh[11] = k - 1 - nrow - nplane;
                    }
                    if (y > minn[Y_17]) {
                        neigh[12] = k - nrow - nplane;
                        if (x < maxn[X_17])
                            neigh[13] = k + 1 - nrow - nplane;
                    }
                }
                /*
             * Then the same plane
             */
                if (x < maxn[X_17] && y < maxn[Y_17])
                    neigh[14] = k + 1 + nrow;
                if (x > minn[X_17]) {
                    if (y < maxn[Y_17])
                        neigh[15] = k - 1 + nrow;
                    if (y > minn[Y_17])
                        neigh[16] = k - 1 - nrow;
                }
                if (y > minn[Y_17] && x < maxn[X_17])
                    neigh[17] = k + 1 - nrow - nplane;
                /*
             * Finally one plane above
             */
                if (z < maxn[Z_17]) {
                    if (x < maxn[X_17]) {
                        neigh[18] = k + 1 + nplane;
                        if (y < maxn[Y_17])
                            neigh[19] = k + 1 + nrow + nplane;
                    }
                    if (y < maxn[Y_17])
                        neigh[20] = k + nrow + nplane;
                    if (x > minn[X_17]) {
                        if (y < maxn[Y_17])
                            neigh[21] = k - 1 + nrow + nplane;
                        neigh[22] = k - 1 + nplane;
                        if (y > minn[Y_17])
                            neigh[23] = k - 1 - nrow + nplane;
                    }
                    if (y > minn[Y_17]) {
                        neigh[24] = k - nrow + nplane;
                        if (x < maxn[X_17])
                            neigh[25] = k + 1 - nrow + nplane;
                    }
                }
            }
        }
    }
    printf("%d sources before omitting any.\n",sp->nuse);
    /*
       * Exclude infeasible points
       */
    for (k = 0; k < sp->np; k++) {
        VEC_DIFF_17(cm,sp->rr[k],diff);
        dist = VEC_LEN_17(diff);
        if (dist < exclude || dist > maxdist) {
            sp->inuse[k] = FALSE;
            sp->nuse--;
        }
    }
    printf("%d sources after omitting infeasible sources.\n",sp->nuse);
    if (mne_filter_source_spaces(surf,mindist,NULL,&sp,1,NULL) != OK)
        goto bad;
    printf("%d sources remaining after excluding the sources outside the surface and less than %6.1f mm inside.\n",sp->nuse,1000*mindist);
    /*
       * Omit unused vertices from the neighborhoods
       */
    printf("Adjusting the neighborhood info...");
    for (k = 0; k < sp->np; k++) {
        neigh  = sp->neighbor_vert[k];
        nneigh = sp->nneighbor_vert[k];
        if (sp->inuse[k]) {
            for (c = 0; c < nneigh; c++)
                if (!sp->inuse[neigh[c]])
                    neigh[c] = -1;
        }
        else {
            for (c = 0; c < nneigh; c++)
                neigh[c] = -1;
        }
    }
    printf("[done]\n");
    /*
    * Set up the volume data (needed for creating the interpolation matrix)
    */
    {
        float r0[3],voxel_size[3],x_ras[3],y_ras[3],z_ras[3];
        int   width,height,depth;

        r0[X_17] = minn[X_17]*grid;
        r0[Y_17] = minn[Y_17]*grid;
        r0[Z_17] = minn[Z_17]*grid;

        voxel_size[0] = grid;
        voxel_size[1] = grid;
        voxel_size[2] = grid;

        width  = (maxn[X_17]-minn[X_17]+1);
        height = (maxn[Y_17]-minn[Y_17]+1);
        depth  = (maxn[Z_17]-minn[Z_17]+1);

        for (k = 0; k < 3; k++)
            x_ras[k] = y_ras[k] = z_ras[k] = 0.0;

        x_ras[0] = 1.0;
        y_ras[1] = 1.0;
        z_ras[2] = 1.0;

        if ((sp->voxel_surf_RAS_t = make_voxel_ras_trans(r0,x_ras,y_ras,z_ras,voxel_size)) == NULL)
            goto bad;

        sp->vol_dims[0] = width;
        sp->vol_dims[1] = height;
        sp->vol_dims[2] = depth;
        VEC_COPY_17(sp->voxel_size,voxel_size);
    }

    return sp;

bad : {
        if(sp)
            delete sp;
        return NULL;
    }
}





//*************************************************************************************************************

MneSourceSpaceOld* MneSurfaceOrVolume::mne_new_source_space(int np)
/*
          * Create a new source space and all associated data
          */
{
    MneSourceSpaceOld* res = new MneSourceSpaceOld();
    res->np      = np;
    if (np > 0) {
        res->rr      = ALLOC_CMATRIX_17(np,3);
        res->nn      = ALLOC_CMATRIX_17(np,3);
        res->inuse   = ALLOC_INT_17(np);
        res->vertno  = ALLOC_INT_17(np);
    }
    else {
        res->rr      = NULL;
        res->nn      = NULL;
        res->inuse   = NULL;
        res->vertno  = NULL;
    }
    res->nuse     = 0;
    res->ntri     = 0;
    res->tris     = NULL;
    res->itris    = NULL;
    res->tot_area = 0.0;

    res->nuse_tri  = 0;
    res->use_tris  = NULL;
    res->use_itris = NULL;

    res->neighbor_tri = NULL;
    res->nneighbor_tri = NULL;
    res->curv = NULL;
    res->val  = NULL;

    res->neighbor_vert = NULL;
    res->nneighbor_vert = NULL;
    res->vert_dist = NULL;

    res->coord_frame = FIFFV_COORD_MRI;
    res->id          = FIFFV_MNE_SURF_UNKNOWN;
    res->subject.clear();
    res->type        = FIFFV_MNE_SPACE_SURFACE;

    res->nearest = NULL;
    res->patches = NULL;
    res->npatch  = 0;

    res->dist       = NULL;
    res->dist_limit = -1.0;

    res->voxel_surf_RAS_t     = NULL;
    res->vol_dims[0] = res->vol_dims[1] = res->vol_dims[2] = 0;

    res->MRI_volume.clear();
    res->MRI_surf_RAS_RAS_t   = NULL;
    res->MRI_voxel_surf_RAS_t = NULL;
    res->MRI_vol_dims[0] = res->MRI_vol_dims[1] = res->MRI_vol_dims[2] = 0;
    res->interpolator         = NULL;

    res->vol_geom         = NULL;
    res->mgh_tags         = NULL;
    res->user_data        = NULL;
    res->user_data_free   = NULL;

    res->cm[X_17] = res->cm[Y_17] = res->cm[Z_17] = 0.0;

    return res;
}


//*************************************************************************************************************

MneSurfaceOld* MneSurfaceOrVolume::read_bem_surface(const QString &name, int which, int add_geometry, float *sigmap)          /* Conductivity? */
{
    return read_bem_surface(name,which,add_geometry,sigmap,true);
}


//*************************************************************************************************************

MneSurfaceOld* MneSurfaceOrVolume::read_bem_surface(const QString &name, int which, int add_geometry, float *sigmap, bool check_too_many_neighbors)
/*
     * Read a Neuromag-style BEM surface description
     */
{
    QFile file(name);
    FiffStream::SPtr stream(new FiffStream(&file));

    QList<FiffDirNode::SPtr> surfs;
    QList<FiffDirNode::SPtr> bems;
    FiffDirNode::SPtr node;
    FiffTag::SPtr t_pTag;

    int     id = -1;
    float   **nodes        = NULL;
    float   **node_normals = NULL;
    int     **triangles    = NULL;
    int     nnode,ntri;
    MneSurfaceOld* s = NULL;
    int k;
    int coord_frame = FIFFV_COORD_MRI;
    float sigma = -1.0;
    MatrixXf tmp_nodes;
    MatrixXi tmp_triangles;

    if(!stream->open())
        goto bad;
    /*
        * Check for the existence of BEM coord frame
        */
    bems = stream->dirtree()->dir_tree_find(FIFFB_BEM);
    if (bems.size() > 0) {
        node = bems[0];
        if (node->find_tag(stream, FIFF_BEM_COORD_FRAME, t_pTag)) {
            coord_frame = *t_pTag->toInt();
        }
    }
    surfs = stream->dirtree()->dir_tree_find(FIFFB_BEM_SURF);
    if (surfs.size() == 0) {
        printf ("No BEM surfaces found in %s",name.toUtf8().constData());
        goto bad;
    }
    if (which >= 0) {
        for (k = 0; k < surfs.size(); ++k) {
            node = surfs[k];
            /*
                * Read the data from this node
                */
            if (node->find_tag(stream, FIFF_BEM_SURF_ID, t_pTag)) {
                id = *t_pTag->toInt();
                if (id == which)
                    break;
            }
        }
        if (id != which) {
            printf("Desired surface not found in %s",name.toUtf8().constData());
            goto bad;
        }
    }
    else
        node = surfs[0];
    /*
       * Get the compulsory tags
       */
    if (!node->find_tag(stream, FIFF_BEM_SURF_NNODE, t_pTag))
        goto bad;
    nnode = *t_pTag->toInt();

    if (!node->find_tag(stream, FIFF_BEM_SURF_NTRI, t_pTag))
        goto bad;
    ntri = *t_pTag->toInt();

    if (!node->find_tag(stream, FIFF_BEM_SURF_NODES, t_pTag))
        goto bad;
    tmp_nodes = t_pTag->toFloatMatrix().transpose();
    nodes = ALLOC_CMATRIX_17(tmp_nodes.rows(),tmp_nodes.cols());
    fromFloatEigenMatrix_17(tmp_nodes, nodes);

    if (node->find_tag(stream, FIFF_BEM_SURF_NORMALS, t_pTag)) {\
        MatrixXf tmp_node_normals = t_pTag->toFloatMatrix().transpose();
        node_normals = ALLOC_CMATRIX_17(tmp_node_normals.rows(),tmp_node_normals.cols());
        fromFloatEigenMatrix_17(tmp_node_normals, node_normals);
    }

    if (!node->find_tag(stream, FIFF_BEM_SURF_TRIANGLES, t_pTag))
        goto bad;
    tmp_triangles = t_pTag->toIntMatrix().transpose();
    triangles = (int **)malloc(tmp_triangles.rows() * sizeof(int *));
    for (int i = 0; i < tmp_triangles.rows(); ++i)
        triangles[i] = (int *)malloc(tmp_triangles.cols() * sizeof(int));
    fromIntEigenMatrix_17(tmp_triangles, triangles);

    if (node->find_tag(stream, FIFF_MNE_COORD_FRAME, t_pTag)) {
        coord_frame = *t_pTag->toInt();
    }
    else if (node->find_tag(stream, FIFF_BEM_COORD_FRAME, t_pTag)) {
        coord_frame = *t_pTag->toInt();
    }
    if (node->find_tag(stream, FIFF_BEM_SIGMA, t_pTag)) {
        sigma = *t_pTag->toFloat();
    }

    stream->close();

    s = (MneSurfaceOld*)mne_new_source_space(0);
    for (k = 0; k < ntri; k++) {
        triangles[k][0]--;
        triangles[k][1]--;
        triangles[k][2]--;
    }
    s->itris       = triangles;
    s->id          = which;
    s->coord_frame = coord_frame;
    s->rr          = nodes;      nodes = NULL;
    s->nn          = node_normals; node_normals = NULL;
    s->ntri        = ntri;
    s->np          = nnode;
    s->curv        = NULL;
    s->val         = NULL;

    if (add_geometry) {
        if (check_too_many_neighbors) {
            if (mne_source_space_add_geometry_info((MneSourceSpaceOld*)s,!s->nn) != OK)
                goto bad;
        }
        else {
            if (mne_source_space_add_geometry_info2((MneSourceSpaceOld*)s,!s->nn) != OK)
                goto bad;
        }
    }
    else if (s->nn == NULL) {       /* Normals only */
        if (mne_add_vertex_normals((MneSourceSpaceOld*)s) != OK)
            goto bad;
    }
    else
        mne_add_triangle_data((MneSourceSpaceOld*)s);

    s->nuse   = s->np;
    s->inuse  = MALLOC_17(s->np,int);
    s->vertno = MALLOC_17(s->np,int);
    for (k = 0; k < s->np; k++) {
        s->inuse[k]  = TRUE;
        s->vertno[k] = k;
    }
    if (sigmap)
        *sigmap = sigma;

    return s;

bad : {
        FREE_CMATRIX_17(nodes);
        FREE_CMATRIX_17(node_normals);
        FREE_ICMATRIX_17(triangles);
        stream->close();
        return NULL;
    }
}


//*************************************************************************************************************

void MneSurfaceOrVolume::mne_triangle_coords(float *r, MneSurfaceOld* s, int tri, float *x, float *y, float *z)
/*
          * Compute the coordinates of a point within a triangle
          */
{
    double rr[3];			/* Vector from triangle corner #1 to r */
    double a,b,c,v1,v2,det;
    MneTriangle* this_tri;

    this_tri = s->tris+tri;

    VEC_DIFF_17(this_tri->r1,r,rr);
    *z = VEC_DOT_17(rr,this_tri->nn);

    a =  VEC_DOT_17(this_tri->r12,this_tri->r12);
    b =  VEC_DOT_17(this_tri->r13,this_tri->r13);
    c =  VEC_DOT_17(this_tri->r12,this_tri->r13);

    v1 = VEC_DOT_17(rr,this_tri->r12);
    v2 = VEC_DOT_17(rr,this_tri->r13);

    det = a*b - c*c;

    *x = (b*v1 - c*v2)/det;
    *y = (a*v2 - c*v1)/det;

    return;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::nearest_triangle_point(float *r, MneSurfaceOld* s, void *user, int tri, float *x, float *y, float *z)
/*
          * Find the nearest point from a triangle
          */
{

    double p,q,p0,q0,t0;
    double rr[3];			/* Vector from triangle corner #1 to r */
    double a,b,c,v1,v2,det;
    double best,dist,dist0;
    projData    pd = (projData)user;
    MneTriangle* this_tri;

    this_tri = s->tris+tri;
    VEC_DIFF_17(this_tri->r1,r,rr);
    dist  = VEC_DOT_17(rr,this_tri->nn);

    if (pd) {
        if (!pd->act[tri])
            return FALSE;
        a = pd->a[tri];
        b = pd->b[tri];
        c = pd->c[tri];
    }
    else {
        a =  VEC_DOT_17(this_tri->r12,this_tri->r12);
        b =  VEC_DOT_17(this_tri->r13,this_tri->r13);
        c =  VEC_DOT_17(this_tri->r12,this_tri->r13);
    }

    v1 = VEC_DOT_17(rr,this_tri->r12);
    v2 = VEC_DOT_17(rr,this_tri->r13);

    det = a*b - c*c;

    p = (b*v1 - c*v2)/det;
    q = (a*v2 - c*v1)/det;
    /*
       * If the point projects into the triangle we are done
       */
    if (p >= 0.0 && p <= 1.0 &&
            q >= 0.0 && q <= 1.0 &&
            q <= 1.0 - p) {
        *x = p;
        *y = q;
        *z = dist;
        return TRUE;
    }
    /*
       * Tough: must investigate the sides
       * We might do something intelligent here. However, for now it is ok
       * to do it in the hard way
       */
    /*
       * Side 1 -> 2
       */
    p0 = p + 0.5*(q * c)/a;
    if (p0 < 0.0)
        p0 = 0.0;
    else if (p0 > 1.0)
        p0 = 1.0;
    q0 = 0.0;
    dist0 = sqrt((p-p0)*(p-p0)*a +
                 (q-q0)*(q-q0)*b +
                 (p-p0)*(q-q0)*c +
                 dist*dist);
    best = dist0;
    *x = p0;
    *y = q0;
    *z = dist0;
    /*
       * Side 2 -> 3
       */
    t0 = 0.5*((2.0*a-c)*(1.0-p) + (2.0*b-c)*q)/(a+b-c);
    if (t0 < 0.0)
        t0 = 0.0;
    else if (t0 > 1.0)
        t0 = 1.0;
    p0 = 1.0 - t0;
    q0 = t0;
    dist0 = sqrt((p-p0)*(p-p0)*a +
                 (q-q0)*(q-q0)*b +
                 (p-p0)*(q-q0)*c +
                 dist*dist);
    if (dist0 < best) {
        best = dist0;
        *x = p0;
        *y = q0;
        *z = dist0;
    }
    /*
       * Side 1 -> 3
       */
    p0 = 0.0;
    q0 = q + 0.5*(p * c)/b;
    if (q0 < 0.0)
        q0 = 0.0;
    else if (q0 > 1.0)
        q0 = 1.0;
    dist0 = sqrt((p-p0)*(p-p0)*a +
                 (q-q0)*(q-q0)*b +
                 (p-p0)*(q-q0)*c +
                 dist*dist);
    if (dist0 < best) {
        best = dist0;
        *x = p0;
        *y = q0;
        *z = dist0;
    }
    return TRUE;
}


//*************************************************************************************************************

void MneSurfaceOrVolume::project_to_triangle(MneSurfaceOld* s, int tri, float p, float q, float *r)
{
    int   k;
    MneTriangle* this_tri;

    this_tri = s->tris+tri;

    for (k = 0; k < 3; k++)
        r[k] = this_tri->r1[k] + p*this_tri->r12[k] + q*this_tri->r13[k];

    return;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_nearest_triangle_point(float *r, MneSurfaceOld* s, int tri, float *x, float *y, float *z)
/*
     * This is for external use
     */
{
    return nearest_triangle_point(r,s,NULL,tri,x,y,z);
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_project_to_surface(MneSurfaceOld* s, void *proj_data, float *r, int project_it, float *distp)
/*
          * Project the point onto the closest point on the surface
          */
{
    float dist;			/* Distance to the triangle */
    float p,q;			/* Coordinates on the triangle */
    float p0,q0,dist0;
    int   best;
    int   k;

    p0 = q0 = 0.0;
    dist0 = 0.0;
    for (best = -1, k = 0; k < s->ntri; k++) {
        if (nearest_triangle_point(r,s,proj_data,k,&p,&q,&dist)) {
            if (best < 0 || fabs(dist) < fabs(dist0)) {
                dist0 = dist;
                best = k;
                p0 = p;
                q0 = q;
            }
        }
    }
    if (best >= 0 && project_it)
        project_to_triangle(s,best,p0,q0,r);
    if (distp)
        *distp = dist0;
    return best;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_read_source_spaces(const QString &name, MneSourceSpaceOld* **spacesp, int *nspacep)
/*
* Read source spaces from a FIFF file
*/
{
    QFile file(name);
    FiffStream::SPtr stream(new FiffStream(&file));

    int            nspace = 0;
    MneSourceSpaceOld* *spaces = NULL;
    MneSourceSpaceOld*  new_space = NULL;
    QList<FiffDirNode::SPtr> sources;
    FiffDirNode::SPtr     node;
    FiffTag::SPtr t_pTag;
    int             j,k,p,q;
    int             ntri;
    int             *nearest = NULL;
    float           *nearest_dist = NULL;
    int             *nneighbors = NULL;
    int             *neighbors  = NULL;
    int             *vol_dims = NULL;

    if(!stream->open())
        goto bad;

    sources = stream->dirtree()->dir_tree_find(FIFFB_MNE_SOURCE_SPACE);
    if (sources.size() == 0) {
        printf("No source spaces available here");
        goto bad;
    }
    for (j = 0; j < sources.size(); j++) {
        new_space = MneSurfaceOrVolume::mne_new_source_space(0);
        node = sources[j];
        /*
            * Get the mandatory data first
            */
        if (!node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_NPOINTS, t_pTag)) {
            goto bad;
        }
        new_space->np = *t_pTag->toInt();
        if (new_space->np == 0) {
            printf("No points in this source space");
            goto bad;
        }
        if (!node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_POINTS, t_pTag)) {
            goto bad;
        }
        MatrixXf tmp_rr = t_pTag->toFloatMatrix().transpose();
        new_space->rr = ALLOC_CMATRIX_17(tmp_rr.rows(),tmp_rr.cols());
        fromFloatEigenMatrix_17(tmp_rr,new_space->rr);
        if (!node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_NORMALS, t_pTag)) {
            goto bad;
        }
        MatrixXf tmp_nn = t_pTag->toFloatMatrix().transpose();
        new_space->nn = ALLOC_CMATRIX_17(tmp_nn.rows(),tmp_nn.cols());
        fromFloatEigenMatrix_17(tmp_nn,new_space->nn);
        if (!node->find_tag(stream, FIFF_MNE_COORD_FRAME, t_pTag)) {
            new_space->coord_frame = FIFFV_COORD_MRI;
        }
        else {
            new_space->coord_frame = *t_pTag->toInt();
        }
        if (node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_ID, t_pTag)) {
            new_space->id = *t_pTag->toInt();
        }
        if (node->find_tag(stream, FIFF_SUBJ_HIS_ID, t_pTag)) {
            new_space->subject = (char *)t_pTag->data();
        }
        if (node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_TYPE, t_pTag)) {
            new_space->type = *t_pTag->toInt();
        }
        ntri = 0;
        if (node->find_tag(stream, FIFF_BEM_SURF_NTRI, t_pTag)) {
            ntri = *t_pTag->toInt();
        }
        else if (node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_NTRI, t_pTag)) {
            ntri = *t_pTag->toInt();
        }
        if (ntri > 0) {
            int **itris = NULL;

            if (!node->find_tag(stream, FIFF_BEM_SURF_TRIANGLES, t_pTag)) {
                if (!node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_TRIANGLES, t_pTag)) {
                    goto bad;
                }
            }

            MatrixXi tmp_itris = t_pTag->toIntMatrix().transpose();
            itris = (int **)malloc(tmp_itris.rows() * sizeof(int *));
            for (int i = 0; i < tmp_itris.rows(); ++i)
                itris[i] = (int *)malloc(tmp_itris.cols() * sizeof(int));
            fromIntEigenMatrix_17(tmp_itris, itris);

            for (p = 0; p < ntri; p++) { /* Adjust the numbering */
                itris[p][X_17]--;
                itris[p][Y_17]--;
                itris[p][Z_17]--;
            }
            new_space->itris = itris; itris = NULL;
            new_space->ntri = ntri;
        }
        if (!node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_NUSE, t_pTag)) {
            if (new_space->type == FIFFV_MNE_SPACE_VOLUME) {
                /*
                    * Use all
                    */
                new_space->nuse   = new_space->np;
                new_space->inuse  = MALLOC_17(new_space->nuse,int);
                new_space->vertno = MALLOC_17(new_space->nuse,int);
                for (k = 0; k < new_space->nuse; k++) {
                    new_space->inuse[k]  = TRUE;
                    new_space->vertno[k] = k;
                }
            }
            else {
                /*
                    * None in use
                    * NOTE: The consequences of this change have to be evaluated carefully
                    */
                new_space->nuse   = 0;
                new_space->inuse  = MALLOC_17(new_space->np,int);
                new_space->vertno = NULL;
                for (k = 0; k < new_space->np; k++)
                    new_space->inuse[k]  = FALSE;
            }
        }
        else {
            new_space->nuse = *t_pTag->toInt();
            if (!node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_SELECTION, t_pTag)) {
                goto bad;
            }

            qDebug() << "ToDo: Check whether new_space->inuse contains the right stuff!!! - use VectorXi instead";
//            new_space->inuse  = t_pTag->toInt();
            new_space->inuse = MALLOC_17(new_space->np,int); //DEBUG
            if (new_space->nuse > 0) {
                new_space->vertno = MALLOC_17(new_space->nuse,int);
                for (k = 0, p = 0; k < new_space->np; k++) {
                    new_space->inuse[k] = t_pTag->toInt()[k]; //DEBUG
                    if (new_space->inuse[k])
                        new_space->vertno[p++] = k;
                }
            }
            else {
                FREE_17(new_space->vertno);
                new_space->vertno = NULL;
            }
            /*
                * Selection triangulation
                */
            ntri = 0;
            if (node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_NUSE_TRI, t_pTag)) {
                ntri = *t_pTag->toInt();
            }
            if (ntri > 0) {
                int **itris = NULL;

                if (!node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_USE_TRIANGLES, t_pTag)) {
                    goto bad;
                }

                MatrixXi tmp_itris = t_pTag->toIntMatrix().transpose();
                itris = (int **)malloc(tmp_itris.rows() * sizeof(int *));
                for (int i = 0; i < tmp_itris.rows(); ++i)
                    itris[i] = (int *)malloc(tmp_itris.cols() * sizeof(int));
                fromIntEigenMatrix_17(tmp_itris, itris);
                for (p = 0; p < ntri; p++) { /* Adjust the numbering */
                    itris[p][X_17]--;
                    itris[p][Y_17]--;
                    itris[p][Z_17]--;
                }
                new_space->use_itris = itris; itris = NULL;
                new_space->nuse_tri = ntri;
            }
            /*
                * The patch information becomes relevant here
                */
            if (node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_NEAREST, t_pTag)) {
                nearest  = t_pTag->toInt();
                new_space->nearest = MALLOC_17(new_space->np,MneNearest);
                for (k = 0; k < new_space->np; k++) {
                    new_space->nearest[k].vert = k;
                    new_space->nearest[k].nearest = nearest[k];
                    new_space->nearest[k].patch = NULL;
                }

                if (!node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_NEAREST_DIST, t_pTag)) {
                    goto bad;
                }
                qDebug() << "ToDo: Check whether nearest_dist contains the right stuff!!! - use VectorXf instead";
                nearest_dist = t_pTag->toFloat();
                for (k = 0; k < new_space->np; k++) {
                    new_space->nearest[k].dist = nearest_dist[k];
                }
//                FREE_17(nearest); nearest = NULL;
//                FREE_17(nearest_dist); nearest_dist = NULL;
            }
            /*
            * We may have the distance matrix
            */
            if (node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_DIST_LIMIT, t_pTag)) {
                new_space->dist_limit = *t_pTag->toFloat();
                if (node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_DIST, t_pTag)) {
//                    SparseMatrix<double> tmpSparse = t_pTag->toSparseFloatMatrix();
                    FiffSparseMatrix* dist = FiffSparseMatrix::fiff_get_float_sparse_matrix(t_pTag);
                    new_space->dist = dist->mne_add_upper_triangle_rcs();
                    delete dist;
                    if (!new_space->dist) {
                        goto bad;
                    }
                }
                else
                    new_space->dist_limit = 0.0;
            }
        }
        /*
            * For volume source spaces we might have the neighborhood information
            */
        if (new_space->type == FIFFV_MNE_SPACE_VOLUME) {
            int ntot,nvert,ntot_count,nneigh;
            int *neigh;

            nneighbors = neighbors = NULL;
            ntot = nvert = 0;
            if (node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_NEIGHBORS, t_pTag)) {
                qDebug() << "ToDo: Check whether neighbors contains the right stuff!!! - use VectorXi instead";
                neighbors = t_pTag->toInt();
                ntot      = t_pTag->size()/sizeof(fiff_int_t);
            }
            if (node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_NNEIGHBORS, t_pTag)) {
                qDebug() << "ToDo: Check whether nneighbors contains the right stuff!!! - use VectorXi instead";
                nneighbors = t_pTag->toInt();
                nvert      = t_pTag->size()/sizeof(fiff_int_t);
            }
            if (neighbors && nneighbors) {
                if (nvert != new_space->np) {
                    printf("Inconsistent neighborhood data in file.");
                    goto bad;
                }
                for (k = 0, ntot_count = 0; k < nvert; k++)
                    ntot_count += nneighbors[k];
                if (ntot_count != ntot) {
                    printf("Inconsistent neighborhood data in file.");
                    goto bad;
                }
                new_space->nneighbor_vert = MALLOC_17(nvert,int);
                new_space->neighbor_vert  = MALLOC_17(nvert,int *);
                for (k = 0, q = 0; k < nvert; k++) {
                    new_space->nneighbor_vert[k] = nneigh = nneighbors[k];
                    new_space->neighbor_vert[k] = neigh = MALLOC_17(nneigh,int);
                    for (p = 0; p < nneigh; p++,q++)
                        neigh[p] = neighbors[q];
                }
            }
            FREE_17(neighbors);
            FREE_17(nneighbors);
            nneighbors = neighbors = NULL;
            /*
                * There might be a coordinate transformation and dimensions
                */
            new_space->voxel_surf_RAS_t   = FiffCoordTransOld::mne_read_transform_from_node(stream, node, FIFFV_MNE_COORD_MRI_VOXEL, FIFFV_MNE_COORD_SURFACE_RAS);
            if (!node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_VOXEL_DIMS, t_pTag)) {
                qDebug() << "ToDo: Check whether vol_dims contains the right stuff!!! - use VectorXi instead";
                vol_dims = t_pTag->toInt();
            }
            if (vol_dims)
                VEC_COPY_17(new_space->vol_dims,vol_dims);
            {
                QList<FiffDirNode::SPtr>  mris = node->dir_tree_find(FIFFB_MNE_PARENT_MRI_FILE);

                if (mris.size() == 0) { /* The old way */
                    new_space->MRI_surf_RAS_RAS_t = FiffCoordTransOld::mne_read_transform_from_node(stream, node, FIFFV_MNE_COORD_SURFACE_RAS, FIFFV_MNE_COORD_RAS);
                    if (node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_MRI_FILE, t_pTag)) {
                        qDebug() << "ToDo: Check whether new_space->MRI_volume  contains the right stuff!!! - use QString instead";
                        new_space->MRI_volume = (char *)t_pTag->data();
                    }
                    if (node->find_tag(stream, FIFF_MNE_SOURCE_SPACE_INTERPOLATOR, t_pTag)) {
                        new_space->interpolator = FiffSparseMatrix::fiff_get_float_sparse_matrix(t_pTag);
                    }
                }
                else {
                    if (node->find_tag(stream, FIFF_MNE_FILE_NAME, t_pTag)) {
                        new_space->MRI_volume = (char *)t_pTag->data();
                    }
                    new_space->MRI_surf_RAS_RAS_t = FiffCoordTransOld::mne_read_transform_from_node(stream, mris[0], FIFFV_MNE_COORD_SURFACE_RAS, FIFFV_MNE_COORD_RAS);
                    new_space->MRI_voxel_surf_RAS_t   = FiffCoordTransOld::mne_read_transform_from_node(stream, mris[0], FIFFV_MNE_COORD_MRI_VOXEL, FIFFV_MNE_COORD_SURFACE_RAS);

                    if (mris[0]->find_tag(stream, FIFF_MNE_SOURCE_SPACE_INTERPOLATOR, t_pTag)) {
                        new_space->interpolator = FiffSparseMatrix::fiff_get_float_sparse_matrix(t_pTag);
                    }
                    if (mris[0]->find_tag(stream, FIFF_MRI_WIDTH, t_pTag)) {
                        new_space->MRI_vol_dims[0] = *t_pTag->toInt();
                    }
                    if (mris[0]->find_tag(stream, FIFF_MRI_HEIGHT, t_pTag)) {
                        new_space->MRI_vol_dims[1] = *t_pTag->toInt();
                    }
                    if (mris[0]->find_tag(stream, FIFF_MRI_DEPTH, t_pTag)) {
                        new_space->MRI_vol_dims[2] = *t_pTag->toInt();
                    }
                }
            }
        }
        mne_add_triangle_data(new_space);
        spaces = REALLOC_17(spaces,nspace+1,MneSourceSpaceOld*);
        spaces[nspace++] = new_space;
        new_space = NULL;
    }
    stream->close();

    *spacesp = spaces;
    *nspacep = nspace;

    return FIFF_OK;

bad : {
        stream->close();
        delete new_space;
        for (k = 0; k < nspace; k++)
            delete spaces[k];
        FREE_17(spaces);
        FREE_17(nearest);
        FREE_17(nearest_dist);
        FREE_17(neighbors);
        FREE_17(nneighbors);
        FREE_17(vol_dims);

        return FIFF_FAIL;
    }
}


//*************************************************************************************************************

void MneSurfaceOrVolume::mne_source_space_update_inuse(MneSourceSpaceOld* s, int *new_inuse)
/*
* Update the active vertices
*/
{
    int k,p,nuse;

    if (!s)
        return;

    FREE_17(s->inuse); s->inuse = new_inuse;

    for (k = 0, nuse = 0; k < s->np; k++)
        if (s->inuse[k])
            nuse++;

    s->nuse   = nuse;
    if (s->nuse > 0) {
        s->vertno = REALLOC_17(s->vertno,s->nuse,int);
        for (k = 0, p = 0; k < s->np; k++)
            if (s->inuse[k])
                s->vertno[p++] = k;
    }
    else {
        FREE_17(s->vertno);
        s->vertno = NULL;
    }
    return;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_is_left_hemi_source_space(MneSourceSpaceOld* s)
/*
* Left or right hemisphere?
*/
{
    int k;
    float xave;

    for (k = 0, xave = 0.0; k < s->np; k++)
        xave += s->rr[k][0];
    if (xave < 0.0)
        return TRUE;
    else
        return FALSE;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_transform_source_space(MneSourceSpaceOld* ss, FiffCoordTransOld *t)
/*
    * Transform source space data into another coordinate frame
    */
{
    int k;
    if (ss == NULL)
        return OK;
    if (ss->coord_frame == t->to)
        return OK;
    if (ss->coord_frame != t->from) {
        printf("Coordinate transformation does not match with the source space coordinate system.");
        return FAIL;
    }
    for (k = 0; k < ss->np; k++) {
        FiffCoordTransOld::fiff_coord_trans(ss->rr[k],t,FIFFV_MOVE);
        FiffCoordTransOld::fiff_coord_trans(ss->nn[k],t,FIFFV_NO_MOVE);
    }
    if (ss->tris) {
        for (k = 0; k < ss->ntri; k++)
            FiffCoordTransOld::fiff_coord_trans(ss->tris[k].nn,t,FIFFV_NO_MOVE);
    }
    ss->coord_frame = t->to;
    return OK;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_transform_source_spaces_to(int coord_frame, FiffCoordTransOld *t, MneSourceSpaceOld* *spaces, int nspace)
/*
* Facilitate the transformation of the source spaces
*/
{
    MneSourceSpaceOld* s;
    int k;
    FiffCoordTransOld* my_t;

    for (k = 0; k < nspace; k++) {
        s = spaces[k];
        if (s->coord_frame != coord_frame) {
            if (t) {
                if (s->coord_frame == t->from && t->to == coord_frame) {
                    if (mne_transform_source_space(s,t) != OK)
                        return FAIL;
                }
                else if (s->coord_frame == t->to && t->from == coord_frame) {
                    my_t = t->fiff_invert_transform();
                    if (mne_transform_source_space(s,my_t) != OK) {
                        FREE_17(my_t);
                        return FAIL;
                    }
                    FREE_17(my_t);
                }
                else {
                    printf("Could not transform a source space because of transformation incompatibility.");
                    return FAIL;
                }
            }
            else {
                printf("Could not transform a source space because of missing coordinate transformation.");
                return FAIL;
            }
        }
    }
    return OK;
}


//*************************************************************************************************************

void MneSurfaceOrVolume::enable_all_sources(MneSourceSpaceOld* s)
{
    int k;
    for (k = 0; k < s->np; k++)
        s->inuse[k] = TRUE;
    s->nuse = s->np;
    return;
}


//*************************************************************************************************************

#define LH_LABEL_TAG "-lh.label"
#define RH_LABEL_TAG "-rh.label"

int MneSurfaceOrVolume::restrict_sources_to_labels(MneSourceSpaceOld* *spaces, int nspace, const QStringList& labels, int nlabel)
/*
* Pick only sources within a label
*/
{
    MneSourceSpaceOld* lh = NULL;
    MneSourceSpaceOld* rh = NULL;
    MneSourceSpaceOld* sp;
    int            *lh_inuse = NULL;
    int            *rh_inuse = NULL;
    int            *sel = NULL;
    int            nsel;
    int            *inuse;
    int            k,p;

    if (nlabel == 0)
        return OK;


    for (k = 0; k < nspace; k++) {
        if (mne_is_left_hemi_source_space(spaces[k])) {
            lh = spaces[k];
            FREE_17(lh_inuse);
            lh_inuse = MALLOC_17(lh->np,int);
            for (p = 0; p < lh->np; p++)
                lh_inuse[p] = 0;
        }
        else {
            rh = spaces[k];
            FREE_17(rh_inuse);
            rh_inuse = MALLOC_17(rh->np,int);
            for (p = 0; p < rh->np; p++)
                rh_inuse[p] = 0;
        }
    }
    /*
       * Go through each label file
       */
    for (k = 0; k < nlabel; k++) {
        /*
         * Which hemi?
         */
        if (labels[k].contains(LH_LABEL_TAG)){ //strstr(labels[k],LH_LABEL_TAG) != NULL) {
            sp = lh;
            inuse = lh_inuse;
        }
        else if (labels[k].contains(RH_LABEL_TAG)){ //strstr(labels[k],RH_LABEL_TAG) != NULL) {
            sp = rh;
            inuse = rh_inuse;
        }
        else {
            printf("\tWarning: cannot assign label file %s to a hemisphere.\n",labels[k].toUtf8().constData());
            continue;
        }
        if (sp) {
            if (mne_read_label(labels[k],NULL,&sel,&nsel) == FAIL)
                goto bad;
            for (p = 0; p < nsel; p++) {
                if (sel[p] >= 0 && sel[p] < sp->np)
                    inuse[sel[p]] = sp->inuse[sel[p]];
                else
                    printf("vertex number out of range in %s (%d vs %d)\n",
                           labels[k].toUtf8().constData(),sel[p],sp->np);
            }
            FREE_17(sel); sel = NULL;
            printf("Processed label file %s\n",labels[k].toUtf8().constData());
        }
    }
    mne_source_space_update_inuse(lh,lh_inuse);
    mne_source_space_update_inuse(rh,rh_inuse);
    return OK;


bad : {
        FREE_17(lh_inuse);
        FREE_17(rh_inuse);
        FREE_17(sel);
        return FAIL;
    }
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_find_sources_in_label(char *label, MneSourceSpaceOld* s, int off, int **selp, int *nselp)	    /* How many selected? */
/*
* Find the source points within a label
*/
{
    FILE *in = NULL;
    int  res = FAIL;

    int  nsel = 0;
    int  *sel = NULL;

    int k,p,pp,nlabel,q;
    char c;
    float fdum;
    /*
       * Read the label file
       */
    if ((in = fopen(label,"r")) == NULL) {
        qCritical(label);//err_set_sys_error(label);
        goto out;
    }
    c = fgetc(in);
    if (c !='#') {
        qCritical("Label file does not start correctly.");
        goto out;
    }
    for (c = fgetc(in); c != '\n'; c = fgetc(in))
        ;
    if (fscanf(in,"%d",&nlabel) != 1) {
        qCritical("Could not read the number of labelled points.");
        goto out;
    }
#ifdef DEBUG
    printf("\t%d points in label %s\n",nlabel,label);
#endif
    for (k = 0; k < nlabel; k++) {
        if (fscanf(in,"%d %g %g %g %g",&p,
                   &fdum, &fdum, &fdum, &fdum) != 5) {
            qCritical("Could not read label point # %d",k+1);
            goto out;
        }
        if (p < 0 || p >= s->np) {
            qCritical("Source index out of range %d (range 0..%d)\n",p,s->np-1);
            goto out;
        }
        if (s->inuse[p]) {
            for (pp = 0, q = 0; pp < p; pp++) {
                if (s->inuse[pp])
                    q++;
            }
            sel = REALLOC_17(sel,nsel+1,int);
            sel[nsel++] = q + off;
        }
    }
    *nselp = nsel;
    *selp  = sel;
    res = OK;

out : {
        if (in)
            fclose(in);
        if (res != OK) {
            FREE_17(sel);
            *selp = NULL;
            *nselp = 0;
        }
        return res;
    }

}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_read_label(const QString& label, char **commentp, int **selp, int *nselp)	    /* How many? */
/*
          * Find the source points within a label
          */
{
    FILE *in = NULL;
    int  res = FAIL;

    int  nsel = 0;
    int  *sel = NULL;

    int k,p,nlabel;
    char c;
    char *comment = NULL;
    float fdum;
    /*
       * Read the label file
       */
    if ((in = fopen(label.toUtf8().constData(),"r")) == NULL) {
        qCritical() << label;//err_set_sys_error(label);
        goto out;
    }
    for (k = 0; k < 2; k++) {
        rewind(in);
        c = fgetc(in);
        if (c !='#') {
            qCritical("Label file does not start correctly.");
            goto out;
        }
        for (c = fgetc(in); c == ' ' && c != '\n'; c = fgetc(in))
            ;
        ungetc(c,in);
        if (k == 0) {
            for (c = fgetc(in), p = 0; c != '\n'; c = fgetc(in), p++)
                ;
        }
        else {
            for (c = fgetc(in); c != '\n'; c = fgetc(in))
                *comment++ = c;
            *comment = '\0';
        }
        if (!commentp)
            break;
        if (p == 0) {
            *commentp = NULL;
            break;
        }
        if (k == 0)
            *commentp = comment = MALLOC_17(p+1,char);
    }
    if (fscanf(in,"%d",&nlabel) != 1) {
        qCritical("Could not read the number of labelled points.");
        goto out;
    }
    for (k = 0; k < nlabel; k++) {
        if (fscanf(in,"%d %g %g %g %g",&p,
                   &fdum, &fdum, &fdum, &fdum) != 5) {
            qCritical("Could not read label point # %d",k+1);
            goto out;
        }
        sel = REALLOC_17(sel,nsel+1,int);
        sel[nsel++] = p;
    }
    *nselp = nsel;
    *selp  = sel;
    res = OK;

out : {
        if (in)
            fclose(in);
        if (res != OK) {
            FREE_17(sel);
            *selp = NULL;
            *nselp = 0;
        }
        return res;
    }

}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_write_label(char *label, char *comment, int *sel, int nsel, float **rr)	    /* Locations of the nodes in MRI coords */
/*
          * Find the source points within a label
          */
{
    FILE  *out = NULL;
    int   res = FAIL;
    int   k;
    float fdum = 0.0;
    /*
       * Read the label file
       */
    if ((out = fopen(label,"w")) == NULL) {
        qCritical(label);//err_set_sys_error(label);
        goto out;
    }
    if (comment == NULL)
        fprintf(out,"# Label file created by the MNE software\n");
    else
        fprintf(out,"# %s\n",comment);

    fprintf(out,"%d\n",nsel);
    if (rr != NULL)
        for (k = 0; k < nsel; k++)
            fprintf(out,"%d %.2f %.2f %.2f %g\n",sel[k],
                    1000*rr[sel[k]][0],
                    1000*rr[sel[k]][1],
                    1000*rr[sel[k]][2],fdum);
    else
        for (k = 0; k < nsel; k++)
            fprintf(out,"%d %.2f %.2f %.2f %g\n",sel[k],fdum,fdum,fdum,fdum);
    res = OK;

out : {
        if (out)
            fclose(out);
        if (res != OK)
            unlink(label);
        return res;
    }
}


//*************************************************************************************************************

void MneSurfaceOrVolume::mne_add_triangle_data(MneSourceSpaceOld* s)
/*
    * Add the triangle data structures
    */
{
    int k;
    MneTriangle* tri;

    if (!s || s->type != MNE_SOURCE_SPACE_SURFACE)
        return;

    FREE_17(s->tris);     s->tris = NULL;
    FREE_17(s->use_tris); s->use_tris = NULL;
    /*
        * Add information for the complete triangulation
        */
    if (s->itris && s->ntri > 0) {
        s->tris = MALLOC_17(s->ntri,MneTriangle);
        s->tot_area = 0.0;
        for (k = 0, tri = s->tris; k < s->ntri; k++, tri++) {
            tri->vert = s->itris[k];
            tri->r1   = s->rr[tri->vert[0]];
            tri->r2   = s->rr[tri->vert[1]];
            tri->r3   = s->rr[tri->vert[2]];
            MneTriangle::add_triangle_data(tri);
            s->tot_area += tri->area;
        }
#ifdef TRIANGLE_SIZE_WARNING
        for (k = 0, tri = s->tris; k < s->ntri; k++, tri++)
            if (tri->area < 1e-5*s->tot_area/s->ntri)
                printf("Warning: Triangle area is only %g um^2 (%.5f %% of expected average)\n",
                       1e12*tri->area,100*s->ntri*tri->area/s->tot_area);
#endif
    }
#ifdef DEBUG
    printf("\ttotal area = %-.1f cm^2\n",1e4*s->tot_area);
#endif
    /*
       * Add information for the selected subset if applicable
       */
    if (s->use_itris && s->nuse_tri > 0) {
        s->use_tris = MALLOC_17(s->nuse_tri,MneTriangle);
        for (k = 0, tri = s->use_tris; k < s->nuse_tri; k++, tri++) {
            tri->vert = s->use_itris[k];
            tri->r1   = s->rr[tri->vert[0]];
            tri->r2   = s->rr[tri->vert[1]];
            tri->r3   = s->rr[tri->vert[2]];
            MneTriangle::add_triangle_data(tri);
        }
    }
    return;
}


//*************************************************************************************************************

void MneSurfaceOrVolume::mne_compute_cm(float **rr, int np, float *cm)
/*
* Compute the center of mass of a set of points
*/
{
    int q;
    cm[0] = cm[1] = cm[2] = 0.0;
    for (q = 0; q < np; q++) {
        cm[0] += rr[q][0];
        cm[1] += rr[q][1];
        cm[2] += rr[q][2];
    }
    if (np > 0) {
        cm[0] = cm[0]/np;
        cm[1] = cm[1]/np;
        cm[2] = cm[2]/np;
    }
    return;
}


//*************************************************************************************************************

void MneSurfaceOrVolume::mne_compute_surface_cm(MneSurfaceOld *s)
/*
     * Compute the center of mass of a surface
     */
{
    if (!s)
        return;

    mne_compute_cm(s->rr,s->np,s->cm);
    return;
}


//*************************************************************************************************************

void MneSurfaceOrVolume::calculate_vertex_distances(MneSourceSpaceOld* s)
{
    int   k,p,ndist;
    float *dist,diff[3];
    int   *neigh, nneigh;

    if (!s->neighbor_vert || !s->nneighbor_vert)
        return;

    if (s->vert_dist) {
        for (k = 0; k < s->np; k++)
            FREE_17(s->vert_dist[k]);
        FREE_17(s->vert_dist);
    }
    s->vert_dist = MALLOC_17(s->np,float *);
    printf("\tDistances between neighboring vertices...");
    for (k = 0, ndist = 0; k < s->np; k++) {
        s->vert_dist[k]  = dist = MALLOC_17(s->nneighbor_vert[k],float);
        neigh  = s->neighbor_vert[k];
        nneigh = s->nneighbor_vert[k];
        for (p = 0; p < nneigh; p++) {
            if (neigh[p] >= 0) {
                VEC_DIFF_17(s->rr[k],s->rr[neigh[p]],diff);
                dist[p] = VEC_LEN_17(diff);
            }
            else
                dist[p] = -1.0;
            ndist++;
        }
    }
    printf("[%d distances done]\n",ndist);
    return;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_add_vertex_normals(MneSourceSpaceOld* s)
{
    int k,c,p;
    int *ii;
    float w,size;
    MneTriangle* tri;

    if (!s || s->type != MNE_SOURCE_SPACE_SURFACE)
        return OK;
    /*
       * Reallocate the stuff and initialize
       */
    FREE_CMATRIX_17(s->nn);
    s->nn = ALLOC_CMATRIX_17(s->np,3);

    for (k = 0; k < s->np; k++) {
        s->nn[k][X_17] = s->nn[k][Y_17] = s->nn[k][Z_17] = 0.0;
    }
    /*
       * One pass through the triangles will do it
       */
    MneSurfaceOrVolume::mne_add_triangle_data(s);
    for (p = 0, tri = s->tris; p < s->ntri; p++, tri++) {
        ii = tri->vert;
        w = 1.0;			/* This should be related to the triangle size */
        /*
         * Then the vertex normals
         */
        for (k = 0; k < 3; k++)
            for (c = 0; c < 3; c++)
                s->nn[ii[k]][c] += w*tri->nn[c];
    }
    for (k = 0; k < s->np; k++) {
        size = VEC_LEN_17(s->nn[k]);
        if (size > 0.0)
            for (c = 0; c < 3; c++)
                s->nn[k][c] = s->nn[k][c]/size;
    }
    mne_compute_surface_cm((MneSurfaceOld*)s);
    return OK;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::add_geometry_info(MneSourceSpaceOld* s, int do_normals, int *border, int check_too_many_neighbors)
/*
          * Add vertex normals and neighbourhood information
          */
{
    int k,c,p,q;
    int vert;
    int *ii;
    int *neighbors,nneighbors;
    float w,size;
    int   found;
    int   nfix_distinct,nfix_no_neighbors,nfix_defect;
    MneTriangle* tri;

    if (!s)
        return OK;

    if (s->type == MNE_SOURCE_SPACE_VOLUME) {
        calculate_vertex_distances(s);
        return OK;
    }
    if (s->type != MNE_SOURCE_SPACE_SURFACE)
        return OK;
    /*
       * Reallocate the stuff and initialize
       */
    if (do_normals) {
        FREE_CMATRIX_17(s->nn);
        s->nn = ALLOC_CMATRIX_17(s->np,3);
    }
    if (s->neighbor_tri) {
        for (k = 0; k < s->np; k++)
            FREE_17(s->neighbor_tri[k]);
        FREE_17(s->neighbor_tri);
    }
    FREE_17(s->nneighbor_tri);
    s->neighbor_tri = MALLOC_17(s->np,int *);
    s->nneighbor_tri = MALLOC_17(s->np,int);

    for (k = 0; k < s->np; k++) {
        s->neighbor_tri[k] = NULL;
        s->nneighbor_tri[k] = 0;
        if (do_normals)
            s->nn[k][X_17] = s->nn[k][Y_17] = s->nn[k][Z_17] = 0.0;
    }
    /*
       * One pass through the triangles will do it
       */
    mne_add_triangle_data(s);
    for (p = 0, tri = s->tris; p < s->ntri; p++, tri++)
        if (tri->area == 0)
            printf("\tWarning : zero size triangle # %d\n",p);
    printf("\tTriangle ");
    if (do_normals)
        printf("and vertex ");
    printf("normals and neighboring triangles...");
    for (p = 0, tri = s->tris; p < s->ntri; p++, tri++) {
        ii = tri->vert;
        w = 1.0;			/* This should be related to the triangle size */
        for (k = 0; k < 3; k++) {
            /*
           * Then the vertex normals
           */
            if (do_normals)
                for (c = 0; c < 3; c++)
                    s->nn[ii[k]][c] += w*tri->nn[c];
            /*
           * Add to the list of neighbors
           */
            s->neighbor_tri[ii[k]] = REALLOC_17(s->neighbor_tri[ii[k]],
                    s->nneighbor_tri[ii[k]]+1,int);
            s->neighbor_tri[ii[k]][s->nneighbor_tri[ii[k]]] = p;
            s->nneighbor_tri[ii[k]]++;
        }
    }
    nfix_no_neighbors = 0;
    nfix_defect = 0;
    for (k = 0; k < s->np; k++) {
        if (s->nneighbor_tri[k] <= 0) {
            if (!border || !border[k]) {
#ifdef STRICT_ERROR
                err_printf_set_error("Vertex %d does not have any neighboring triangles!",k);
                return FAIL;
#else
#ifdef REPORT_WARNINGS
                printf("Warning: Vertex %d does not have any neighboring triangles!\n",k);
#endif
#endif
                nfix_no_neighbors++;
            }
        }
        else if (s->nneighbor_tri[k] < 3 && !border) {
#ifdef REPORT_WARNINGS
            printf("\n\tTopological defect: Vertex %d has only %d neighboring triangle%s Vertex omitted.\n\t",
                   k,s->nneighbor_tri[k],s->nneighbor_tri[k] > 1 ? "s." : ".");
#endif
            nfix_defect++;
            s->nneighbor_tri[k] = 0;
            FREE_17(s->neighbor_tri[k]);
            s->neighbor_tri[k] = NULL;
        }
    }
    /*
       * Scale the vertex normals to unit length
       */
    for (k = 0; k < s->np; k++)
        if (s->nneighbor_tri[k] > 0) {
            size = VEC_LEN_17(s->nn[k]);
            if (size > 0.0)
                for (c = 0; c < 3; c++)
                    s->nn[k][c] = s->nn[k][c]/size;
        }
    printf("[done]\n");
    /*
       * Determine the neighboring vertices
       */
    printf("\tVertex neighbors...");
    if (s->neighbor_vert) {
        for (k = 0; k < s->np; k++)
            FREE_17(s->neighbor_vert[k]);
        FREE_17(s->neighbor_vert);
    }
    FREE_17(s->nneighbor_vert);
    s->neighbor_vert = MALLOC_17(s->np,int *);
    s->nneighbor_vert = MALLOC_17(s->np,int);
    /*
       * We know the number of neighbors beforehand
       */
    if (border) {
        for (k = 0; k < s->np; k++) {
            if (s->nneighbor_tri[k] > 0) {
                if (border[k]) {
                    s->neighbor_vert[k]  = MALLOC_17(s->nneighbor_tri[k]+1,int);
                    s->nneighbor_vert[k] = s->nneighbor_tri[k]+1;
                }
                else {
                    s->neighbor_vert[k]  = MALLOC_17(s->nneighbor_tri[k],int);
                    s->nneighbor_vert[k] = s->nneighbor_tri[k];
                }
            }
            else {
                s->neighbor_vert[k]  = NULL;
                s->nneighbor_vert[k] = 0;
            }
        }
    }
    else {
        for (k = 0; k < s->np; k++) {
            if (s->nneighbor_tri[k] > 0) {
                s->neighbor_vert[k]  = MALLOC_17(s->nneighbor_tri[k],int);
                s->nneighbor_vert[k] = s->nneighbor_tri[k];
            }
            else {
                s->neighbor_vert[k]  = NULL;
                s->nneighbor_vert[k] = 0;
            }
        }
    }
    nfix_distinct = 0;
    for (k = 0; k < s->np; k++) {
        neighbors  = s->neighbor_vert[k];
        nneighbors = 0;
        for (p = 0; p < s->nneighbor_tri[k]; p++) {
            /*
           * Fit in the other vertices of the neighboring triangle
           */
            for (c = 0; c < 3; c++) {
                vert = s->tris[s->neighbor_tri[k][p]].vert[c];
                if (vert != k) {
                    for (q = 0, found = FALSE; q < nneighbors; q++) {
                        if (neighbors[q] == vert) {
                            found = TRUE;
                            break;
                        }
                    }
                    if (!found) {
                        if (nneighbors < s->nneighbor_vert[k])
                            neighbors[nneighbors++] = vert;
                        else if (!border || !border[k]) {
                            if (check_too_many_neighbors) {
                                printf("Too many neighbors for vertex %d.",k);
                                return FAIL;
                            }
                            else
                                printf("\tWarning: Too many neighbors for vertex %d\n",k);
                        }
                    }
                }
            }
        }
        if (nneighbors != s->nneighbor_vert[k]) {
#ifdef REPORT_WARNINGS
            printf("\n\tIncorrect number of distinct neighbors for vertex %d (%d instead of %d) [fixed].",
                   k,nneighbors,s->nneighbor_vert[k]);
#endif
            nfix_distinct++;
            s->nneighbor_vert[k] = nneighbors;
        }
    }
    printf("[done]\n");
    /*
       * Distance calculation follows
       */
    calculate_vertex_distances(s);
    mne_compute_surface_cm((MneSurfaceOld*)s);
    /*
       * Summarize the defects
       */
    if (nfix_defect > 0)
        printf("\tWarning: %d topological defects were fixed.\n",nfix_defect);
    if (nfix_distinct > 0)
        printf("\tWarning: %d vertices had incorrect number of distinct neighbors (fixed).\n",nfix_distinct);
    if (nfix_no_neighbors > 0)
        printf("\tWarning: %d vertices did not have any neighboring triangles (fixed)\n",nfix_no_neighbors);
#ifdef DEBUG
    for (k = 0; k < s->np; k++) {
        if (s->nneighbor_vert[k] <= 0)
            printf("No neighbors for vertex %d\n",k);
        if (s->nneighbor_tri[k] <= 0)
            printf("No neighbor tris for vertex %d\n",k);
    }
#endif
    return OK;
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_source_space_add_geometry_info(MneSourceSpaceOld* s, int do_normals)
{
    return add_geometry_info(s,do_normals,NULL,TRUE);
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_source_space_add_geometry_info2(MneSourceSpaceOld* s, int do_normals)

{
    return add_geometry_info(s,do_normals,NULL,FALSE);
}


//*************************************************************************************************************

int MneSurfaceOrVolume::mne_label_area(char *label, MneSourceSpaceOld* s, float *areap)     /* Return the area here */
/*
     * Calculate the area of the label
     */
{
    int *sel = NULL;
    int nsel = 0;
    float area;
    int   k,q,nneigh,*neigh;

    if (!s) {
        qCritical("Source space not specified for mne_label_area");
        goto bad;
    }
    if (mne_read_label(label,NULL,&sel,&nsel))
        goto bad;

    area = 0.0;
    for (k = 0; k < nsel; k++) {
        if (sel[k] < 0 || sel[k] >= s->np) {
            qCritical("Label vertex index out of range in mne_label_area");
            goto bad;
        }
        nneigh = s->nneighbor_tri[sel[k]];
        neigh  = s->neighbor_tri[sel[k]];
        for (q = 0; q < nneigh; q++)
            area += s->tris[neigh[q]].area/3.0;
    }
    FREE_17(sel);
    *areap = area;
    return OK;

bad : {
        FREE_17(sel);
        return FAIL;
    }
}
