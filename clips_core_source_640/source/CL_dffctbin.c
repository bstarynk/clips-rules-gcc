   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*             DEFFACTS BSAVE/BLOAD MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for the  */
/*    deffacts construct.                                    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFFACTS_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)

#include <stdio.h>

#include "bload.h"
#include "bsave.h"
#include "dffctdef.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "moduldef.h"

#include "dffctbin.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
   static void                    CL_BsaveFind(Environment *);
   static void                    CL_BsaveExpressions(Environment *,FILE *);
   static void                    CL_BsaveStorage(Environment *,FILE *);
   static void                    CL_BsaveBinaryItem(Environment *,FILE *);
#endif
   static void                    CL_BloadStorage(Environment *);
   static void                    CL_BloadBinaryItem(Environment *);
   static void                    Update_DeffactsModule(Environment *,void *,unsigned long);
   static void                    UpdateDeffacts(Environment *,void *,unsigned long);
   static void                    CL_Clear_Bload(Environment *);
   static void                    DeallocateDeffacts_BloadData(Environment *);

/********************************************/
/* CL_DeffactsBinarySetup: Installs the binary */
/*   save/load feature for deffacts.        */
/********************************************/
void CL_DeffactsBinarySetup(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,DFFCTBIN_DATA,sizeof(struct deffactsBinaryData),DeallocateDeffacts_BloadData);
#if BLOAD_AND_BSAVE
   CL_AddBinaryItem(theEnv,"deffacts",0,CL_BsaveFind,CL_BsaveExpressions,
                             CL_BsaveStorage,CL_BsaveBinaryItem,
                             CL_BloadStorage,CL_BloadBinaryItem,
                             CL_Clear_Bload);
#endif

#if (BLOAD || BLOAD_ONLY)
   CL_AddBinaryItem(theEnv,"deffacts",0,NULL,NULL,NULL,NULL,
                             CL_BloadStorage,CL_BloadBinaryItem,
                             CL_Clear_Bload);
#endif
  }

/********************************************************/
/* DeallocateDeffacts_BloadData: Deallocates environment */
/*    data for the deffacts bsave functionality.        */
/********************************************************/
static void DeallocateDeffacts_BloadData(
  Environment *theEnv)
  {
   size_t space;

   space = DeffactsBinaryData(theEnv)->NumberOfDeffacts * sizeof(Deffacts);
   if (space != 0) CL_genfree(theEnv,DeffactsBinaryData(theEnv)->DeffactsArray,space);

   space = DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules * sizeof(struct deffactsModule);
   if (space != 0) CL_genfree(theEnv,DeffactsBinaryData(theEnv)->ModuleArray,space);
  }

#if BLOAD_AND_BSAVE

/*********************************************************/
/* CL_BsaveFind: Counts the number of data structures which */
/*   must be saved in the binary image for the deffacts  */
/*   in the current environment.                         */
/*********************************************************/
static void CL_BsaveFind(
  Environment *theEnv)
  {
   Deffacts *theDeffacts;
   Defmodule *theModule;

   /*=======================================================*/
   /* If a binary image is already loaded, then temporarily */
   /* save the count values since these will be overwritten */
   /* in the process of saving the binary image.            */
   /*=======================================================*/

   CL_Save_BloadCount(theEnv,DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules);
   CL_Save_BloadCount(theEnv,DeffactsBinaryData(theEnv)->NumberOfDeffacts);

   /*========================================*/
   /* Set the count of deffacts and deffacts */
   /* module data structures to zero.        */
   /*========================================*/

   DeffactsBinaryData(theEnv)->NumberOfDeffacts = 0;
   DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules = 0;

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      /*===============================================*/
      /* Set the current module to the module being    */
      /* examined and increment the number of deffacts */
      /* modules encountered.                          */
      /*===============================================*/

      CL_SetCurrentModule(theEnv,theModule);
      DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules++;

      /*===================================================*/
      /* Loop through each deffacts in the current module. */
      /*===================================================*/

      for (theDeffacts = CL_GetNextDeffacts(theEnv,NULL);
           theDeffacts != NULL;
           theDeffacts = CL_GetNextDeffacts(theEnv,theDeffacts))
        {
         /*======================================================*/
         /* Initialize the construct header for the binary save. */
         /*======================================================*/

         CL_MarkConstructHeaderNeededItems(&theDeffacts->header,DeffactsBinaryData(theEnv)->NumberOfDeffacts++);

         /*============================================================*/
         /* Count the number of expressions contained in the deffacts' */
         /* assertion list and mark any atomic values contained there  */
         /* as in use.                                                 */
         /*============================================================*/

         ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(theDeffacts->assertList);
         CL_MarkNeededItems(theEnv,theDeffacts->assertList);
        }
     }
  }

