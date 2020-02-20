   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*            DEFGLOBAL BSAVE/BLOAD MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for the  */
/*    defglobal construct.                                   */
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
/*            Moved CL_WatchGlobals global to defglobalData.    */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFGLOBAL_CONSTRUCT && (BLOAD || BLOAD_AND_BSAVE || BLOAD_ONLY) && (! RUN_TIME)

#include <stdio.h>

#include "bload.h"
#include "bsave.h"
#include "envrnmnt.h"
#include "globlbsc.h"
#include "globldef.h"
#include "memalloc.h"
#include "moduldef.h"
#include "multifld.h"

#include "globlbin.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
   static void                    CL_BsaveFind(Environment *);
   static void                    CL_BsaveStorage(Environment *,FILE *);
   static void                    CL_BsaveBinaryItem(Environment *,FILE *);
#endif
   static void                    CL_BloadStorageDefglobals(Environment *);
   static void                    CL_BloadBinaryItem(Environment *);
   static void                    UpdateCL_DefglobalModule(Environment *,void *,unsigned long);
   static void                    UpdateDefglobal(Environment *,void *,unsigned long);
   static void                    CL_ClearCL_Bload(Environment *);
   static void                    DeallocateDefglobalCL_BloadData(Environment *);

/*********************************************/
/* CL_DefglobalBinarySetup: Installs the binary */
/*   save/load feature for the defglobals.   */
/*********************************************/
void CL_DefglobalBinarySetup(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,GLOBLBIN_DATA,sizeof(struct defglobalBinaryData),DeallocateDefglobalCL_BloadData);
#if (BLOAD_AND_BSAVE || BLOAD)
   CL_AddAfterCL_BloadFunction(theEnv,"defglobal",CL_ResetDefglobals,50,NULL);
#endif

#if BLOAD_AND_BSAVE
   CL_AddBinaryItem(theEnv,"defglobal",0,CL_BsaveFind,NULL,
                             CL_BsaveStorage,CL_BsaveBinaryItem,
                             CL_BloadStorageDefglobals,CL_BloadBinaryItem,
                             CL_ClearCL_Bload);
#endif

#if (BLOAD || BLOAD_ONLY)
   CL_AddBinaryItem(theEnv,"defglobal",0,NULL,NULL,NULL,NULL,
                             CL_BloadStorageDefglobals,CL_BloadBinaryItem,
                             CL_ClearCL_Bload);
#endif
  }

/*********************************************************/
/* DeallocateDefglobalCL_BloadData: Deallocates environment */
/*    data for the defglobal bsave functionality.        */
/*********************************************************/
static void DeallocateDefglobalCL_BloadData(
  Environment *theEnv)
  {
#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
   size_t space;
   unsigned long i;

   for (i = 0; i < DefglobalBinaryData(theEnv)->NumberOfDefglobals; i++)
     {
      if (DefglobalBinaryData(theEnv)->DefglobalArray[i].current.header->type == MULTIFIELD_TYPE)
        { CL_ReturnMultifield(theEnv,DefglobalBinaryData(theEnv)->DefglobalArray[i].current.multifieldValue); }
     }

   space = DefglobalBinaryData(theEnv)->NumberOfDefglobals * sizeof(Defglobal);
   if (space != 0)
     { CL_genfree(theEnv,DefglobalBinaryData(theEnv)->DefglobalArray,space); }

   space =  DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules * sizeof(struct defglobalModule);
   if (space != 0)
     { CL_genfree(theEnv,DefglobalBinaryData(theEnv)->ModuleArray,space); }
#endif
  }

#if BLOAD_AND_BSAVE

