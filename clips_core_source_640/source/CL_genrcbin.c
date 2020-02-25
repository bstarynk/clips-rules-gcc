   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Binary CL_Load/CL_Save Functions for Generic Functions */
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

#if DEFGENERIC_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)

#include "bload.h"
#include "bsave.h"
#include "constant.h"
#include "cstrcbin.h"
#include "cstrccom.h"
#include "envrnmnt.h"
#include "genrccom.h"
#include "memalloc.h"
#include "modulbin.h"
#if OBJECT_SYSTEM
#include "objbin.h"
#else
#include "prntutil.h"
#endif
#include "router.h"

#include "genrcbin.h"

/* =========================================
   *****************************************
               MACROS AND TYPES
   =========================================
   ***************************************** */

#define MethodPointer(i) (((i) == ULONG_MAX) ? NULL : (&DefgenericBinaryData(theEnv)->MethodArray[i]))
#define RestrictionPointer(i) (((i) == ULONG_MAX) ? NULL : (RESTRICTION *) &DefgenericBinaryData(theEnv)->RestrictionArray[i])
#define TypePointer(i) (((i) == ULONG_MAX) ? NULL : (void **) &DefgenericBinaryData(theEnv)->TypeArray[i])

typedef struct bsaveRestriction
  {
   unsigned long types;
   unsigned long query;
   unsigned short tcnt;
  } BSAVE_RESTRICTION;

typedef struct bsaveMethod
  {
   struct bsaveConstructHeader header;
   unsigned short index;
   unsigned short restrictionCount;
   unsigned short minRestrictions, maxRestrictions;
   unsigned short localVarCount;
   unsigned system;
   unsigned long restrictions;
   unsigned long actions;
  } BSAVE_METHOD;

typedef struct bsaveGenericFunc
  {
   struct bsaveConstructHeader header;
   unsigned long methods;
   unsigned short mcnt;
  } BSAVE_GENERIC;

