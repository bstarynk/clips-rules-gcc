   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/01/16             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Binary CL_Load/CL_Save Functions for Deffunctions      */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFFUNCTION_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)

#include "bload.h"
#include "bsave.h"
#include "cstrcbin.h"
#include "cstrccom.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "modulbin.h"

#include "dffnxbin.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */

/* =========================================
   *****************************************
               MACROS AND TYPES
   =========================================
   ***************************************** */
typedef struct bsaveCL_DeffunctionModule
  {
   struct bsaveDefmoduleItemHeader header;
  } BSAVE_DEFFUNCTION_MODULE;

typedef struct bsaveDeffunctionStruct
  {
   struct bsaveConstructHeader header;
   unsigned short minNumberOfParameters;
   unsigned short maxNumberOfParameters;
   unsigned short numberOfLocalVars;
   unsigned long name;
   unsigned long code;
  } BSAVE_DEFFUNCTION;

/****************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS  */
/****************************************/

#if BLOAD_AND_BSAVE
   static void                    CL_BsaveDeffunctionFind(Environment *);
   static void                    MarkDeffunctionItems(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveDeffunctionExpressions(Environment *,FILE *);
   static void                    CL_BsaveDeffunctionExpression(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveStorageDeffunctions(Environment *,FILE *);
   static void                    CL_BsaveDeffunctions(Environment *,FILE *);
   static void                    CL_BsaveDeffunction(Environment *,ConstructHeader *,void *);
#endif

   static void                    CL_BloadStorageDeffunctions(Environment *);
   static void                    CL_BloadDeffunctions(Environment *);
   static void                    UpdateCL_DeffunctionModule(Environment *,void *,unsigned long);
   static void                    UpdateDeffunction(Environment *,void *,unsigned long);
   static void                    CL_ClearDeffunctionCL_Bload(Environment *);
   static void                    DeallocateDeffunctionCL_BloadData(Environment *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***********************************************************
  NAME         : CL_SetupDeffunctionsCL_Bload
  DESCRIPTION  : Initializes data structures and
                   routines for binary loads of deffunctions
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Routines defined and structures initialized
  NOTES        : None
 ***********************************************************/
void CL_SetupDeffunctionsCL_Bload(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,DFFNXBIN_DATA,sizeof(struct deffunctionBinaryData),DeallocateDeffunctionCL_BloadData);
#if BLOAD_AND_BSAVE
   CL_AddBinaryItem(theEnv,"deffunctions",0,CL_BsaveDeffunctionFind,CL_BsaveDeffunctionExpressions,
                             CL_BsaveStorageDeffunctions,CL_BsaveDeffunctions,
                             CL_BloadStorageDeffunctions,CL_BloadDeffunctions,
                             CL_ClearDeffunctionCL_Bload);
#else
   CL_AddBinaryItem(theEnv,"deffunctions",0,NULL,NULL,NULL,NULL,
                             CL_BloadStorageDeffunctions,CL_BloadDeffunctions,
                             CL_ClearDeffunctionCL_Bload);
#endif
  }

/***********************************************************/
/* DeallocateDeffunctionCL_BloadData: Deallocates environment */
/*    data for the deffunction bsave functionality.        */
/***********************************************************/
static void DeallocateDeffunctionCL_BloadData(
  Environment *theEnv)
  {
   size_t space;

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
   space = DeffunctionBinaryData(theEnv)->DeffunctionCount * sizeof(Deffunction);
   if (space != 0) CL_genfree(theEnv,DeffunctionBinaryData(theEnv)->DeffunctionArray,space);

   space =  DeffunctionBinaryData(theEnv)->ModuleCount * sizeof(struct deffunctionModuleData);
   if (space != 0) CL_genfree(theEnv,DeffunctionBinaryData(theEnv)->ModuleArray,space);
#endif
  }

/***************************************************
  NAME         : CL_BloadCL_DeffunctionModuleReference
  DESCRIPTION  : Returns a pointer to the
                 appropriate deffunction module
  INPUTS       : The index of the module
  RETURNS      : A pointer to the module
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void *CL_BloadCL_DeffunctionModuleReference(
  Environment *theEnv,
  unsigned long theIndex)
  {
   return (void *) &DeffunctionBinaryData(theEnv)->ModuleArray[theIndex];
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if BLOAD_AND_BSAVE

/***************************************************************************
  NAME         : CL_BsaveDeffunctionFind
  DESCRIPTION  : For all deffunctions, this routine marks all
                   the needed symbols.
                 Also, it also counts the number of
                   expression structures needed.
                 Also, counts total number of deffunctions.
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : ExpressionCount (a global from BSAVE.C) is incremented
                   for every expression needed
                 Symbols are marked in their structures
  NOTES        : Also sets bsaveIndex for each deffunction (assumes
                   deffunctions will be bsaved in order of binary list)
 ***************************************************************************/
static void CL_BsaveDeffunctionFind(
  Environment *theEnv)
  {
   CL_SaveCL_BloadCount(theEnv,DeffunctionBinaryData(theEnv)->ModuleCount);
   CL_SaveCL_BloadCount(theEnv,DeffunctionBinaryData(theEnv)->DeffunctionCount);
   DeffunctionBinaryData(theEnv)->DeffunctionCount = 0L;

   DeffunctionBinaryData(theEnv)->ModuleCount = CL_GetNumberOfDefmodules(theEnv);
   
   CL_DoForAllConstructs(theEnv,MarkDeffunctionItems,
                      DeffunctionData(theEnv)->CL_DeffunctionModuleIndex,
                      false,NULL);
  }

/***************************************************
  NAME         : MarkDeffunctionItems
  DESCRIPTION  : Marks the needed items for
                 a deffunction bsave
  INPUTS       : 1) The deffunction
                 2) User data buffer (ignored)
  RETURNS      : Nothing useful
  SIDE EFFECTS : Needed items marked
  NOTES        : None
 ***************************************************/
static void MarkDeffunctionItems(
  Environment *theEnv,
  ConstructHeader *theDeffunction,
  void *userBuffer)
  {
#if MAC_XCD
#pragma unused(userBuffer)
#endif

   CL_MarkConstructHeaderNeededItems(theDeffunction,DeffunctionBinaryData(theEnv)->DeffunctionCount++);
   ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(((Deffunction *) theDeffunction)->code);
   CL_MarkNeededItems(theEnv,((Deffunction *) theDeffunction)->code);
  }

/***************************************************
  NAME         : CL_BsaveDeffunctionExpressions
  DESCRIPTION  : CL_Writes out all expressions needed
                   by deffunctyions
  INPUTS       : The file pointer of the binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : File updated
  NOTES        : None
 ***************************************************/
static void CL_BsaveDeffunctionExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   CL_DoForAllConstructs(theEnv,CL_BsaveDeffunctionExpression,DeffunctionData(theEnv)->CL_DeffunctionModuleIndex,
                      false,fp);
  }

/***************************************************
  NAME         : CL_BsaveDeffunctionExpression
  DESCRIPTION  : CL_Saves the needed expressions for
                 a deffunction bsave
  INPUTS       : 1) The deffunction
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Expressions saved
  NOTES        : None
 ***************************************************/
static void CL_BsaveDeffunctionExpression(
  Environment *theEnv,
  ConstructHeader *theDeffunction,
  void *userBuffer)
  {
   CL_BsaveExpression(theEnv,((Deffunction *) theDeffunction)->code,(FILE *) userBuffer);
  }

/***********************************************************
  NAME         : CL_BsaveStorageDeffunctions
  DESCRIPTION  : CL_Writes out number of each type of structure
                   required for deffunctions
                 Space required for counts (unsigned long)
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 ***********************************************************/
static void CL_BsaveStorageDeffunctions(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;

   space = sizeof(unsigned long) * 2;
   CL_GenCL_Write(&space,sizeof(size_t),fp);
   CL_GenCL_Write(&DeffunctionBinaryData(theEnv)->ModuleCount,sizeof(unsigned long),fp);
   CL_GenCL_Write(&DeffunctionBinaryData(theEnv)->DeffunctionCount,sizeof(unsigned long),fp);
  }

/*************************************************************************************
  NAME         : CL_BsaveDeffunctions
  DESCRIPTION  : CL_Writes out deffunction in binary foCL_rmat
                 Space required (unsigned long)
                 All deffunctions (sizeof(Deffunction) * Number of deffunctions)
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 *************************************************************************************/
static void CL_BsaveDeffunctions(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   Defmodule *theModule;
   CL_DeffunctionModuleData *theModuleItem;
   BSAVE_DEFFUNCTION_MODULE dummy_mitem;

   space = ((sizeof(BSAVE_DEFFUNCTION_MODULE) * DeffunctionBinaryData(theEnv)->ModuleCount) +
            (sizeof(BSAVE_DEFFUNCTION) * DeffunctionBinaryData(theEnv)->DeffunctionCount));
   CL_GenCL_Write(&space,sizeof(size_t),fp);

   /* =================================
      CL_Write out each deffunction module
      ================================= */
   DeffunctionBinaryData(theEnv)->DeffunctionCount = 0L;
   theModule = CL_GetNextDefmodule(theEnv,NULL);
   while (theModule != NULL)
     {
      theModuleItem = (CL_DeffunctionModuleData *)
                      CL_GetModuleItem(theEnv,theModule,CL_FindModuleItem(theEnv,"deffunction")->moduleIndex);
      CL_AssignCL_BsaveDefmdlItemHdrVals(&dummy_mitem.header,&theModuleItem->header);
      CL_GenCL_Write(&dummy_mitem,sizeof(BSAVE_DEFFUNCTION_MODULE),fp);
      theModule = CL_GetNextDefmodule(theEnv,theModule);
     }

   /* ==========================
      CL_Write out each deffunction
      ========================== */
   CL_DoForAllConstructs(theEnv,CL_BsaveDeffunction,DeffunctionData(theEnv)->CL_DeffunctionModuleIndex,
                      false,fp);

   RestoreCL_BloadCount(theEnv,&DeffunctionBinaryData(theEnv)->ModuleCount);
   RestoreCL_BloadCount(theEnv,&DeffunctionBinaryData(theEnv)->DeffunctionCount);
  }

/***************************************************
  NAME         : CL_BsaveDeffunction
  DESCRIPTION  : CL_Bsaves a deffunction
  INPUTS       : 1) The deffunction
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deffunction saved
  NOTES        : None
 ***************************************************/
static void CL_BsaveDeffunction(
  Environment *theEnv,
  ConstructHeader *theDeffunction,
  void *userBuffer)
  {
   Deffunction *dptr = (Deffunction *) theDeffunction;
   BSAVE_DEFFUNCTION dummy_df;

   CL_AssignCL_BsaveConstructHeaderVals(&dummy_df.header,&dptr->header);
   dummy_df.minNumberOfParameters = dptr->minNumberOfParameters;
   dummy_df.maxNumberOfParameters = dptr->maxNumberOfParameters;
   dummy_df.numberOfLocalVars = dptr->numberOfLocalVars;
   if (dptr->code != NULL)
     {
      dummy_df.code = ExpressionData(theEnv)->ExpressionCount;
      ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(dptr->code);
     }
   else
     dummy_df.code = ULONG_MAX;
   CL_GenCL_Write(&dummy_df,sizeof(BSAVE_DEFFUNCTION),(FILE *) userBuffer);
  }

#endif

/***********************************************************************
  NAME         : CL_BloadStorageDeffunctions
  DESCRIPTION  : This routine space required for deffunction
                   structures and allocates space for them
  INPUTS       : Nothing
  RETURNS      : Nothing useful
  SIDE EFFECTS : Arrays allocated and set
  NOTES        : This routine makes no attempt to reset any pointers
                   within the structures
 ***********************************************************************/
static void CL_BloadStorageDeffunctions(
  Environment *theEnv)
  {
   size_t space;

   CL_GenReadBinary(theEnv,&space,sizeof(size_t));
   if (space == 0L)
     return;
   CL_GenReadBinary(theEnv,&DeffunctionBinaryData(theEnv)->ModuleCount,sizeof(unsigned long));
   CL_GenReadBinary(theEnv,&DeffunctionBinaryData(theEnv)->DeffunctionCount,sizeof(unsigned long));
   if (DeffunctionBinaryData(theEnv)->ModuleCount == 0L)
     {
      DeffunctionBinaryData(theEnv)->ModuleArray = NULL;
      DeffunctionBinaryData(theEnv)->DeffunctionArray = NULL;
      return;
     }

   space = (DeffunctionBinaryData(theEnv)->ModuleCount * sizeof(CL_DeffunctionModuleData));
   DeffunctionBinaryData(theEnv)->ModuleArray = (CL_DeffunctionModuleData *) CL_genalloc(theEnv,space);

   if (DeffunctionBinaryData(theEnv)->DeffunctionCount == 0L)
     {
      DeffunctionBinaryData(theEnv)->DeffunctionArray = NULL;
      return;
     }

   space = (DeffunctionBinaryData(theEnv)->DeffunctionCount * sizeof(Deffunction));
   DeffunctionBinaryData(theEnv)->DeffunctionArray = (Deffunction *) CL_genalloc(theEnv,space);
  }

/*********************************************************************
  NAME         : CL_BloadDeffunctions
  DESCRIPTION  : This routine reads deffunction infoCL_rmation from
                   a binary file
                 This routine moves through the deffunction
                   binary array updating pointers
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Pointers reset from array indices
  NOTES        : Assumes all loading is finished
 ********************************************************************/
static void CL_BloadDeffunctions(
  Environment *theEnv)
  {
   size_t space;

   CL_GenReadBinary(theEnv,&space,sizeof(size_t));
   CL_BloadandCL_Refresh(theEnv,DeffunctionBinaryData(theEnv)->ModuleCount,sizeof(BSAVE_DEFFUNCTION_MODULE),UpdateCL_DeffunctionModule);
   CL_BloadandCL_Refresh(theEnv,DeffunctionBinaryData(theEnv)->DeffunctionCount,sizeof(BSAVE_DEFFUNCTION),UpdateDeffunction);
  }

/*******************************************************
  NAME         : UpdateCL_DeffunctionModule
  DESCRIPTION  : Updates deffunction module with binary
                 load data - sets pointers from
                 offset infoCL_rmation
  INPUTS       : 1) A pointer to the bloaded data
                 2) The index of the binary array
                    element to update
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deffunction moudle pointers updated
  NOTES        : None
 *******************************************************/
static void UpdateCL_DeffunctionModule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_DEFFUNCTION_MODULE *bdptr;

   bdptr = (BSAVE_DEFFUNCTION_MODULE *) buf;
   CL_UpdateDefmoduleItemHeader(theEnv,&bdptr->header,&DeffunctionBinaryData(theEnv)->ModuleArray[obji].header,
                             sizeof(Deffunction),DeffunctionBinaryData(theEnv)->DeffunctionArray);
  }