/****************************************************/
/* CL_BsaveFind:  Counts the number of data structures */
/*   which must be saved in the binary image for    */
/*   the defglobals in the current environment.     */
/****************************************************/
static void CL_BsaveFind(
  Environment *theEnv)
  {
   Defglobal *defglobalPtr;
   Defmodule *theModule;

   /*=======================================================*/
   /* If a binary image is already loaded, then temporarily */
   /* save the count values since these will be overwritten */
   /* in the process of saving the binary image.            */
   /*=======================================================*/

   CL_SaveCL_BloadCount(theEnv,DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules);
   CL_SaveCL_BloadCount(theEnv,DefglobalBinaryData(theEnv)->NumberOfDefglobals);

   /*============================================*/
   /* Set the count of defglobals and defglobals */
   /* module data structures to zero.            */
   /*============================================*/

   DefglobalBinaryData(theEnv)->NumberOfDefglobals = 0;
   DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules = 0;

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      /*================================================*/
      /* Set the current module to the module being     */
      /* examined and increment the number of defglobal */
      /* modules encountered.                           */
      /*================================================*/

      CL_SetCurrentModule(theEnv,theModule);
      DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules++;

      /*====================================================*/
      /* Loop through each defglobal in the current module. */
      /*====================================================*/

      for (defglobalPtr = CL_GetNextDefglobal(theEnv,NULL);
           defglobalPtr != NULL;
           defglobalPtr = CL_GetNextDefglobal(theEnv,defglobalPtr))
        {
         /*======================================================*/
         /* Initialize the construct header for the binary save. */
         /*======================================================*/

         CL_MarkConstructHeaderNeededItems(&defglobalPtr->header,DefglobalBinaryData(theEnv)->NumberOfDefglobals++);
        }
     }
  }

/*****************************************************/
/* CL_BsaveStorage: CL_Writes out storage requirements for */
/*   all defglobal structures to the binary file     */
/*****************************************************/
static void CL_BsaveStorage(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;

   /*===========================================================*/
   /* Only two data structures are saved as part of a defglobal */
   /* binary image: the defglobal data structure and the        */
   /* defglobalModule data structure.                           */
   /*===========================================================*/

   space = sizeof(long) * 2;
   CL_GenCL_Write(&space,sizeof(size_t),fp);
   CL_GenCL_Write(&DefglobalBinaryData(theEnv)->NumberOfDefglobals,sizeof(long),fp);
   CL_GenCL_Write(&DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules,sizeof(long),fp);
  }

/*********************************************/
/* CL_BsaveBinaryItem: CL_Writes out all defglobal */
/*   structures to the binary file           */
/*********************************************/
static void CL_BsaveBinaryItem(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   Defglobal *theDefglobal;
   struct bsaveDefglobal newDefglobal;
   Defmodule *theModule;
   struct bsaveCL_DefglobalModule tempCL_DefglobalModule;
   struct defglobalModule *theModuleItem;

   /*==========================================================*/
   /* CL_Write out the amount of space taken up by the defglobal  */
   /* and defglobalModule data structures in the binary image. */
   /*==========================================================*/

   space = DefglobalBinaryData(theEnv)->NumberOfDefglobals * sizeof(struct bsaveDefglobal) +
           (DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules * sizeof(struct bsaveCL_DefglobalModule));
   CL_GenCL_Write(&space,sizeof(size_t),fp);

   /*=================================================*/
   /* CL_Write out each defglobal module data structure. */
   /*=================================================*/

   DefglobalBinaryData(theEnv)->NumberOfDefglobals = 0;
   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      CL_SetCurrentModule(theEnv,theModule);

      theModuleItem = (struct defglobalModule *)
                      CL_GetModuleItem(theEnv,NULL,CL_FindModuleItem(theEnv,"defglobal")->moduleIndex);
      CL_AssignCL_BsaveDefmdlItemHdrVals(&tempCL_DefglobalModule.header,
                                           &theModuleItem->header);
      CL_GenCL_Write(&tempCL_DefglobalModule,sizeof(struct bsaveCL_DefglobalModule),fp);
     }

   /*===========================*/
   /* CL_Write out each defglobal. */
   /*===========================*/

   DefglobalBinaryData(theEnv)->NumberOfDefglobals = 0;
   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      CL_SetCurrentModule(theEnv,theModule);

      for (theDefglobal = CL_GetNextDefglobal(theEnv,NULL);
           theDefglobal != NULL;
           theDefglobal = CL_GetNextDefglobal(theEnv,theDefglobal))
        {
         CL_AssignCL_BsaveConstructHeaderVals(&newDefglobal.header,
                                          &theDefglobal->header);
         newDefglobal.initial = CL_HashedExpressionIndex(theEnv,theDefglobal->initial);

         CL_GenCL_Write(&newDefglobal,sizeof(struct bsaveDefglobal),fp);
        }
     }

   /*=============================================================*/
   /* If a binary image was already loaded when the bsave command */
   /* was issued, then restore the counts indicating the number   */
   /* of defglobals and defglobal modules in the binary image     */
   /* (these were overwritten by the binary save).                */
   /*=============================================================*/

   RestoreCL_BloadCount(theEnv,&DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules);
   RestoreCL_BloadCount(theEnv,&DefglobalBinaryData(theEnv)->NumberOfDefglobals);
  }