typedef struct bsaveGenericModule
  {
   struct bsaveDefmoduleItemHeader header;
  } BSAVE_DEFGENERIC_MODULE;

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE

   static void                    CL_BsaveGenericsFind(Environment *);
   static void                    MarkDefgenericItems(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveGenericsExpressions(Environment *,FILE *);
   static void                    CL_BsaveMethodExpressions(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveRestrictionExpressions(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveGenerics(Environment *,FILE *);
   static void                    CL_BsaveDefgenericHeader(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveMethods(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveMethodRestrictions(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveRestrictionTypes(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveStorageGenerics(Environment *,FILE *);

#endif

   static void                    CL_BloadStorageGenerics(Environment *);
   static void                    CL_BloadGenerics(Environment *);
   static void                    UpdateGenericModule(Environment *,void *,unsigned long);
   static void                    UpdateGeneric(Environment *,void *,unsigned long);
   static void                    UpdateMethod(Environment *,void *,unsigned long);
   static void                    UpdateRestriction(Environment *,void *,unsigned long);
   static void                    UpdateType(Environment *,void *,unsigned long);
   static void                    CL_Clear_BloadGenerics(Environment *);
   static void                    DeallocateDefgenericBinaryData(Environment *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***********************************************************
  NAME         : SetupGenerics_Bload
  DESCRIPTION  : Initializes data structures and
                   routines for binary loads of
                   generic function constructs
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Routines defined and structures initialized
  NOTES        : None
 ***********************************************************/
void SetupGenerics_Bload(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,GENRCBIN_DATA,sizeof(struct defgenericBinaryData),DeallocateDefgenericBinaryData);
#if BLOAD_AND_BSAVE
   CL_AddBinaryItem(theEnv,"generic functions",0,CL_BsaveGenericsFind,CL_BsaveGenericsExpressions,
                             CL_BsaveStorageGenerics,CL_BsaveGenerics,
                             CL_BloadStorageGenerics,CL_BloadGenerics,
                             CL_Clear_BloadGenerics);
#endif
#if BLOAD || BLOAD_ONLY
   CL_AddBinaryItem(theEnv,"generic functions",0,NULL,NULL,NULL,NULL,
                             CL_BloadStorageGenerics,CL_BloadGenerics,
                             CL_Clear_BloadGenerics);
#endif
  }

/***********************************************************/
/* DeallocateDefgenericBinaryData: Deallocates environment */
/*    data for the defgeneric binary functionality.        */
/***********************************************************/
static void DeallocateDefgenericBinaryData(
  Environment *theEnv)
  {
#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
   size_t space;

   space = DefgenericBinaryData(theEnv)->GenericCount * sizeof(Defgeneric);
   if (space != 0) CL_genfree(theEnv,DefgenericBinaryData(theEnv)->DefgenericArray,space);

   space = DefgenericBinaryData(theEnv)->MethodCount * sizeof(Defmethod);
   if (space != 0) CL_genfree(theEnv,DefgenericBinaryData(theEnv)->MethodArray,space);

   space = DefgenericBinaryData(theEnv)->RestrictionCount * sizeof(struct restriction);
   if (space != 0) CL_genfree(theEnv,DefgenericBinaryData(theEnv)->RestrictionArray,space);

   space = DefgenericBinaryData(theEnv)->TypeCount * sizeof(void *);
   if (space != 0) CL_genfree(theEnv,DefgenericBinaryData(theEnv)->TypeArray,space);

   space =  DefgenericBinaryData(theEnv)->ModuleCount * sizeof(struct defgenericModule);
   if (space != 0) CL_genfree(theEnv,DefgenericBinaryData(theEnv)->ModuleArray,space);
#endif
  }

/***************************************************
  NAME         : CL_Bload_DefgenericModuleReference
  DESCRIPTION  : Returns a pointer to the
                 appropriate defgeneric module
  INPUTS       : The index of the module
  RETURNS      : A pointer to the module
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void *CL_Bload_DefgenericModuleReference(
  Environment *theEnv,
  unsigned long theIndex)
  {
   return ((void *) &DefgenericBinaryData(theEnv)->ModuleArray[theIndex]);
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if BLOAD_AND_BSAVE

/***************************************************************************
  NAME         : CL_BsaveGenericsFind
  DESCRIPTION  : For all generic functions and their
                   methods, this routine marks all
                   the needed symbols and system functions.
                 Also, it also counts the number of
                   expression structures needed.
                 Also, counts total number of generics, methods,
                   restrictions and types.
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : ExpressionCount (a global from BSAVE.C) is incremented
                   for every expression needed
                 Symbols and system function are marked in their structures
  NOTES        : Also sets bsaveIndex for each generic function (assumes
                   generic functions will be bsaved in order of binary list)
 ***************************************************************************/
static void CL_BsaveGenericsFind(
  Environment *theEnv)
  {
   CL_Save_BloadCount(theEnv,DefgenericBinaryData(theEnv)->ModuleCount);
   CL_Save_BloadCount(theEnv,DefgenericBinaryData(theEnv)->GenericCount);
   CL_Save_BloadCount(theEnv,DefgenericBinaryData(theEnv)->MethodCount);
   CL_Save_BloadCount(theEnv,DefgenericBinaryData(theEnv)->RestrictionCount);
   CL_Save_BloadCount(theEnv,DefgenericBinaryData(theEnv)->TypeCount);

   DefgenericBinaryData(theEnv)->GenericCount = 0L;
   DefgenericBinaryData(theEnv)->MethodCount = 0L;
   DefgenericBinaryData(theEnv)->RestrictionCount = 0L;
   DefgenericBinaryData(theEnv)->TypeCount = 0L;

   DefgenericBinaryData(theEnv)->ModuleCount = CL_GetNumberOfDefmodules(theEnv);
   
   CL_DoForAllConstructs(theEnv,MarkDefgenericItems,
                      DefgenericData(theEnv)->CL_DefgenericModuleIndex,
                      false,NULL);
  }

/***************************************************
  NAME         : MarkDefgenericItems
  DESCRIPTION  : Marks the needed items for
                 a defgeneric (and methods) bsave
  INPUTS       : 1) The defgeneric
                 2) User data buffer (ignored)
  RETURNS      : Nothing useful
  SIDE EFFECTS : Needed items marked
  NOTES        : None
 ***************************************************/
static void MarkDefgenericItems(
  Environment *theEnv,
  ConstructHeader *theDefgeneric,
  void *userBuffer)
  {
#if MAC_XCD
#pragma unused(userBuffer)
#endif
   Defgeneric *gfunc = (Defgeneric *) theDefgeneric;
   long i,j;
   Defmethod *meth;
   RESTRICTION *rptr;

   CL_MarkConstructHeaderNeededItems(&gfunc->header,DefgenericBinaryData(theEnv)->GenericCount++);
   DefgenericBinaryData(theEnv)->MethodCount += gfunc->mcnt;
   for (i = 0 ; i < gfunc->mcnt ; i++)
     {
      meth = &gfunc->methods[i];
      ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(meth->actions);
      CL_MarkNeededItems(theEnv,meth->actions);
      DefgenericBinaryData(theEnv)->RestrictionCount += meth->restrictionCount;
      for (j = 0 ; j < meth->restrictionCount ; j++)
        {
         rptr = &meth->restrictions[j];
         ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(rptr->query);
         CL_MarkNeededItems(theEnv,rptr->query);
         DefgenericBinaryData(theEnv)->TypeCount += rptr->tcnt;
        }
     }
  }

/***************************************************
  NAME         : CL_BsaveGenericsExpressions
  DESCRIPTION  : CL_Writes out all expressions needed
                   by generic functions
  INPUTS       : The file pointer of the binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : File updated
  NOTES        : None
 ***************************************************/
static void CL_BsaveGenericsExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   /*===================================================================*/
   /* Important to save all expressions for methods before any          */
   /* expressions for restrictions, since methods will be stored first. */
   /*===================================================================*/

   CL_DoForAllConstructs(theEnv,CL_BsaveMethodExpressions,DefgenericData(theEnv)->CL_DefgenericModuleIndex,
                      false,fp);

   CL_DoForAllConstructs(theEnv,CL_BsaveRestrictionExpressions,DefgenericData(theEnv)->CL_DefgenericModuleIndex,
                      false,fp);
  }

/***************************************************
  NAME         : CL_BsaveMethodExpressions
  DESCRIPTION  : CL_Saves the needed expressions for
                 a defgeneric methods bsave
  INPUTS       : 1) The defgeneric
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Method action expressions saved
  NOTES        : None
 ***************************************************/
static void CL_BsaveMethodExpressions(
  Environment *theEnv,
  ConstructHeader *theDefgeneric,
  void *userBuffer)
  {
   Defgeneric *gfunc = (Defgeneric *) theDefgeneric;
   long i;

   for (i = 0 ; i < gfunc->mcnt ; i++)
     CL_BsaveExpression(theEnv,gfunc->methods[i].actions,(FILE *) userBuffer);
  }

/***************************************************
  NAME         : CL_BsaveRestrictionExpressions
  DESCRIPTION  : CL_Saves the needed expressions for
                 a defgeneric method restriction
                 queries bsave
  INPUTS       : 1) The defgeneric
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Method restriction query
                 expressions saved
  NOTES        : None
 ***************************************************/
static void CL_BsaveRestrictionExpressions(
  Environment *theEnv,
  ConstructHeader *theDefgeneric,
  void *userBuffer)
  {
   Defgeneric *gfunc = (Defgeneric *) theDefgeneric;
   long i,j;
   Defmethod *meth;

   for (i = 0 ; i < gfunc->mcnt ; i++)
     {
      meth = &gfunc->methods[i];
      for (j = 0 ; j < meth->restrictionCount ; j++)
        CL_BsaveExpression(theEnv,meth->restrictions[j].query,(FILE *) userBuffer);
     }
  }

/***********************************************************
  NAME         : CL_BsaveStorageGenerics
  DESCRIPTION  : CL_Writes out number of each type of structure
                   required for generics
                 Space required for counts (unsigned long)
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 ***********************************************************/
static void CL_BsaveStorageGenerics(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;

   space = sizeof(long) * 5;
   CL_Gen_Write(&space,sizeof(size_t),fp);
   CL_Gen_Write(&DefgenericBinaryData(theEnv)->ModuleCount,sizeof(long),fp);
   CL_Gen_Write(&DefgenericBinaryData(theEnv)->GenericCount,sizeof(long),fp);
   CL_Gen_Write(&DefgenericBinaryData(theEnv)->MethodCount,sizeof(long),fp);
   CL_Gen_Write(&DefgenericBinaryData(theEnv)->RestrictionCount,sizeof(long),fp);
   CL_Gen_Write(&DefgenericBinaryData(theEnv)->TypeCount,sizeof(long),fp);
  }

/****************************************************************************************
  NAME         : CL_BsaveGenerics
  DESCRIPTION  : CL_Writes out generic function in binary fo_rmat
                 Space required (unsigned long)
                 All generic modules (sizeof(DEFGENERIC_MODULE) * Number of generic modules)
                 All generic headers (sizeof(Defgeneric) * Number of generics)
                 All methods (sizeof(Defmethod) * Number of methods)
                 All method restrictions (sizeof(RESTRICTION) * Number of restriction
                 All restriction type arrays (sizeof(void *) * # of types)
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 ****************************************************************************************/
static void CL_BsaveGenerics(
  Environment *theEnv,
  FILE *fp)
  {
   Defmodule *theModule;
   DEFGENERIC_MODULE *theModuleItem;
   size_t space;
   BSAVE_DEFGENERIC_MODULE dummy_generic_module;

   /* =====================================================================
      Space is: Sum over all structures(sizeof(structure) * structure-cnt))
      ===================================================================== */
   space = (DefgenericBinaryData(theEnv)->ModuleCount * sizeof(BSAVE_DEFGENERIC_MODULE)) +
           (DefgenericBinaryData(theEnv)->GenericCount * sizeof(BSAVE_GENERIC)) +
           (DefgenericBinaryData(theEnv)->MethodCount * sizeof(BSAVE_METHOD)) +
           (DefgenericBinaryData(theEnv)->RestrictionCount * sizeof(BSAVE_RESTRICTION)) +
           (DefgenericBinaryData(theEnv)->TypeCount * sizeof(unsigned long));

   /* ================================================================
      CL_Write out the total amount of space required:  modules,headers,
      methods, restrictions, types
      ================================================================ */
   CL_Gen_Write(&space,sizeof(size_t),fp);

   /* ======================================
      CL_Write out the generic function modules
      ====================================== */
   DefgenericBinaryData(theEnv)->GenericCount = 0L;
   theModule = CL_GetNextDefmodule(theEnv,NULL);
   while (theModule != NULL)
     {
      theModuleItem = (DEFGENERIC_MODULE *)
                      CL_GetModuleItem(theEnv,theModule,CL_FindModuleItem(theEnv,"defgeneric")->moduleIndex);
      CL_Assign_BsaveDefmdlItemHdrVals(&dummy_generic_module.header,
                                           &theModuleItem->header);
      CL_Gen_Write(&dummy_generic_module,
               sizeof(BSAVE_DEFGENERIC_MODULE),fp);
      theModule = CL_GetNextDefmodule(theEnv,theModule);
     }


   /* ======================================
      CL_Write out the generic function headers
      ====================================== */
   DefgenericBinaryData(theEnv)->MethodCount = 0L;
   CL_DoForAllConstructs(theEnv,CL_BsaveDefgenericHeader,DefgenericData(theEnv)->CL_DefgenericModuleIndex,
                      false,fp);

   /* =====================
      CL_Write out the methods
      ===================== */
   DefgenericBinaryData(theEnv)->RestrictionCount = 0L;
   CL_DoForAllConstructs(theEnv,CL_BsaveMethods,DefgenericData(theEnv)->CL_DefgenericModuleIndex,
                      false,fp);

   /* =================================
      CL_Write out the method restrictions
      ================================= */
   DefgenericBinaryData(theEnv)->TypeCount = 0L;
   CL_DoForAllConstructs(theEnv,CL_BsaveMethodRestrictions,DefgenericData(theEnv)->CL_DefgenericModuleIndex,
                      false,fp);

   /* =============================================================
      Finally, write out the type lists for the method restrictions
      ============================================================= */
   CL_DoForAllConstructs(theEnv,CL_BsaveRestrictionTypes,DefgenericData(theEnv)->CL_DefgenericModuleIndex,
                      false,fp);

   Restore_BloadCount(theEnv,&DefgenericBinaryData(theEnv)->ModuleCount);
   Restore_BloadCount(theEnv,&DefgenericBinaryData(theEnv)->GenericCount);
   Restore_BloadCount(theEnv,&DefgenericBinaryData(theEnv)->MethodCount);
   Restore_BloadCount(theEnv,&DefgenericBinaryData(theEnv)->RestrictionCount);
   Restore_BloadCount(theEnv,&DefgenericBinaryData(theEnv)->TypeCount);
  }

/***************************************************
  NAME         : CL_BsaveDefgenericHeader
  DESCRIPTION  : CL_Bsaves a generic function header
  INPUTS       : 1) The defgeneric
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defgeneric header saved
  NOTES        : None
 ***************************************************/
static void CL_BsaveDefgenericHeader(
  Environment *theEnv,
  ConstructHeader *theDefgeneric,
  void *userBuffer)
  {
   Defgeneric *gfunc = (Defgeneric *) theDefgeneric;
   BSAVE_GENERIC dummy_generic;

   CL_Assign_BsaveConstructHeaderVals(&dummy_generic.header,&gfunc->header);
   dummy_generic.mcnt = gfunc->mcnt;
   if (gfunc->methods != NULL)
     {
      dummy_generic.methods = DefgenericBinaryData(theEnv)->MethodCount;
      DefgenericBinaryData(theEnv)->MethodCount += gfunc->mcnt;
     }
   else
     dummy_generic.methods = ULONG_MAX;
   CL_Gen_Write(&dummy_generic,sizeof(BSAVE_GENERIC),(FILE *) userBuffer);
  }

/***************************************************
  NAME         : CL_BsaveMethods
  DESCRIPTION  : CL_Bsaves defgeneric methods
  INPUTS       : 1) The defgeneric
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defgeneric methods saved
  NOTES        : None
 ***************************************************/
static void CL_BsaveMethods(
  Environment *theEnv,
  ConstructHeader *theDefgeneric,
  void *userBuffer)
  {
   Defgeneric *gfunc = (Defgeneric *) theDefgeneric;
   Defmethod *meth;
   BSAVE_METHOD dummy_method;
   long i;

   for (i = 0 ; i < gfunc->mcnt ; i++)
     {
      meth = &gfunc->methods[i];
      
      CL_Assign_BsaveConstructHeaderVals(&dummy_method.header,&meth->header);

      dummy_method.index = meth->index;
      dummy_method.restrictionCount = meth->restrictionCount;
      dummy_method.minRestrictions = meth->minRestrictions;
      dummy_method.maxRestrictions = meth->maxRestrictions;
      dummy_method.localVarCount = meth->localVarCount;
      dummy_method.system = meth->system;
      if (meth->restrictions != NULL)
        {
         dummy_method.restrictions = DefgenericBinaryData(theEnv)->RestrictionCount;
         DefgenericBinaryData(theEnv)->RestrictionCount += meth->restrictionCount;
        }
      else
        dummy_method.restrictions = ULONG_MAX;
      if (meth->actions != NULL)
        {
         dummy_method.actions = ExpressionData(theEnv)->ExpressionCount;
         ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(meth->actions);
        }
      else
        dummy_method.actions = ULONG_MAX;
      CL_Gen_Write(&dummy_method,sizeof(BSAVE_METHOD),(FILE *) userBuffer);
     }
  }

/******************************************************
  NAME         : CL_BsaveMethodRestrictions
  DESCRIPTION  : CL_Bsaves defgeneric methods' retrictions
  INPUTS       : 1) The defgeneric
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defgeneric methods' restrictions saved
  NOTES        : None
 ******************************************************/
static void CL_BsaveMethodRestrictions(
  Environment *theEnv,
  ConstructHeader *theDefgeneric,
  void *userBuffer)
  {
   Defgeneric *gfunc = (Defgeneric *) theDefgeneric;
   BSAVE_RESTRICTION dummy_restriction;
   RESTRICTION *rptr;
   short i,j;

   for (i = 0 ; i < gfunc->mcnt ; i++)
     {
      for (j = 0 ; j < gfunc->methods[i].restrictionCount ; j++)
        {
         rptr = &gfunc->methods[i].restrictions[j];
         dummy_restriction.tcnt = rptr->tcnt;
         if (rptr->types != NULL)
           {
            dummy_restriction.types = DefgenericBinaryData(theEnv)->TypeCount;
            DefgenericBinaryData(theEnv)->TypeCount += rptr->tcnt;
           }
         else
           dummy_restriction.types = ULONG_MAX;
         if (rptr->query != NULL)
           {
            dummy_restriction.query = ExpressionData(theEnv)->ExpressionCount;
            ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(rptr->query);
           }
         else
           dummy_restriction.query = ULONG_MAX;
         CL_Gen_Write(&dummy_restriction,
                  sizeof(BSAVE_RESTRICTION),(FILE *) userBuffer);
        }
     }
  }

/*************************************************************
  NAME         : CL_BsaveRestrictionTypes
  DESCRIPTION  : CL_Bsaves defgeneric methods' retrictions' types
  INPUTS       : 1) The defgeneric
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defgeneric methods' restrictions' types saved
  NOTES        : None
 *************************************************************/
static void CL_BsaveRestrictionTypes(
  Environment *theEnv,
  ConstructHeader *theDefgeneric,
  void *userBuffer)
  {
   Defgeneric *gfunc = (Defgeneric *) theDefgeneric;
   unsigned long dummy_type;
   RESTRICTION *rptr;
   short i,j,k;
#if MAC_XCD
#pragma unused(theEnv)
#endif

   for (i = 0 ; i < gfunc->mcnt ; i++)
     {
      for (j = 0 ; j < gfunc->methods[i].restrictionCount ; j++)
        {
         rptr = &gfunc->methods[i].restrictions[j];
         for (k = 0 ; k < rptr->tcnt ; k++)
           {
#if OBJECT_SYSTEM
            dummy_type = DefclassIndex(rptr->types[k]);
#else
            dummy_type = (unsigned long) ((CLIPSInteger *) rptr->types[k])->contents;
#endif
            CL_Gen_Write(&dummy_type,sizeof(unsigned long),(FILE *) userBuffer);
           }
        }
     }
  }

#endif

/***********************************************************************
  NAME         : CL_BloadStorageGenerics
  DESCRIPTION  : This routine space required for generic function
                   structures and allocates space for them
  INPUTS       : Nothing
  RETURNS      : Nothing useful
  SIDE EFFECTS : Arrays allocated and set
  NOTES        : This routine makes no attempt to reset any pointers
                   within the structures
 ***********************************************************************/
static void CL_BloadStorageGenerics(
  Environment *theEnv)
  {
   size_t space;
   unsigned long counts[5];

   CL_GenReadBinary(theEnv,&space,sizeof(size_t));
   if (space == 0L)
     return;
   CL_GenReadBinary(theEnv,counts,space);
   DefgenericBinaryData(theEnv)->ModuleCount = counts[0];
   DefgenericBinaryData(theEnv)->GenericCount = counts[1];
   DefgenericBinaryData(theEnv)->MethodCount = counts[2];
   DefgenericBinaryData(theEnv)->RestrictionCount = counts[3];
   DefgenericBinaryData(theEnv)->TypeCount = counts[4];
   if (DefgenericBinaryData(theEnv)->ModuleCount != 0L)
     {
      space = (sizeof(DEFGENERIC_MODULE) * DefgenericBinaryData(theEnv)->ModuleCount);
      DefgenericBinaryData(theEnv)->ModuleArray = (DEFGENERIC_MODULE *) CL_genalloc(theEnv,space);
     }
   else
     return;
   if (DefgenericBinaryData(theEnv)->GenericCount != 0L)
     {
      space = (sizeof(Defgeneric) * DefgenericBinaryData(theEnv)->GenericCount);
      DefgenericBinaryData(theEnv)->DefgenericArray = (Defgeneric *) CL_genalloc(theEnv,space);
     }
   else
     return;
   if (DefgenericBinaryData(theEnv)->MethodCount != 0L)
     {
      space = (sizeof(Defmethod) * DefgenericBinaryData(theEnv)->MethodCount);
      DefgenericBinaryData(theEnv)->MethodArray = (Defmethod *) CL_genalloc(theEnv,space);
     }
   else
     return;
   if (DefgenericBinaryData(theEnv)->RestrictionCount != 0L)
     {
      space = (sizeof(RESTRICTION) * DefgenericBinaryData(theEnv)->RestrictionCount);
      DefgenericBinaryData(theEnv)->RestrictionArray = (RESTRICTION *) CL_genalloc(theEnv,space);
     }
   else
     return;
   if (DefgenericBinaryData(theEnv)->TypeCount != 0L)
     {
      space = (sizeof(void *) * DefgenericBinaryData(theEnv)->TypeCount);
      DefgenericBinaryData(theEnv)->TypeArray = (void * *) CL_genalloc(theEnv,space);
     }
  }

/*********************************************************************
  NAME         : CL_BloadGenerics
  DESCRIPTION  : This routine reads generic function info_rmation from
                 a binary file in four chunks:
                 Generic-header array
                 Method array
                 Method restrictions array
                 Restriction types array

                 This routine moves through the generic function
                   binary arrays updating pointers
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Pointers reset from array indices
  NOTES        : Assumes all loading is finished
 ********************************************************************/
static void CL_BloadGenerics(
  Environment *theEnv)
  {
   size_t space;

   CL_GenReadBinary(theEnv,&space,sizeof(size_t));
   if (DefgenericBinaryData(theEnv)->ModuleCount == 0L)
     return;
   CL_Bloadand_Refresh(theEnv,DefgenericBinaryData(theEnv)->ModuleCount,sizeof(BSAVE_DEFGENERIC_MODULE),UpdateGenericModule);
   if (DefgenericBinaryData(theEnv)->GenericCount == 0L)
     return;
   CL_Bloadand_Refresh(theEnv,DefgenericBinaryData(theEnv)->GenericCount,sizeof(BSAVE_GENERIC),UpdateGeneric);
   CL_Bloadand_Refresh(theEnv,DefgenericBinaryData(theEnv)->MethodCount,sizeof(BSAVE_METHOD),UpdateMethod);
   CL_Bloadand_Refresh(theEnv,DefgenericBinaryData(theEnv)->RestrictionCount,sizeof(BSAVE_RESTRICTION),UpdateRestriction);
   CL_Bloadand_Refresh(theEnv,DefgenericBinaryData(theEnv)->TypeCount,sizeof(long),UpdateType);
  }

/*********************************************
  CL_Bload update routines for generic structures
 *********************************************/
static void UpdateGenericModule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_DEFGENERIC_MODULE *bdptr;

   bdptr = (BSAVE_DEFGENERIC_MODULE *) buf;
   CL_UpdateDefmoduleItemHeader(theEnv,&bdptr->header,&DefgenericBinaryData(theEnv)->ModuleArray[obji].header,
                             sizeof(Defgeneric),DefgenericBinaryData(theEnv)->DefgenericArray);
  }

static void UpdateGeneric(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_GENERIC *bgp;
   Defgeneric *gp;

   bgp = (BSAVE_GENERIC *) buf;
   gp = &DefgenericBinaryData(theEnv)->DefgenericArray[obji];

   CL_UpdateConstructHeader(theEnv,&bgp->header,&gp->header,DEFGENERIC,
                         sizeof(DEFGENERIC_MODULE),DefgenericBinaryData(theEnv)->ModuleArray,
                         sizeof(Defgeneric),DefgenericBinaryData(theEnv)->DefgenericArray);
   DefgenericBinaryData(theEnv)->DefgenericArray[obji].busy = 0;
#if DEBUGGING_FUNCTIONS
   DefgenericBinaryData(theEnv)->DefgenericArray[obji].trace = DefgenericData(theEnv)->CL_WatchGenerics;
#endif
   DefgenericBinaryData(theEnv)->DefgenericArray[obji].methods = MethodPointer(bgp->methods);
   DefgenericBinaryData(theEnv)->DefgenericArray[obji].mcnt = bgp->mcnt;
   DefgenericBinaryData(theEnv)->DefgenericArray[obji].new_index = 0;
  }

static void UpdateMethod(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_METHOD *bmth;

   bmth = (BSAVE_METHOD *) buf;
   DefgenericBinaryData(theEnv)->MethodArray[obji].index = bmth->index;
   DefgenericBinaryData(theEnv)->MethodArray[obji].busy = 0;
#if DEBUGGING_FUNCTIONS
   DefgenericBinaryData(theEnv)->MethodArray[obji].trace = DefgenericData(theEnv)->CL_WatchMethods;
#endif
   DefgenericBinaryData(theEnv)->MethodArray[obji].restrictionCount = bmth->restrictionCount;
   DefgenericBinaryData(theEnv)->MethodArray[obji].minRestrictions = bmth->minRestrictions;
   DefgenericBinaryData(theEnv)->MethodArray[obji].maxRestrictions = bmth->maxRestrictions;
   DefgenericBinaryData(theEnv)->MethodArray[obji].localVarCount = bmth->localVarCount;
   DefgenericBinaryData(theEnv)->MethodArray[obji].system = bmth->system;
   DefgenericBinaryData(theEnv)->MethodArray[obji].restrictions = RestrictionPointer(bmth->restrictions);
   DefgenericBinaryData(theEnv)->MethodArray[obji].actions = ExpressionPointer(bmth->actions);
   
   CL_UpdateConstructHeader(theEnv,&bmth->header,&DefgenericBinaryData(theEnv)->MethodArray[obji].header,DEFMETHOD,
                         sizeof(DEFGENERIC_MODULE),DefgenericBinaryData(theEnv)->ModuleArray,
                         sizeof(Defmethod),DefgenericBinaryData(theEnv)->MethodArray);
  }

static void UpdateRestriction(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_RESTRICTION *brp;

   brp = (BSAVE_RESTRICTION *) buf;
   DefgenericBinaryData(theEnv)->RestrictionArray[obji].tcnt = brp->tcnt;
   DefgenericBinaryData(theEnv)->RestrictionArray[obji].types = TypePointer(brp->types);
   DefgenericBinaryData(theEnv)->RestrictionArray[obji].query = ExpressionPointer(brp->query);
  }

static void UpdateType(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
#if OBJECT_SYSTEM
   DefgenericBinaryData(theEnv)->TypeArray[obji] = DefclassPointer(* (unsigned long *) buf);
#else
   if ((* (long *) buf) > INSTANCE_TYPE_CODE)
     {
      CL_PrintWarningID(theEnv,"GENRCBIN",1,false);
      CL_WriteString(theEnv,STDWRN,"COOL not installed!  User-defined class\n");
      CL_WriteString(theEnv,STDWRN,"  in method restriction substituted with OBJECT.\n");
      DefgenericBinaryData(theEnv)->TypeArray[obji] = CL_CreateInteger(theEnv,OBJECT_TYPE_CODE);
     }
   else
     DefgenericBinaryData(theEnv)->TypeArray[obji] = CL_CreateInteger(theEnv,* (long *) buf);
   IncrementIntegerCount((CLIPSInteger *) DefgenericBinaryData(theEnv)->TypeArray[obji]);
#endif
  }

/***************************************************************
  NAME         : CL_Clear_BloadGenerics
  DESCRIPTION  : CL_Release all binary-loaded generic function
                   structure arrays
                 CL_Resets generic function list to NULL
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Memory cleared
  NOTES        : Generic function name symbol counts decremented
 ***************************************************************/
static void CL_Clear_BloadGenerics(
  Environment *theEnv)
  {
   unsigned long i;
   size_t space;

   space = (sizeof(DEFGENERIC_MODULE) * DefgenericBinaryData(theEnv)->ModuleCount);
   if (space == 0L)
     return;
   CL_genfree(theEnv,DefgenericBinaryData(theEnv)->ModuleArray,space);
   DefgenericBinaryData(theEnv)->ModuleArray = NULL;
   DefgenericBinaryData(theEnv)->ModuleCount = 0L;

   for (i = 0 ; i < DefgenericBinaryData(theEnv)->GenericCount ; i++)
     CL_UnmarkConstructHeader(theEnv,&DefgenericBinaryData(theEnv)->DefgenericArray[i].header);

   space = (sizeof(Defgeneric) * DefgenericBinaryData(theEnv)->GenericCount);
   if (space == 0L)
     return;
   CL_genfree(theEnv,DefgenericBinaryData(theEnv)->DefgenericArray,space);
   DefgenericBinaryData(theEnv)->DefgenericArray = NULL;
   DefgenericBinaryData(theEnv)->GenericCount = 0L;

   space = (sizeof(Defmethod) * DefgenericBinaryData(theEnv)->MethodCount);
   if (space == 0L)
     return;
   CL_genfree(theEnv,DefgenericBinaryData(theEnv)->MethodArray,space);
   DefgenericBinaryData(theEnv)->MethodArray = NULL;
   DefgenericBinaryData(theEnv)->MethodCount = 0L;

   space = (sizeof(RESTRICTION) * DefgenericBinaryData(theEnv)->RestrictionCount);
   if (space == 0L)
     return;
   CL_genfree(theEnv,DefgenericBinaryData(theEnv)->RestrictionArray,space);
   DefgenericBinaryData(theEnv)->RestrictionArray = NULL;
   DefgenericBinaryData(theEnv)->RestrictionCount = 0L;

#if ! OBJECT_SYSTEM
   for (i = 0 ; i < DefgenericBinaryData(theEnv)->TypeCount ; i++)
     DecrementIntegerCount(theEnv,(CLIPSInteger *) DefgenericBinaryData(theEnv)->TypeArray[i]);
#endif
   space = (sizeof(void *) * DefgenericBinaryData(theEnv)->TypeCount);
   if (space == 0L)
     return;
   CL_genfree(theEnv,DefgenericBinaryData(theEnv)->TypeArray,space);
   DefgenericBinaryData(theEnv)->TypeArray = NULL;
   DefgenericBinaryData(theEnv)->TypeCount = 0L;
  }

#endif