/***************************************************
  NAME         : UpdateDeffunction
  DESCRIPTION  : Updates deffunction with binary
                 load data - sets pointers from
                 offset infoCL_rmation
  INPUTS       : 1) A pointer to the bloaded data
                 2) The index of the binary array
                    element to update
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deffunction pointers upadted
  NOTES        : None
 ***************************************************/
static void UpdateDeffunction(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_DEFFUNCTION *bdptr;
   Deffunction *dptr;

   bdptr = (BSAVE_DEFFUNCTION *) buf;
   dptr = &DeffunctionBinaryData(theEnv)->DeffunctionArray[obji];

   CL_UpdateConstructHeader(theEnv,&bdptr->header,&dptr->header,DEFFUNCTION,
                         sizeof(CL_DeffunctionModuleData),DeffunctionBinaryData(theEnv)->ModuleArray,
                         sizeof(Deffunction),DeffunctionBinaryData(theEnv)->DeffunctionArray);

   dptr->code = ExpressionPointer(bdptr->code);
   dptr->busy = 0;
   dptr->executing = 0;
#if DEBUGGING_FUNCTIONS
   dptr->trace = DeffunctionData(theEnv)->CL_WatchDeffunctions;
#endif
   dptr->minNumberOfParameters = bdptr->minNumberOfParameters;
   dptr->maxNumberOfParameters = bdptr->maxNumberOfParameters;
   dptr->numberOfLocalVars = bdptr->numberOfLocalVars;
  }