#endif /* BLOAD_AND_BSAVE */

/***********************************************/
/* CL_BloadStorageDefglobals: Allocates space for */
/*   the defglobals used by this binary image. */
/***********************************************/
static void CL_BloadStorageDefglobals(
  Environment *theEnv)
  {
   size_t space;

   /*=======================================================*/
   /* DeteCL_rmine the number of defglobal and defglobalModule */
   /* data structures to be read.                           */
   /*=======================================================*/

   CL_GenReadBinary(theEnv,&space,sizeof(size_t));
   CL_GenReadBinary(theEnv,&DefglobalBinaryData(theEnv)->NumberOfDefglobals,sizeof(long));
   CL_GenReadBinary(theEnv,&DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules,sizeof(long));

   /*===================================*/
   /* Allocate the space needed for the */
   /* defglobalModule data structures.  */
   /*===================================*/

   if (DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules == 0)
     {
      DefglobalBinaryData(theEnv)->DefglobalArray = NULL;
      DefglobalBinaryData(theEnv)->ModuleArray = NULL;
     }

   space = DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules * sizeof(struct defglobalModule);
   DefglobalBinaryData(theEnv)->ModuleArray = (struct defglobalModule *) CL_genalloc(theEnv,space);

   /*===================================*/
   /* Allocate the space needed for the */
   /* defglobal data structures.        */
   /*===================================*/

   if (DefglobalBinaryData(theEnv)->NumberOfDefglobals == 0)
     {
      DefglobalBinaryData(theEnv)->DefglobalArray = NULL;
      return;
     }

   space = (DefglobalBinaryData(theEnv)->NumberOfDefglobals * sizeof(Defglobal));
   DefglobalBinaryData(theEnv)->DefglobalArray = (Defglobal *) CL_genalloc(theEnv,space);
  }

/******************************************************/
/* CL_BloadBinaryItem: CL_Loads and refreshes the defglobal */
/*   constructs used by this binary image.            */
/******************************************************/
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

   /*=============================================*/
   /* Read in the defglobalModule data structures */
   /* and refresh the pointers.                   */
   /*=============================================*/

   CL_BloadandCL_Refresh(theEnv,DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules,
                   sizeof(struct bsaveCL_DefglobalModule),
                   UpdateCL_DefglobalModule);

   /*=======================================*/
   /* Read in the defglobal data structures */
   /* and refresh the pointers.             */
   /*=======================================*/

   CL_BloadandCL_Refresh(theEnv,DefglobalBinaryData(theEnv)->NumberOfDefglobals,
                   sizeof(struct bsaveDefglobal),
                   UpdateDefglobal);
  }

