/**CFile****************************************************************

  FileName    [cgtInt.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Clock gating package.]

  Synopsis    [Internal declarations.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: cgtInt.h,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/
 
#ifndef __CGT_INT_H__
#define __CGT_INT_H__

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include "saig.h"
#include "satSolver.h"
#include "cnf.h"
#include "cgt.h"

////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Cgt_Man_t_ Cgt_Man_t;
struct Cgt_Man_t_
{
    // user's data
    Cgt_Par_t *  pPars;          // user's parameters
    Aig_Man_t *  pAig;           // user's AIG manager
    Aig_Man_t *  pCare;          // user's constraints
    Vec_Vec_t *  vGatesAll;      // the computed clock-gates
    Vec_Ptr_t *  vGates;         // the selected clock-gates
    // internal data
    Aig_Man_t *  pFrame;         // clock gate AIG manager
    Vec_Ptr_t *  vFanout;        // temporary storage for fanouts
    // SAT solving
    Aig_Man_t *  pPart;          // partition
    Cnf_Dat_t *  pCnf;           // CNF of the partition
    sat_solver * pSat;           // SAT solver 
    Vec_Ptr_t *  vPatts;         // simulation patterns
    int          nPatts;         // the number of patterns accumulated
    int          nPattWords;     // the number of pattern words
    // statistics
    int          nCalls;         // total calls
    int          nCallsSat;      // satisfiable calls
    int          nCallsUnsat;    // unsatisfiable calls  
    int          nCallsUndec;    // undecided calls
    int          timeSat;        // total runtime
    int          timeSatSat;     // satisfiable runtime 
    int          timeSatUnsat;   // unsatisfiable runtime 
    int          timeSatUndec;   // undecided runtime
};

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== cgtAig.c ==========================================================*/
extern void             Cgt_ManDetectCandidates( Aig_Man_t * pAig, Aig_Obj_t * pObj, int nLevelMax, Vec_Ptr_t * vCands );
extern Aig_Man_t *      Cgt_ManDeriveAigForGating( Cgt_Man_t * p );
extern Aig_Man_t *      Cgt_ManDupPartition( Aig_Man_t * pAig, int nVarsMin, int nFlopsMin, int iStart );
extern Aig_Man_t *      Cgt_ManDeriveGatedAig( Aig_Man_t * pAig, Vec_Ptr_t * vGates );
/*=== cgtDecide.c ==========================================================*/
extern Vec_Ptr_t *      Cgt_ManDecide( Aig_Man_t * pAig, Vec_Vec_t * vGatesAll );
extern Vec_Ptr_t *      Cgt_ManDecideSimple( Aig_Man_t * pAig, Vec_Vec_t * vGatesAll );
/*=== cgtMan.c ==========================================================*/
extern Cgt_Man_t *      Cgt_ManCreate( Aig_Man_t * pAig, Aig_Man_t * pCare, Cgt_Par_t * pPars );
extern void             Cgt_ManClean( Cgt_Man_t * p );
extern void             Cgt_ManStop( Cgt_Man_t * p );
/*=== cgtSat.c ==========================================================*/
extern int              Cgt_CheckImplication( Cgt_Man_t * p, Aig_Obj_t * pGate, Aig_Obj_t * pFlop );

#ifdef __cplusplus
}
#endif

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////