/************************************************/
/* CL_BsaveExpressions: CL_Saves the expressions used */
/*   by deffacts to the binary save file.       */
/************************************************/
static void CL_BsaveExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   Deffacts *theDeffacts;
   Defmodule *theModule;

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      /*======================================================*/
      /* Set the current module to the module being examined. */
      /*======================================================*/

      CL_SetCurrentModule(theEnv,theModule);

      /*==================================================*/
      /* Loop through each deffacts in the current module */
      /* and save the assertion list expression.          */
      /*==================================================*/

      for (theDeffacts = CL_GetNextDeffacts(theEnv,NULL);
           theDeffacts != NULL;
           theDeffacts = CL_GetNextDeffacts(theEnv,theDeffacts))
        { CL_BsaveExpression(theEnv,theDeffacts->assertList,fp); }
     }
  }

/******************************************************/
/* CL_BsaveStorage: CL_Writes out the storage requirements  */
/*    for all deffacts structures to the binary file. */
/******************************************************/
static void CL_BsaveStorage(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;

   /*=================================================================*/
   /* Only two data structures are saved as part of a deffacts binary */
   /* image: the deffacts data structure and the deffactsModule data  */
   /* structure. The assertion list expressions are not save with the */
   /* deffacts portion of the binary image.                           */
   /*=================================================================*/

   space = sizeof(long) * 2;
   CL_Gen_Write(&space,sizeof(size_t),fp);
   CL_Gen_Write(&DeffactsBinaryData(theEnv)->NumberOfDeffacts,sizeof(long),fp);
   CL_Gen_Write(&DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules,sizeof(long),fp);
  }

/********************************************/
/* CL_BsaveBinaryItem: CL_Writes out all deffacts */
/*   structures to the binary file.         */
/********************************************/
static void CL_BsaveBinaryItem(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   Deffacts *theDeffacts;
   struct bsaveDeffacts newDeffacts;
   Defmodule *theModule;
   struct bsave_DeffactsModule temp_DeffactsModule;
   struct deffactsModule *theModuleItem;

   /*=========================================================*/
   /* CL_Write out the amount of space taken up by the deffacts  */
   /* and deffactsModule data structures in the binary image. */
   /*=========================================================*/

   space = DeffactsBinaryData(theEnv)->NumberOfDeffacts * sizeof(struct bsaveDeffacts) +
           (DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules * sizeof(struct bsave_DeffactsModule));
   CL_Gen_Write(&space,sizeof(size_t),fp);

   /*================================================*/
   /* CL_Write out each deffacts module data structure. */
   /*================================================*/

   DeffactsBinaryData(theEnv)->NumberOfDeffacts = 0;
   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      CL_SetCurrentModule(theEnv,theModule);

      theModuleItem = (struct deffactsModule *) CL_GetModuleItem(theEnv,NULL,DeffactsData(theEnv)->CL_DeffactsModuleIndex);
      CL_Assign_BsaveDefmdlItemHdrVals(&temp_DeffactsModule.header,&theModuleItem->header);
      CL_Gen_Write(&temp_DeffactsModule,sizeof(struct bsave_DeffactsModule),fp);
     }

   /*==========================*/
   /* CL_Write out each deffacts. */
   /*==========================*/

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      CL_SetCurrentModule(theEnv,theModule);

      for (theDeffacts = CL_GetNextDeffacts(theEnv,NULL);
           theDeffacts != NULL;
           theDeffacts = CL_GetNextDeffacts(theEnv,theDeffacts))
        {
         CL_Assign_BsaveConstructHeaderVals(&newDeffacts.header,&theDeffacts->header);
         if (theDeffacts->assertList != NULL)
           {
            newDeffacts.assertList = ExpressionData(theEnv)->ExpressionCount;
            ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(theDeffacts->assertList);
           }
         else
           { newDeffacts.assertList = ULONG_MAX; }

         CL_Gen_Write(&newDeffacts,sizeof(struct bsaveDeffacts),fp);
        }
     }

   /*=============================================================*/
   /* If a binary image was already loaded when the bsave command */
   /* was issued, then restore the counts indicating the number   */
   /* of deffacts and deffacts modules in the binary image (these */
   /* were overwritten by the binary save).                       */
   /*=============================================================*/

   Restore_BloadCount(theEnv,&DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules);
   Restore_BloadCount(theEnv,&DeffactsBinaryData(theEnv)->NumberOfDeffacts);
  }

#endif /* BLOAD_AND_BSAVE */

/****************************************************/
/* CL_BloadStorage: Allocates storage requirements for */
/*   the deffacts used by this binary image.        */
/****************************************************/
static void CL_BloadStorage(
  Environment *theEnv)
  {
   size_t space;

   /*=====================================================*/
   /* Dete_rmine the number of deffacts and deffactsModule */
   /* data structures to be read.                         */
   /*=====================================================*/

   CL_GenReadBinary(theEnv,&space,sizeof(size_t));
   CL_GenReadBinary(theEnv,&DeffactsBinaryData(theEnv)->NumberOfDeffacts,sizeof(long));
   CL_GenReadBinary(theEnv,&DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules,sizeof(long));

   /*===================================*/
   /* Allocate the space needed for the */
   /* deffactsModule data structures.   */
   /*===================================*/

   if (DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules == 0)
     {
      DeffactsBinaryData(theEnv)->DeffactsArray = NULL;
      DeffactsBinaryData(theEnv)->ModuleArray = NULL;
      return;
     }

   space = DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules * sizeof(struct deffactsModule);
   DeffactsBinaryData(theEnv)->ModuleArray = (struct deffactsModule *) CL_genalloc(theEnv,space);

   /*===================================*/
   /* Allocate the space needed for the */
   /* deffacts data structures.         */
   /*===================================*/

   if (DeffactsBinaryData(theEnv)->NumberOfDeffacts == 0)
     {
      DeffactsBinaryData(theEnv)->DeffactsArray = NULL;
      return;
     }

   space = (DeffactsBinaryData(theEnv)->NumberOfDeffacts * sizeof(Deffacts));
   DeffactsBinaryData(theEnv)->DeffactsArray = (Deffacts *) CL_genalloc(theEnv,space);
  }