/************************************************/
/* UpdateCL_DefglobalModule: CL_Bload refresh routine */
/*   for defglobal module data structures.      */
/************************************************/
static void UpdateCL_DefglobalModule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveCL_DefglobalModule *bdmPtr;

   bdmPtr = (struct bsaveCL_DefglobalModule *) buf;

   CL_UpdateDefmoduleItemHeader(theEnv,&bdmPtr->header,&DefglobalBinaryData(theEnv)->ModuleArray[obji].header,
                             sizeof(Defglobal),
                             DefglobalBinaryData(theEnv)->DefglobalArray);
  }

/******************************************/
/* UpdateDefglobal: CL_Bload refresh routine */
/*   for defglobal data structures.       */
/******************************************/
static void UpdateDefglobal(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveDefglobal *bdp;

   bdp = (struct bsaveDefglobal *) buf;
   CL_UpdateConstructHeader(theEnv,&bdp->header,&DefglobalBinaryData(theEnv)->DefglobalArray[obji].header,DEFGLOBAL,
                         sizeof(struct defglobalModule),DefglobalBinaryData(theEnv)->ModuleArray,
                         sizeof(Defglobal),DefglobalBinaryData(theEnv)->DefglobalArray);

#if DEBUGGING_FUNCTIONS
   DefglobalBinaryData(theEnv)->DefglobalArray[obji].watch = DefglobalData(theEnv)->CL_WatchGlobals;
#endif
   DefglobalBinaryData(theEnv)->DefglobalArray[obji].initial = HashedExpressionPointer(bdp->initial);
   DefglobalBinaryData(theEnv)->DefglobalArray[obji].current.voidValue = VoidConstant(theEnv);
  }

/***************************************/
/* CL_ClearCL_Bload: Defglobal clear routine */
/*   when a binary load is in effect.  */
/***************************************/
static void CL_ClearCL_Bload(
  Environment *theEnv)
  {
   unsigned long i;
   size_t space;

   /*=======================================================*/
   /* Decrement in use counters for atomic values contained */
   /* in the construct headers. Also decrement data         */
   /* structures used to store the defglobal's value.       */
   /*=======================================================*/

   for (i = 0; i < DefglobalBinaryData(theEnv)->NumberOfDefglobals; i++)
     {
      CL_UnmarkConstructHeader(theEnv,&DefglobalBinaryData(theEnv)->DefglobalArray[i].header);

      CL_Release(theEnv,DefglobalBinaryData(theEnv)->DefglobalArray[i].current.header);
      if (DefglobalBinaryData(theEnv)->DefglobalArray[i].current.header->type == MULTIFIELD_TYPE)
        { CL_ReturnMultifield(theEnv,DefglobalBinaryData(theEnv)->DefglobalArray[i].current.multifieldValue); }
     }

   /*==============================================================*/
   /* Deallocate the space used for the defglobal data structures. */
   /*==============================================================*/

   space = DefglobalBinaryData(theEnv)->NumberOfDefglobals * sizeof(Defglobal);
   if (space != 0) CL_genfree(theEnv,DefglobalBinaryData(theEnv)->DefglobalArray,space);
   DefglobalBinaryData(theEnv)->NumberOfDefglobals = 0;

   /*=====================================================================*/
   /* Deallocate the space used for the defglobal module data structures. */
   /*=====================================================================*/

   space = DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules * sizeof(struct defglobalModule);
   if (space != 0) CL_genfree(theEnv,DefglobalBinaryData(theEnv)->ModuleArray,space);
   DefglobalBinaryData(theEnv)->NumberOfCL_DefglobalModules = 0;
  }

/********************************************************/
/* CL_BloadCL_DefglobalModuleReference: Returns the defglobal */
/*   module pointer for using with the bload function.  */
/********************************************************/
void *CL_BloadCL_DefglobalModuleReference(
  Environment *theEnv,
  unsigned long theIndex)
  {
   return (void *) &DefglobalBinaryData(theEnv)->ModuleArray[theIndex];
  }

#endif /* DEFGLOBAL_CONSTRUCT && (BLOAD || BLOAD_AND_BSAVE || BLOAD_ONLY) && (! RUN_TIME) */



