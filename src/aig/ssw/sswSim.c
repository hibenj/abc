/**CFile****************************************************************

  FileName    [sswSim.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Inductive prover with constraints.]

  Synopsis    [Sequential simulator used by the inductive prover.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - September 1, 2008.]

  Revision    [$Id: sswSim.c,v 1.00 2008/09/01 00:00:00 alanmi Exp $]

***********************************************************************/

#include "sswInt.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

// simulation manager
struct Ssw_Sml_t_
{
    Aig_Man_t *      pAig;              // the original AIG manager
    int              nPref;             // the number of timeframes in the prefix
    int              nFrames;           // the number of timeframes 
    int              nWordsFrame;       // the number of words in each timeframe
    int              nWordsTotal;       // the total number of words at a node
    int              nWordsPref;        // the number of word in the prefix
    int              fNonConstOut;      // have seen a non-const-0 output during simulation
    int              nSimRounds;        // statistics
    int              timeSim;           // statistics
    unsigned         pData[0];          // simulation data for the nodes
};

static inline unsigned * Ssw_ObjSim( Ssw_Sml_t * p, int Id )  { return p->pData + p->nWordsTotal * Id; }
static inline unsigned   Ssw_ObjRandomSim()                   { return Aig_ManRandom(0);               }

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Computes hash value of the node using its simulation info.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
unsigned Ssw_SmlObjHashWord( Ssw_Sml_t * p, Aig_Obj_t * pObj )
{
    static int s_SPrimes[128] = { 
        1009, 1049, 1093, 1151, 1201, 1249, 1297, 1361, 1427, 1459, 
        1499, 1559, 1607, 1657, 1709, 1759, 1823, 1877, 1933, 1997, 
        2039, 2089, 2141, 2213, 2269, 2311, 2371, 2411, 2467, 2543, 
        2609, 2663, 2699, 2741, 2797, 2851, 2909, 2969, 3037, 3089, 
        3169, 3221, 3299, 3331, 3389, 3461, 3517, 3557, 3613, 3671, 
        3719, 3779, 3847, 3907, 3943, 4013, 4073, 4129, 4201, 4243, 
        4289, 4363, 4441, 4493, 4549, 4621, 4663, 4729, 4793, 4871, 
        4933, 4973, 5021, 5087, 5153, 5227, 5281, 5351, 5417, 5471, 
        5519, 5573, 5651, 5693, 5749, 5821, 5861, 5923, 6011, 6073, 
        6131, 6199, 6257, 6301, 6353, 6397, 6481, 6563, 6619, 6689, 
        6737, 6803, 6863, 6917, 6977, 7027, 7109, 7187, 7237, 7309, 
        7393, 7477, 7523, 7561, 7607, 7681, 7727, 7817, 7877, 7933, 
        8011, 8039, 8059, 8081, 8093, 8111, 8123, 8147
    };
    unsigned * pSims;
    unsigned uHash;
    int i;
    assert( p->nWordsTotal <= 128 );
    uHash = 0;
    pSims = Ssw_ObjSim(p, pObj->Id);
    for ( i = p->nWordsPref; i < p->nWordsTotal; i++ )
        uHash ^= pSims[i] * s_SPrimes[i & 0x7F];
    return uHash;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if simulation info is composed of all zeros.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlObjIsConstWord( Ssw_Sml_t * p, Aig_Obj_t * pObj )
{
    unsigned * pSims;
    int i;
    pSims = Ssw_ObjSim(p, pObj->Id);
    for ( i = p->nWordsPref; i < p->nWordsTotal; i++ )
        if ( pSims[i] )
            return 0;
    return 1;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if simulation infos are equal.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlObjsAreEqualWord( Ssw_Sml_t * p, Aig_Obj_t * pObj0, Aig_Obj_t * pObj1 )
{
    unsigned * pSims0, * pSims1;
    int i;
    pSims0 = Ssw_ObjSim(p, pObj0->Id);
    pSims1 = Ssw_ObjSim(p, pObj1->Id);
    for ( i = p->nWordsPref; i < p->nWordsTotal; i++ )
        if ( pSims0[i] != pSims1[i] )
            return 0;
    return 1;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the node appears to be constant 1 candidate.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlObjIsConstBit( void * p, Aig_Obj_t * pObj )
{
    return pObj->fPhase == pObj->fMarkB;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the nodes appear equal.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlObjsAreEqualBit( void * p, Aig_Obj_t * pObj0, Aig_Obj_t * pObj1 )
{
    return (pObj0->fPhase == pObj1->fPhase) == (pObj0->fMarkB == pObj1->fMarkB);
}


/**Function*************************************************************

  Synopsis    [Counts the number of 1s in the XOR of simulation data.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlNodeNotEquWeight( Ssw_Sml_t * p, int Left, int Right )
{
    unsigned * pSimL, * pSimR;
    int k, Counter = 0;
    pSimL = Ssw_ObjSim( p, Left );
    pSimR = Ssw_ObjSim( p, Right );
    for ( k = p->nWordsPref; k < p->nWordsTotal; k++ )
        Counter += Aig_WordCountOnes( pSimL[k] ^ pSimR[k] );
    return Counter;
}

/**Function*************************************************************

  Synopsis    [Checks implication.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlCheckXorImplication( Ssw_Sml_t * p, Aig_Obj_t * pObjLi, Aig_Obj_t * pObjLo, Aig_Obj_t * pCand )
{
    unsigned * pSimLi, * pSimLo, * pSimCand;
    int k;
    pSimCand = Ssw_ObjSim( p, Aig_Regular(pCand)->Id );
    pSimLi   = Ssw_ObjSim( p, pObjLi->Id );
    pSimLo   = Ssw_ObjSim( p, pObjLo->Id );
    if ( !Aig_IsComplement(pCand) )
    {
        for ( k = p->nWordsPref; k < p->nWordsTotal; k++ )
            if ( pSimCand[k] & (pSimLi[k] ^ pSimLo[k]) )
                return 0;
    }
    else
    {
        for ( k = p->nWordsPref; k < p->nWordsTotal; k++ )
            if ( ~pSimCand[k] & (pSimLi[k] ^ pSimLo[k]) )
                return 0;
    }
    return 1;
}

/**Function*************************************************************

  Synopsis    [Counts the number of 1s in the implication.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlCountXorImplication( Ssw_Sml_t * p, Aig_Obj_t * pObjLi, Aig_Obj_t * pObjLo, Aig_Obj_t * pCand )
{
    unsigned * pSimLi, * pSimLo, * pSimCand;
    int k, Counter = 0;
    pSimCand = Ssw_ObjSim( p, Aig_Regular(pCand)->Id );
    pSimLi   = Ssw_ObjSim( p, pObjLi->Id );
    pSimLo   = Ssw_ObjSim( p, pObjLo->Id );
    if ( !Aig_IsComplement(pCand) )
    {
        for ( k = p->nWordsPref; k < p->nWordsTotal; k++ )
            Counter += Aig_WordCountOnes(pSimCand[k] & ~(pSimLi[k] ^ pSimLo[k]));
    }
    else
    {
        for ( k = p->nWordsPref; k < p->nWordsTotal; k++ )
            Counter += Aig_WordCountOnes(~pSimCand[k] & ~(pSimLi[k] ^ pSimLo[k]));
    }
    return Counter;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if simulation info is composed of all zeros.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlNodeIsZero( Ssw_Sml_t * p, Aig_Obj_t * pObj )
{
    unsigned * pSims;
    int i;
    pSims = Ssw_ObjSim(p, pObj->Id);
    for ( i = p->nWordsPref; i < p->nWordsTotal; i++ )
        if ( pSims[i] )
            return 0;
    return 1;
}

/**Function*************************************************************

  Synopsis    [Counts the number of one's in the patten of the output.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlNodeCountOnes( Ssw_Sml_t * p, Aig_Obj_t * pObj )
{
    unsigned * pSims;
    int i, Counter = 0;
    pSims = Ssw_ObjSim(p, pObj->Id);
    for ( i = 0; i < p->nWordsTotal; i++ )
        Counter += Aig_WordCountOnes( pSims[i] );
    return Counter;
}



/**Function*************************************************************

  Synopsis    [Generated const 0 pattern.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlSavePattern0( Ssw_Man_t * p, int fInit )
{
    memset( p->pPatWords, 0, sizeof(unsigned) * p->nPatWords );
}

/**Function*************************************************************

  Synopsis    [[Generated const 1 pattern.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlSavePattern1( Ssw_Man_t * p, int fInit )
{
    Aig_Obj_t * pObj;
    int i, k, nTruePis;
    memset( p->pPatWords, 0xff, sizeof(unsigned) * p->nPatWords );
    if ( !fInit )
        return;
    // clear the state bits to correspond to all-0 initial state
    nTruePis = Saig_ManPiNum(p->pAig);
    k = 0;
    Saig_ManForEachLo( p->pAig, pObj, i )
        Aig_InfoXorBit( p->pPatWords, nTruePis * p->nFrames + k++ );
}


/**Function*************************************************************

  Synopsis    [Creates the counter-example from the successful pattern.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int * Ssw_SmlCheckOutputSavePattern( Ssw_Sml_t * p, Aig_Obj_t * pObjPo )
{ 
    Aig_Obj_t * pFanin, * pObjPi;
    unsigned * pSims;
    int i, k, BestPat, * pModel;
    // find the word of the pattern
    pFanin = Aig_ObjFanin0(pObjPo);
    pSims = Ssw_ObjSim(p, pFanin->Id);
    for ( i = 0; i < p->nWordsTotal; i++ )
        if ( pSims[i] )
            break;
    assert( i < p->nWordsTotal );
    // find the bit of the pattern
    for ( k = 0; k < 32; k++ )
        if ( pSims[i] & (1 << k) )
            break;
    assert( k < 32 );
    // determine the best pattern
    BestPat = i * 32 + k;
    // fill in the counter-example data
    pModel = ALLOC( int, Aig_ManPiNum(p->pAig)+1 );
    Aig_ManForEachPi( p->pAig, pObjPi, i )
    {
        pModel[i] = Aig_InfoHasBit(Ssw_ObjSim(p, pObjPi->Id), BestPat);
//        printf( "%d", pModel[i] );
    }
    pModel[Aig_ManPiNum(p->pAig)] = pObjPo->Id;
//    printf( "\n" );
    return pModel;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the one of the output is already non-constant 0.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int * Ssw_SmlCheckOutput( Ssw_Sml_t * p )
{
    Aig_Obj_t * pObj;
    int i;
    // make sure the reference simulation pattern does not detect the bug
    pObj = Aig_ManPo( p->pAig, 0 );
    assert( Aig_ObjFanin0(pObj)->fPhase == (unsigned)Aig_ObjFaninC0(pObj) ); 
    Aig_ManForEachPo( p->pAig, pObj, i )
    {
        if ( !Ssw_SmlObjIsConstWord( p, Aig_ObjFanin0(pObj) ) )
        {
            // create the counter-example from this pattern
            return Ssw_SmlCheckOutputSavePattern( p, pObj );
        }
    }
    return NULL;
}



/**Function*************************************************************

  Synopsis    [Assigns random patterns to the PI node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlAssignRandom( Ssw_Sml_t * p, Aig_Obj_t * pObj )
{
    unsigned * pSims;
    int i, f;
    assert( Aig_ObjIsPi(pObj) );
    pSims = Ssw_ObjSim( p, pObj->Id );
    for ( i = 0; i < p->nWordsTotal; i++ )
        pSims[i] = Ssw_ObjRandomSim();
    // set the first bit 0 in each frame
    assert( p->nWordsFrame * p->nFrames == p->nWordsTotal );
    for ( f = 0; f < p->nFrames; f++ )
        pSims[p->nWordsFrame*f] <<= 1;
}

/**Function*************************************************************

  Synopsis    [Assigns random patterns to the PI node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlAssignRandomFrame( Ssw_Sml_t * p, Aig_Obj_t * pObj, int iFrame )
{
    unsigned * pSims;
    int i;
    assert( iFrame < p->nFrames );
    assert( Aig_ObjIsPi(pObj) );
    pSims = Ssw_ObjSim( p, pObj->Id ) + p->nWordsFrame * iFrame;
    for ( i = 0; i < p->nWordsFrame; i++ )
        pSims[i] = Ssw_ObjRandomSim();
}

/**Function*************************************************************

  Synopsis    [Assigns constant patterns to the PI node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlObjAssignConst( Ssw_Sml_t * p, Aig_Obj_t * pObj, int fConst1, int iFrame )
{
    unsigned * pSims;
    int i;
    assert( iFrame < p->nFrames );
    assert( Aig_ObjIsPi(pObj) );
    pSims = Ssw_ObjSim( p, pObj->Id ) + p->nWordsFrame * iFrame;
    for ( i = 0; i < p->nWordsFrame; i++ )
        pSims[i] = fConst1? ~(unsigned)0 : 0;
} 

/**Function*************************************************************

  Synopsis    [Assigns constant patterns to the PI node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlObjSetWord( Ssw_Sml_t * p, Aig_Obj_t * pObj, unsigned Word, int iWord, int iFrame )
{
    unsigned * pSims;
    assert( iFrame < p->nFrames );
    assert( Aig_ObjIsPi(pObj) );
    pSims = Ssw_ObjSim( p, pObj->Id ) + p->nWordsFrame * iFrame;
    pSims[iWord] = Word;
} 

/**Function*************************************************************

  Synopsis    [Assings distance-1 simulation info for the PIs.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlAssignDist1( Ssw_Sml_t * p, unsigned * pPat )
{
    Aig_Obj_t * pObj;
    int f, i, k, Limit, nTruePis;
    assert( p->nFrames > 0 );
    if ( p->nFrames == 1 )
    {
        // copy the PI info 
        Aig_ManForEachPi( p->pAig, pObj, i )
            Ssw_SmlObjAssignConst( p, pObj, Aig_InfoHasBit(pPat, i), 0 );
        // flip one bit
        Limit = AIG_MIN( Aig_ManPiNum(p->pAig), p->nWordsTotal * 32 - 1 );
        for ( i = 0; i < Limit; i++ )
            Aig_InfoXorBit( Ssw_ObjSim( p, Aig_ManPi(p->pAig,i)->Id ), i+1 );
    }
    else
    {
        int fUseDist1 = 0;

        // copy the PI info for each frame
        nTruePis = Aig_ManPiNum(p->pAig) - Aig_ManRegNum(p->pAig);
        for ( f = 0; f < p->nFrames; f++ )
            Saig_ManForEachPi( p->pAig, pObj, i )
                Ssw_SmlObjAssignConst( p, pObj, Aig_InfoHasBit(pPat, nTruePis * f + i), f );
        // copy the latch info 
        k = 0;
        Saig_ManForEachLo( p->pAig, pObj, i )
            Ssw_SmlObjAssignConst( p, pObj, Aig_InfoHasBit(pPat, nTruePis * p->nFrames + k++), 0 );
//        assert( p->pFrames == NULL || nTruePis * p->nFrames + k == Aig_ManPiNum(p->pFrames) );

        // flip one bit of the last frame
        if ( fUseDist1 ) //&& p->nFrames == 2 )
        {
            Limit = AIG_MIN( nTruePis, p->nWordsFrame * 32 - 1 );
            for ( i = 0; i < Limit; i++ )
                Aig_InfoXorBit( Ssw_ObjSim( p, Aig_ManPi(p->pAig, i)->Id ) + p->nWordsFrame*(p->nFrames-1), i+1 );
        }
    }
}

/**Function*************************************************************

  Synopsis    [Assings distance-1 simulation info for the PIs.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlAssignDist1Plus( Ssw_Sml_t * p, unsigned * pPat )
{
    Aig_Obj_t * pObj;
    int f, i, Limit;
    assert( p->nFrames > 0 );

    // copy the pattern into the primary inputs
    Aig_ManForEachPi( p->pAig, pObj, i )
        Ssw_SmlObjAssignConst( p, pObj, Aig_InfoHasBit(pPat, i), 0 );

    // set distance one PIs for the first frame
    Limit = AIG_MIN( Saig_ManPiNum(p->pAig), p->nWordsFrame * 32 - 1 );
    for ( i = 0; i < Limit; i++ )
        Aig_InfoXorBit( Ssw_ObjSim( p, Aig_ManPi(p->pAig, i)->Id ), i+1 );

    // create random info for the remaining timeframes
    for ( f = 1; f < p->nFrames; f++ )
        Saig_ManForEachPi( p->pAig, pObj, i )
            Ssw_SmlAssignRandomFrame( p, pObj, f );
}

/**Function*************************************************************

  Synopsis    [Simulates one node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlNodeSimulate( Ssw_Sml_t * p, Aig_Obj_t * pObj, int iFrame )
{
    unsigned * pSims, * pSims0, * pSims1;
    int fCompl, fCompl0, fCompl1, i;
    assert( iFrame < p->nFrames );
    assert( !Aig_IsComplement(pObj) );
    assert( Aig_ObjIsNode(pObj) );
    assert( iFrame == 0 || p->nWordsFrame < p->nWordsTotal );
    // get hold of the simulation information
    pSims  = Ssw_ObjSim(p, pObj->Id) + p->nWordsFrame * iFrame;
    pSims0 = Ssw_ObjSim(p, Aig_ObjFanin0(pObj)->Id) + p->nWordsFrame * iFrame;
    pSims1 = Ssw_ObjSim(p, Aig_ObjFanin1(pObj)->Id) + p->nWordsFrame * iFrame;
    // get complemented attributes of the children using their random info
    fCompl  = pObj->fPhase;
    fCompl0 = Aig_ObjPhaseReal(Aig_ObjChild0(pObj));
    fCompl1 = Aig_ObjPhaseReal(Aig_ObjChild1(pObj));
    // simulate
    if ( fCompl0 && fCompl1 )
    {
        if ( fCompl )
            for ( i = 0; i < p->nWordsFrame; i++ )
                pSims[i] = (pSims0[i] | pSims1[i]);
        else
            for ( i = 0; i < p->nWordsFrame; i++ )
                pSims[i] = ~(pSims0[i] | pSims1[i]);
    }
    else if ( fCompl0 && !fCompl1 )
    {
        if ( fCompl )
            for ( i = 0; i < p->nWordsFrame; i++ )
                pSims[i] = (pSims0[i] | ~pSims1[i]);
        else
            for ( i = 0; i < p->nWordsFrame; i++ )
                pSims[i] = (~pSims0[i] & pSims1[i]);
    }
    else if ( !fCompl0 && fCompl1 )
    {
        if ( fCompl )
            for ( i = 0; i < p->nWordsFrame; i++ )
                pSims[i] = (~pSims0[i] | pSims1[i]);
        else
            for ( i = 0; i < p->nWordsFrame; i++ )
                pSims[i] = (pSims0[i] & ~pSims1[i]);
    }
    else // if ( !fCompl0 && !fCompl1 )
    {
        if ( fCompl )
            for ( i = 0; i < p->nWordsFrame; i++ )
                pSims[i] = ~(pSims0[i] & pSims1[i]);
        else
            for ( i = 0; i < p->nWordsFrame; i++ )
                pSims[i] = (pSims0[i] & pSims1[i]);
    }
}

/**Function*************************************************************

  Synopsis    [Simulates one node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlNodesCompareInFrame( Ssw_Sml_t * p, Aig_Obj_t * pObj0, Aig_Obj_t * pObj1, int iFrame0, int iFrame1 )
{
    unsigned * pSims0, * pSims1;
    int i;
    assert( iFrame0 < p->nFrames );
    assert( iFrame1 < p->nFrames );
    assert( !Aig_IsComplement(pObj0) );
    assert( !Aig_IsComplement(pObj1) );
    assert( iFrame0 == 0 || p->nWordsFrame < p->nWordsTotal );
    assert( iFrame1 == 0 || p->nWordsFrame < p->nWordsTotal );
    // get hold of the simulation information
    pSims0  = Ssw_ObjSim(p, pObj0->Id) + p->nWordsFrame * iFrame0;
    pSims1  = Ssw_ObjSim(p, pObj1->Id) + p->nWordsFrame * iFrame1;
    // compare
    for ( i = 0; i < p->nWordsFrame; i++ )
        if ( pSims0[i] != pSims1[i] )
            return 0;
    return 1;
}

/**Function*************************************************************

  Synopsis    [Simulates one node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlNodeCopyFanin( Ssw_Sml_t * p, Aig_Obj_t * pObj, int iFrame )
{
    unsigned * pSims, * pSims0;
    int fCompl, fCompl0, i;
    assert( iFrame < p->nFrames );
    assert( !Aig_IsComplement(pObj) );
    assert( Aig_ObjIsPo(pObj) );
    assert( iFrame == 0 || p->nWordsFrame < p->nWordsTotal );
    // get hold of the simulation information
    pSims  = Ssw_ObjSim(p, pObj->Id) + p->nWordsFrame * iFrame;
    pSims0 = Ssw_ObjSim(p, Aig_ObjFanin0(pObj)->Id) + p->nWordsFrame * iFrame;
    // get complemented attributes of the children using their random info
    fCompl  = pObj->fPhase;
    fCompl0 = Aig_ObjPhaseReal(Aig_ObjChild0(pObj));
    // copy information as it is
    if ( fCompl0 )
        for ( i = 0; i < p->nWordsFrame; i++ )
            pSims[i] = ~pSims0[i];
    else
        for ( i = 0; i < p->nWordsFrame; i++ )
            pSims[i] = pSims0[i];
}

/**Function*************************************************************

  Synopsis    [Simulates one node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlNodeTransferNext( Ssw_Sml_t * p, Aig_Obj_t * pOut, Aig_Obj_t * pIn, int iFrame )
{
    unsigned * pSims0, * pSims1;
    int i;
    assert( iFrame < p->nFrames );
    assert( !Aig_IsComplement(pOut) );
    assert( !Aig_IsComplement(pIn) );
    assert( Aig_ObjIsPo(pOut) );
    assert( Aig_ObjIsPi(pIn) );
    assert( iFrame == 0 || p->nWordsFrame < p->nWordsTotal );
    // get hold of the simulation information
    pSims0 = Ssw_ObjSim(p, pOut->Id) + p->nWordsFrame * iFrame;
    pSims1 = Ssw_ObjSim(p, pIn->Id) + p->nWordsFrame * (iFrame+1);
    // copy information as it is
    for ( i = 0; i < p->nWordsFrame; i++ )
        pSims1[i] = pSims0[i];
}

/**Function*************************************************************

  Synopsis    [Simulates one node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlNodeTransferFirst( Ssw_Sml_t * p, Aig_Obj_t * pOut, Aig_Obj_t * pIn )
{
    unsigned * pSims0, * pSims1;
    int i;
    assert( !Aig_IsComplement(pOut) );
    assert( !Aig_IsComplement(pIn) );
    assert( Aig_ObjIsPo(pOut) );
    assert( Aig_ObjIsPi(pIn) );
    assert( p->nWordsFrame < p->nWordsTotal );
    // get hold of the simulation information
    pSims0 = Ssw_ObjSim(p, pOut->Id) + p->nWordsFrame * (p->nFrames-1);
    pSims1 = Ssw_ObjSim(p, pIn->Id);
    // copy information as it is
    for ( i = 0; i < p->nWordsFrame; i++ )
        pSims1[i] = pSims0[i];
}


/**Function*************************************************************

  Synopsis    [Assings random simulation info for the PIs.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlInitialize( Ssw_Sml_t * p, int fInit )
{
    Aig_Obj_t * pObj;
    int i;
    if ( fInit )
    {
        assert( Aig_ManRegNum(p->pAig) > 0 );
        assert( Aig_ManRegNum(p->pAig) < Aig_ManPiNum(p->pAig) );
        // assign random info for primary inputs
        Saig_ManForEachPi( p->pAig, pObj, i )
            Ssw_SmlAssignRandom( p, pObj );
        // assign the initial state for the latches
        Saig_ManForEachLo( p->pAig, pObj, i )
            Ssw_SmlObjAssignConst( p, pObj, 0, 0 );
    }
    else
    {
        Aig_ManForEachPi( p->pAig, pObj, i )
            Ssw_SmlAssignRandom( p, pObj );
    }
}

/**Function*************************************************************

  Synopsis    [Assings random simulation info for the PIs.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlReinitialize( Ssw_Sml_t * p )
{
    Aig_Obj_t * pObj, * pObjLi, * pObjLo;
    int i;
    assert( Aig_ManRegNum(p->pAig) > 0 );
    assert( Aig_ManRegNum(p->pAig) < Aig_ManPiNum(p->pAig) );
    // assign random info for primary inputs
    Saig_ManForEachPi( p->pAig, pObj, i )
        Ssw_SmlAssignRandom( p, pObj );
    // copy simulation info into the inputs
    Saig_ManForEachLiLo( p->pAig, pObjLi, pObjLo, i )
        Ssw_SmlNodeTransferFirst( p, pObjLi, pObjLo );
}

/**Function*************************************************************

  Synopsis    [Check if any of the POs becomes non-constant.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlCheckNonConstOutputs( Ssw_Sml_t * p )
{
    Aig_Obj_t * pObj;
    int i;
    Saig_ManForEachPo( p->pAig, pObj, i )
        if ( !Ssw_SmlNodeIsZero(p, pObj) )
            return 1;
    return 0;
}

/**Function*************************************************************

  Synopsis    [Simulates AIG manager.]

  Description [Assumes that the PI simulation info is attached.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlSimulateOne( Ssw_Sml_t * p )
{
    Aig_Obj_t * pObj, * pObjLi, * pObjLo;
    int f, i, clk;
clk = clock();
    for ( f = 0; f < p->nFrames; f++ )
    {
        // simulate the nodes
        Aig_ManForEachNode( p->pAig, pObj, i )
            Ssw_SmlNodeSimulate( p, pObj, f );
        // copy simulation info into outputs
        Saig_ManForEachPo( p->pAig, pObj, i )
            Ssw_SmlNodeCopyFanin( p, pObj, f );
        // copy simulation info into outputs
        Saig_ManForEachLi( p->pAig, pObj, i )
            Ssw_SmlNodeCopyFanin( p, pObj, f );
        // quit if this is the last timeframe
        if ( f == p->nFrames - 1 )
            break;
        // copy simulation info into the inputs
        Saig_ManForEachLiLo( p->pAig, pObjLi, pObjLo, i )
            Ssw_SmlNodeTransferNext( p, pObjLi, pObjLo, f );
    }
p->timeSim += clock() - clk;
p->nSimRounds++;
}

/**Function*************************************************************

  Synopsis    [Simulates AIG manager.]

  Description [Assumes that the PI simulation info is attached.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlSimulateOneFrame( Ssw_Sml_t * p )
{
    Aig_Obj_t * pObj, * pObjLi, * pObjLo;
    int i, clk;
clk = clock();
    // simulate the nodes
    Aig_ManForEachNode( p->pAig, pObj, i )
        Ssw_SmlNodeSimulate( p, pObj, 0 );
    // copy simulation info into outputs
    Saig_ManForEachLi( p->pAig, pObj, i )
        Ssw_SmlNodeCopyFanin( p, pObj, 0 );
    // copy simulation info into the inputs
    Saig_ManForEachLiLo( p->pAig, pObjLi, pObjLo, i )
        Ssw_SmlNodeTransferNext( p, pObjLi, pObjLo, 0 );
p->timeSim += clock() - clk;
p->nSimRounds++;
}


/**Function*************************************************************

  Synopsis    [Allocates simulation manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Ssw_Sml_t * Ssw_SmlStart( Aig_Man_t * pAig, int nPref, int nFrames, int nWordsFrame )
{
    Ssw_Sml_t * p;
    p = (Ssw_Sml_t *)malloc( sizeof(Ssw_Sml_t) + sizeof(unsigned) * Aig_ManObjNumMax(pAig) * (nPref + nFrames) * nWordsFrame );
    memset( p, 0, sizeof(Ssw_Sml_t) + sizeof(unsigned) * (nPref + nFrames) * nWordsFrame );
    p->pAig        = pAig;
    p->nPref       = nPref;
    p->nFrames     = nPref + nFrames;
    p->nWordsFrame = nWordsFrame;
    p->nWordsTotal = (nPref + nFrames) * nWordsFrame;
    p->nWordsPref  = nPref * nWordsFrame;
    return p;
}

/**Function*************************************************************

  Synopsis    [Allocates simulation manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlClean( Ssw_Sml_t * p )
{
    memset( p->pData, 0, sizeof(unsigned) * Aig_ManObjNumMax(p->pAig) * p->nWordsTotal );
}

/**Function*************************************************************

  Synopsis    [Deallocates simulation manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlStop( Ssw_Sml_t * p )
{
    free( p );
}

/**Function*************************************************************

  Synopsis    [Deallocates simulation manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlNumFrames( Ssw_Sml_t * p )
{
    return p->nFrames;
}


/**Function*************************************************************

  Synopsis    [Performs simulation of the uninitialized circuit.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Ssw_Sml_t * Ssw_SmlSimulateComb( Aig_Man_t * pAig, int nWords )
{
    Ssw_Sml_t * p;
    p = Ssw_SmlStart( pAig, 0, 1, nWords );
    Ssw_SmlInitialize( p, 0 );
    Ssw_SmlSimulateOne( p );
    return p;
}

/**Function*************************************************************

  Synopsis    [Performs simulation of the initialized circuit.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Ssw_Sml_t * Ssw_SmlSimulateSeq( Aig_Man_t * pAig, int nPref, int nFrames, int nWords )
{
    Ssw_Sml_t * p;
    p = Ssw_SmlStart( pAig, nPref, nFrames, nWords );
    Ssw_SmlInitialize( p, 1 );
    Ssw_SmlSimulateOne( p );
    p->fNonConstOut = Ssw_SmlCheckNonConstOutputs( p );
    return p;
}

/**Function*************************************************************

  Synopsis    [Performs next round of sequential simulation.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlResimulateSeq( Ssw_Sml_t * p )
{
    Ssw_SmlReinitialize( p );
    Ssw_SmlSimulateOne( p );
    p->fNonConstOut = Ssw_SmlCheckNonConstOutputs( p );
}



/**Function*************************************************************

  Synopsis    [Allocates a counter-example.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Ssw_Cex_t * Ssw_SmlAllocCounterExample( int nRegs, int nRealPis, int nFrames )
{
    Ssw_Cex_t * pCex;
    int nWords = Aig_BitWordNum( nRegs + nRealPis * nFrames );
    pCex = (Ssw_Cex_t *)malloc( sizeof(Ssw_Cex_t) + sizeof(unsigned) * nWords );
    memset( pCex, 0, sizeof(Ssw_Cex_t) + sizeof(unsigned) * nWords );
    pCex->nRegs  = nRegs;
    pCex->nPis   = nRealPis;
    pCex->nBits  = nRegs + nRealPis * nFrames;
    return pCex;
}

/**Function*************************************************************

  Synopsis    [Frees the counter-example.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Ssw_SmlFreeCounterExample( Ssw_Cex_t * pCex )
{
    free( pCex );
}

/**Function*************************************************************

  Synopsis    [Resimulates the counter-example.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlRunCounterExample( Aig_Man_t * pAig, Ssw_Cex_t * p )
{
    Ssw_Sml_t * pSml;
    Aig_Obj_t * pObj;
    int RetValue, i, k, iBit;
    assert( Aig_ManRegNum(pAig) > 0 );
    assert( Aig_ManRegNum(pAig) < Aig_ManPiNum(pAig) );
    // start a new sequential simulator
    pSml = Ssw_SmlStart( pAig, 0, p->iFrame+1, 1 );
    // assign simulation info for the registers
    iBit = 0;
    Saig_ManForEachLo( pAig, pObj, i )
        Ssw_SmlObjAssignConst( pSml, pObj, Aig_InfoHasBit(p->pData, iBit++), 0 );
    // assign simulation info for the primary inputs
    for ( i = 0; i <= p->iFrame; i++ )
        Saig_ManForEachPi( pAig, pObj, k )
            Ssw_SmlObjAssignConst( pSml, pObj, Aig_InfoHasBit(p->pData, iBit++), i );
    assert( iBit == p->nBits );
    // run random simulation
    Ssw_SmlSimulateOne( pSml );
    // check if the given output has failed
    RetValue = !Ssw_SmlNodeIsZero( pSml, Aig_ManPo(pAig, p->iPo) );
    Ssw_SmlStop( pSml );
    return RetValue;
}

/**Function*************************************************************

  Synopsis    [Resimulates the counter-example.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlFindOutputCounterExample( Aig_Man_t * pAig, Ssw_Cex_t * p )
{
    Ssw_Sml_t * pSml;
    Aig_Obj_t * pObj;
    int i, k, iBit, iOut;
    assert( Aig_ManRegNum(pAig) > 0 );
    assert( Aig_ManRegNum(pAig) < Aig_ManPiNum(pAig) );
    // start a new sequential simulator
    pSml = Ssw_SmlStart( pAig, 0, p->iFrame+1, 1 );
    // assign simulation info for the registers
    iBit = 0;
    Saig_ManForEachLo( pAig, pObj, i )
        Ssw_SmlObjAssignConst( pSml, pObj, Aig_InfoHasBit(p->pData, iBit++), 0 );
    // assign simulation info for the primary inputs
    for ( i = 0; i <= p->iFrame; i++ )
        Saig_ManForEachPi( pAig, pObj, k )
            Ssw_SmlObjAssignConst( pSml, pObj, Aig_InfoHasBit(p->pData, iBit++), i );
    assert( iBit == p->nBits );
    // run random simulation
    Ssw_SmlSimulateOne( pSml );
    // check if the given output has failed
    iOut = -1;
    Saig_ManForEachPo( pAig, pObj, k )
        if ( !Ssw_SmlNodeIsZero( pSml, Aig_ManPo(pAig, k) ) )
        {
            iOut = k;
            break;
        }
    Ssw_SmlStop( pSml );
    return iOut;
}

/**Function*************************************************************

  Synopsis    [Creates sequential counter-example from the simulation info.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Ssw_Cex_t * Ssw_SmlGetCounterExample( Ssw_Sml_t * p )
{
    Ssw_Cex_t * pCex;
    Aig_Obj_t * pObj;
    unsigned * pSims;
    int iPo, iFrame, iBit, i, k;

    // make sure the simulation manager has it
    assert( p->fNonConstOut );

    // find the first output that failed
    iPo = -1;
    iBit = -1;
    iFrame = -1;
    Saig_ManForEachPo( p->pAig, pObj, iPo )
    {
        if ( Ssw_SmlNodeIsZero(p, pObj) )
            continue;
        pSims = Ssw_ObjSim( p, pObj->Id );
        for ( i = p->nWordsPref; i < p->nWordsTotal; i++ )
            if ( pSims[i] )
            {
                iFrame = i / p->nWordsFrame;
                iBit = 32 * (i % p->nWordsFrame) + Aig_WordFindFirstBit( pSims[i] );
                break;
            }
        break;
    }
    assert( iPo < Aig_ManPoNum(p->pAig)-Aig_ManRegNum(p->pAig) );
    assert( iFrame < p->nFrames );
    assert( iBit < 32 * p->nWordsFrame );

    // allocate the counter example
    pCex = Ssw_SmlAllocCounterExample( Aig_ManRegNum(p->pAig), Aig_ManPiNum(p->pAig) - Aig_ManRegNum(p->pAig), iFrame + 1 );
    pCex->iPo    = iPo;
    pCex->iFrame = iFrame;

    // copy the bit data
    Saig_ManForEachLo( p->pAig, pObj, k )
    {
        pSims = Ssw_ObjSim( p, pObj->Id );
        if ( Aig_InfoHasBit( pSims, iBit ) )
            Aig_InfoSetBit( pCex->pData, k );
    }
    for ( i = 0; i <= iFrame; i++ )
    {
        Saig_ManForEachPi( p->pAig, pObj, k )
        {
            pSims = Ssw_ObjSim( p, pObj->Id );
            if ( Aig_InfoHasBit( pSims, 32 * p->nWordsFrame * i + iBit ) )
                Aig_InfoSetBit( pCex->pData, pCex->nRegs + pCex->nPis * i + k );
        }
    }
    // verify the counter example
    if ( !Ssw_SmlRunCounterExample( p->pAig, pCex ) )
    {
        printf( "Ssw_SmlGetCounterExample(): Counter-example is invalid.\n" );
        Ssw_SmlFreeCounterExample( pCex );
        pCex = NULL;
    }
    return pCex;
}
 
/**Function*************************************************************

  Synopsis    [Generates seq counter-example from the combinational one.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Ssw_Cex_t * Ssw_SmlCopyCounterExample( Aig_Man_t * pAig, Aig_Man_t * pFrames, int * pModel )
{
    Ssw_Cex_t * pCex;
    Aig_Obj_t * pObj;
    int i, nFrames, nTruePis, nTruePos, iPo, iFrame;
    // get the number of frames
    assert( Aig_ManRegNum(pAig) > 0 );
    assert( Aig_ManRegNum(pFrames) == 0 );
    nTruePis = Aig_ManPiNum(pAig)-Aig_ManRegNum(pAig);
    nTruePos = Aig_ManPoNum(pAig)-Aig_ManRegNum(pAig);
    nFrames = Aig_ManPiNum(pFrames) / nTruePis;
    assert( nTruePis * nFrames == Aig_ManPiNum(pFrames) );
    assert( nTruePos * nFrames == Aig_ManPoNum(pFrames) );
    // find the PO that failed
    iPo = -1;
    iFrame = -1;
    Aig_ManForEachPo( pFrames, pObj, i )
        if ( pObj->Id == pModel[Aig_ManPiNum(pFrames)] )
        {
            iPo = i % nTruePos;
            iFrame = i / nTruePos;
            break;
        }
    assert( iPo >= 0 );
    // allocate the counter example
    pCex = Ssw_SmlAllocCounterExample( Aig_ManRegNum(pAig), nTruePis, iFrame + 1 );
    pCex->iPo    = iPo;
    pCex->iFrame = iFrame;

    // copy the bit data
    for ( i = 0; i < Aig_ManPiNum(pFrames); i++ )
    {
        if ( pModel[i] )
            Aig_InfoSetBit( pCex->pData, pCex->nRegs + i );
        if ( pCex->nRegs + i == pCex->nBits - 1 )
            break;
    }

    // verify the counter example
    if ( !Ssw_SmlRunCounterExample( pAig, pCex ) )
    {
        printf( "Ssw_SmlGetCounterExample(): Counter-example is invalid.\n" );
        Ssw_SmlFreeCounterExample( pCex );
        pCex = NULL;
    }
    return pCex;

}

/**Function*************************************************************

  Synopsis    [Make the trivial counter-example for the trivially asserted output.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Ssw_Cex_t * Ssw_SmlTrivCounterExample( Aig_Man_t * pAig, int iFrameOut )
{
    Ssw_Cex_t * pCex;
    int nTruePis, nTruePos, iPo, iFrame;
    assert( Aig_ManRegNum(pAig) > 0 );
    nTruePis = Aig_ManPiNum(pAig)-Aig_ManRegNum(pAig);
    nTruePos = Aig_ManPoNum(pAig)-Aig_ManRegNum(pAig);
    iPo = iFrameOut % nTruePos;
    iFrame = iFrameOut / nTruePos;
    // allocate the counter example
    pCex = Ssw_SmlAllocCounterExample( Aig_ManRegNum(pAig), nTruePis, iFrame + 1 );
    pCex->iPo    = iPo;
    pCex->iFrame = iFrame;
    return pCex;
}

/**Function*************************************************************

  Synopsis    [Make the trivial counter-example for the trivially asserted output.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Ssw_Cex_t * Ssw_SmlDupCounterExample( Ssw_Cex_t * p, int nRegsNew )
{
    Ssw_Cex_t * pCex;
    int i;
    pCex = Ssw_SmlAllocCounterExample( nRegsNew, p->nPis, p->iFrame+1 );
    pCex->iPo    = p->iPo;
    pCex->iFrame = p->iFrame;
    for ( i = p->nRegs; i < p->nBits; i++ )
        if ( Aig_InfoHasBit(p->pData, i) )
            Aig_InfoSetBit( pCex->pData, pCex->nRegs + i - p->nRegs );
    return pCex;
}

/**Function*************************************************************

  Synopsis    [Resimulates the counter-example.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Ssw_SmlWriteCounterExample( FILE * pFile, Aig_Man_t * pAig, Ssw_Cex_t * p )
{
    Ssw_Sml_t * pSml;
    Aig_Obj_t * pObj;
    int RetValue, i, k, iBit;
    unsigned * pSims;
    assert( Aig_ManRegNum(pAig) > 0 );
    assert( Aig_ManRegNum(pAig) < Aig_ManPiNum(pAig) );
    // start a new sequential simulator
    pSml = Ssw_SmlStart( pAig, 0, p->iFrame+1, 1 );
    // assign simulation info for the registers
    iBit = 0;
    Saig_ManForEachLo( pAig, pObj, i )
//        Ssw_SmlObjAssignConst( pSml, pObj, Aig_InfoHasBit(p->pData, iBit++), 0 );
        Ssw_SmlObjAssignConst( pSml, pObj, 0, 0 );
    // assign simulation info for the primary inputs
    iBit = p->nRegs;
    for ( i = 0; i <= p->iFrame; i++ )
        Saig_ManForEachPi( pAig, pObj, k )
            Ssw_SmlObjAssignConst( pSml, pObj, Aig_InfoHasBit(p->pData, iBit++), i );
    assert( iBit == p->nBits );
    // run random simulation
    Ssw_SmlSimulateOne( pSml );
    // check if the given output has failed
    RetValue = !Ssw_SmlNodeIsZero( pSml, Aig_ManPo(pAig, p->iPo) );

    // write the output file
    for ( i = 0; i <= p->iFrame; i++ )
    {
/*
        Saig_ManForEachLo( pAig, pObj, k )
        {
            pSims = Ssw_ObjSim(pSml, pObj->Id);
            fprintf( pFile, "%d", (int)(pSims[i] != 0) );
        }
        fprintf( pFile, " " );
*/
        Saig_ManForEachPi( pAig, pObj, k )
        {
            pSims = Ssw_ObjSim(pSml, pObj->Id);
            fprintf( pFile, "%d", (int)(pSims[i] != 0) );
        }
/*
        fprintf( pFile, " " );
        Saig_ManForEachPo( pAig, pObj, k )
        {
            pSims = Ssw_ObjSim(pSml, pObj->Id);
            fprintf( pFile, "%d", (int)(pSims[i] != 0) );
        }
        fprintf( pFile, " " );
        Saig_ManForEachLi( pAig, pObj, k )
        {
            pSims = Ssw_ObjSim(pSml, pObj->Id);
            fprintf( pFile, "%d", (int)(pSims[i] != 0) );
        }
*/
        fprintf( pFile, "\n" );
    }

    Ssw_SmlStop( pSml );
    return RetValue;
}



////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