/*****************************************************/
/* CL_BloadBinaryItem: CL_Loads and refreshes the deffacts */
/*   constructs used by this binary image.           */
/*****************************************************/
static void CL_BloadBinaryItem(
  Environment *theEnv)
  {
   size_t space;

   /*======================================================*/
   /* Read in the amount of space used by the binary image */
   /* (this is used to skip the construct in the event it  */
   /* is not available in the version being run).          */
   /*======================================================*/

   CL_GenReadBinary(theEnv,&space,sizeof(size_t));

   /*============================================*/
   /* Read in the deffactsModule data structures */
   /* and refresh the pointers.                  */
   /*============================================*/

   CL_Bloadand_Refresh(theEnv,DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules,
                   sizeof(struct bsave_DeffactsModule),Update_DeffactsModule);

   /*======================================*/
   /* Read in the deffacts data structures */
   /* and refresh the pointers.            */
   /*======================================*/

   CL_Bloadand_Refresh(theEnv,DeffactsBinaryData(theEnv)->NumberOfDeffacts,
                   sizeof(struct bsaveDeffacts),UpdateDeffacts);
  }

/***************************************************/
/* Update_DeffactsModule: CL_Bload refresh routine for */
/*   deffacts module data structures.              */
/***************************************************/
static void Update_DeffactsModule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsave_DeffactsModule *bdmPtr;

   bdmPtr = (struct bsave_DeffactsModule *) buf;
   CL_UpdateDefmoduleItemHeader(theEnv,&bdmPtr->header,&DeffactsBinaryData(theEnv)->ModuleArray[obji].header,
                             sizeof(Deffacts),DeffactsBinaryData(theEnv)->DeffactsArray);
  }

/*********************************************/
/* UpdateDeffacts: CL_Bload refresh routine for */
/*   deffacts data structures.               */
/*********************************************/
static void UpdateDeffacts(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveDeffacts *bdp;

   bdp = (struct bsaveDeffacts *) buf;
   CL_UpdateConstructHeader(theEnv,&bdp->header,&DeffactsBinaryData(theEnv)->DeffactsArray[obji].header,
                         DEFFACTS,sizeof(struct deffactsModule),DeffactsBinaryData(theEnv)->ModuleArray,
                         sizeof(Deffacts),DeffactsBinaryData(theEnv)->DeffactsArray);
   DeffactsBinaryData(theEnv)->DeffactsArray[obji].assertList = ExpressionPointer(bdp->assertList);
  }

/**************************************/
/* CL_Clear_Bload: Deffacts clear routine */
/*   when a binary load is in effect. */
/**************************************/
static void CL_Clear_Bload(
  Environment *theEnv)
  {
   unsigned long i;
   size_t space;

   /*=============================================*/
   /* Decrement in use counters for atomic values */
   /* contained in the construct headers.         */
   /*=============================================*/

   for (i = 0; i < DeffactsBinaryData(theEnv)->NumberOfDeffacts; i++)
     { CL_UnmarkConstructHeader(theEnv,&DeffactsBinaryData(theEnv)->DeffactsArray[i].header); }

   /*=============================================================*/
   /* Deallocate the space used for the deffacts data structures. */
   /*=============================================================*/

   space = DeffactsBinaryData(theEnv)->NumberOfDeffacts * sizeof(Deffacts);
   if (space != 0) CL_genfree(theEnv,DeffactsBinaryData(theEnv)->DeffactsArray,space);
   DeffactsBinaryData(theEnv)->NumberOfDeffacts = 0;

   /*====================================================================*/
   /* Deallocate the space used for the deffacts module data structures. */
   /*====================================================================*/

   space = DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules * sizeof(struct deffactsModule);
   if (space != 0) CL_genfree(theEnv,DeffactsBinaryData(theEnv)->ModuleArray,space);
   DeffactsBinaryData(theEnv)->NumberOf_DeffactsModules = 0;
  }

/******************************************************/
/* CL_Bload_DeffactsModuleReference: Returns the deffacts */
/*   module pointer for use with the bload function.  */
/******************************************************/
void *CL_Bload_DeffactsModuleReference(
  Environment *theEnv,
  unsigned long theIndex)
  {
   return (void *) &DeffactsBinaryData(theEnv)->ModuleArray[theIndex];
  }

#endif /* DEFFACTS_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME) */