/***************************************************************
  NAME         : CL_ClearDeffunctionCL_Bload
  DESCRIPTION  : CL_Release all binary-loaded deffunction
                   structure arrays
                 CL_Resets deffunction list to NULL
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Memory cleared
  NOTES        : Deffunction name symbol counts decremented
 ***************************************************************/
static void CL_ClearDeffunctionCL_Bload(
  Environment *theEnv)
  {
   unsigned long i;
   size_t space;

   space = (sizeof(CL_DeffunctionModuleData) * DeffunctionBinaryData(theEnv)->ModuleCount);
   if (space == 0L)
     return;
   CL_genfree(theEnv,DeffunctionBinaryData(theEnv)->ModuleArray,space);
   DeffunctionBinaryData(theEnv)->ModuleArray = NULL;
   DeffunctionBinaryData(theEnv)->ModuleCount = 0L;

   for (i = 0 ; i < DeffunctionBinaryData(theEnv)->DeffunctionCount ; i++)
     CL_UnmarkConstructHeader(theEnv,&DeffunctionBinaryData(theEnv)->DeffunctionArray[i].header);
   space = (sizeof(Deffunction) * DeffunctionBinaryData(theEnv)->DeffunctionCount);
   if (space == 0)
     return;
   CL_genfree(theEnv,DeffunctionBinaryData(theEnv)->DeffunctionArray,space);
   DeffunctionBinaryData(theEnv)->DeffunctionArray = NULL;
   DeffunctionBinaryData(theEnv)->DeffunctionCount = 0L;
  }

#endif

