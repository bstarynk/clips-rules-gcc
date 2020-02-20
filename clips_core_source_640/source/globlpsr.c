   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/18/16             */
   /*                                                     */
   /*              DEFGLOBAL PARSER MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Parses the defglobal construct.                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Made the construct redefinition message more   */
/*            prominent.                                     */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Moved CL_WatchGlobals global to defglobalData.    */
/*                                                           */
/*      6.40: Added Env prefix to GetCL_EvaluationError and     */
/*            SetCL_EvaluationError functions.                  */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFGLOBAL_CONSTRUCT

#include <string.h>

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#endif
#include "constrct.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "evaluatn.h"
#include "exprnpsr.h"
#include "globlbsc.h"
#include "globldef.h"
#include "memalloc.h"
#include "modulpsr.h"
#include "modulutl.h"
#include "multifld.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "watch.h"

#include "globlpsr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   static bool                    GetVariableDefinition(Environment *,const char *,bool *,bool,struct token *);
   static void                    AddDefglobal(Environment *,CLIPSLexeme *,UDFValue *,struct expr *);
#endif

/*********************************************************************/
/* CL_ParseDefglobal: Coordinates all actions necessary for the parsing */
/*   and creation of a defglobal into the current environment.       */
/*********************************************************************/
bool CL_ParseDefglobal(
  Environment *theEnv,
  const char *readSource)
  {
   bool defglobalError = false;
#if (! RUN_TIME) && (! BLOAD_ONLY)

   struct token theToken;
   bool tokenRead = true;
   Defmodule *theModule;

   /*=====================================*/
   /* Pretty print buffer initialization. */
   /*=====================================*/

   CL_SetPPBufferStatus(theEnv,true);
   CL_FlushPPBuffer(theEnv);
   CL_SetIndentDepth(theEnv,3);
   CL_SavePPBuffer(theEnv,"(defglobal ");

   /*=================================================*/
   /* Individual defglobal constructs can't be parsed */
   /* while a binary load is in effect.               */
   /*=================================================*/

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
   if ((CL_Bloaded(theEnv) == true) && (! ConstructData(theEnv)->CL_CheckSyntaxMode))
     {
      CannotCL_LoadWithCL_BloadMessage(theEnv,"defglobal");
      return true;
     }
#endif

   /*===========================*/
   /* Look for the module name. */
   /*===========================*/

   CL_GetToken(theEnv,readSource,&theToken);
   if (theToken.tknType == SYMBOL_TOKEN)
     {
      /*=================================================*/
      /* The optional module name can't contain a module */
      /* separator like other constructs. For example,   */
      /* (defrule X::foo is OK for rules, but the right  */
      /* syntax for defglobals is (defglobal X ?*foo*.   */
      /*=================================================*/

      tokenRead = false;
      if (CL_FindModuleSeparator(theToken.lexemeValue->contents))
        {
         CL_SyntaxErrorMessage(theEnv,"defglobal");
         return true;
        }

      /*=================================*/
      /* DeteCL_rmine if the module exists. */
      /*=================================*/

      theModule = CL_FindDefmodule(theEnv,theToken.lexemeValue->contents);
      if (theModule == NULL)
        {
         CL_CantFindItemErrorMessage(theEnv,"defmodule",theToken.lexemeValue->contents,true);
         return true;
        }

      /*=========================================*/
      /* If the module name was OK, then set the */
      /* current module to the specified module. */
      /*=========================================*/

      CL_SavePPBuffer(theEnv," ");
      CL_SetCurrentModule(theEnv,theModule);
     }

   /*===========================================*/
   /* If the module name wasn't specified, then */
   /* use the current module's name in the      */
   /* defglobal's pretty print representation.  */
   /*===========================================*/

   else
     {
      CL_PPBackup(theEnv);
      CL_SavePPBuffer(theEnv,CL_DefmoduleName(CL_GetCurrentModule(theEnv)));
      CL_SavePPBuffer(theEnv," ");
      CL_SavePPBuffer(theEnv,theToken.printFoCL_rm);
     }

   /*======================*/
   /* Parse the variables. */
   /*======================*/

   while (GetVariableDefinition(theEnv,readSource,&defglobalError,tokenRead,&theToken))
     {
      tokenRead = false;

      CL_FlushPPBuffer(theEnv);
      CL_SavePPBuffer(theEnv,"(defglobal ");
      CL_SavePPBuffer(theEnv,CL_DefmoduleName(CL_GetCurrentModule(theEnv)));
      CL_SavePPBuffer(theEnv," ");
     }

#endif

   /*==================================*/
   /* Return the parsing error status. */
   /*==================================*/

   return(defglobalError);
  }

#if (! RUN_TIME) && (! BLOAD_ONLY)

/***************************************************************/
/* GetVariableDefinition: Parses and evaluates a single global */
/*   variable in a defglobal construct. Returns true if the    */
/*   variable was successfully parsed and false if a right     */
/*   parenthesis is encountered (signifying the end of the     */
/*   defglobal construct) or an error occurs. The error status */
/*   flag is also set if an error occurs.                      */
/***************************************************************/
static bool GetVariableDefinition(
  Environment *theEnv,
  const char *readSource,
  bool *defglobalError,
  bool tokenRead,
  struct token *theToken)
  {
   CLIPSLexeme *variableName;
   struct expr *assignPtr;
   UDFValue assignValue;

   /*========================================*/
   /* Get next token, which should either be */
   /* a closing parenthesis or a variable.   */
   /*========================================*/

   if (! tokenRead) CL_GetToken(theEnv,readSource,theToken);
   if (theToken->tknType == RIGHT_PARENTHESIS_TOKEN) return false;

   if (theToken->tknType == SF_VARIABLE_TOKEN)
     {
      CL_SyntaxErrorMessage(theEnv,"defglobal");
      *defglobalError = true;
      return false;
     }
   else if (theToken->tknType != GBL_VARIABLE_TOKEN)
     {
      CL_SyntaxErrorMessage(theEnv,"defglobal");
      *defglobalError = true;
      return false;
     }

   variableName = theToken->lexemeValue;

   CL_SavePPBuffer(theEnv," ");

   /*================================*/
   /* Print out compilation message. */
   /*================================*/

#if DEBUGGING_FUNCTIONS
   if ((CL_GetCL_WatchItem(theEnv,"compilations") == 1) && CL_GetPrintWhileCL_Loading(theEnv))
     {
      const char *outRouter = STDOUT;
      if (QCL_FindDefglobal(theEnv,variableName) != NULL)
        {
         outRouter = STDWRN;
         CL_PrintWarningID(theEnv,"CSTRCPSR",1,true);
         CL_WriteString(theEnv,outRouter,"Redefining defglobal: ");
        }
      else CL_WriteString(theEnv,outRouter,"Defining defglobal: ");
      CL_WriteString(theEnv,outRouter,variableName->contents);
      CL_WriteString(theEnv,outRouter,"\n");
     }
   else
#endif
     { if (CL_GetPrintWhileCL_Loading(theEnv)) CL_WriteString(theEnv,STDOUT,":"); }

   /*==================================================================*/
   /* Check for import/export conflicts from the construct definition. */
   /*==================================================================*/

#if DEFMODULE_CONSTRUCT
   if (CL_FindImportExportConflict(theEnv,"defglobal",CL_GetCurrentModule(theEnv),variableName->contents))
     {
      CL_ImportExportConflictMessage(theEnv,"defglobal",variableName->contents,NULL,NULL);
      *defglobalError = true;
      return false;
     }
#endif

   /*==============================*/
   /* The next token must be an =. */
   /*==============================*/

   CL_GetToken(theEnv,readSource,theToken);
   if (strcmp(theToken->printFoCL_rm,"=") != 0)
     {
      CL_SyntaxErrorMessage(theEnv,"defglobal");
      *defglobalError = true;
      return false;
     }

   CL_SavePPBuffer(theEnv," ");

   /*======================================================*/
   /* Parse the expression to be assigned to the variable. */
   /*======================================================*/

   assignPtr = CL_ParseAtomOrExpression(theEnv,readSource,NULL);
   if (assignPtr == NULL)
     {
      *defglobalError = true;
      return false;
     }

   /*==========================*/
   /* CL_Evaluate the expression. */
   /*==========================*/

   if (! ConstructData(theEnv)->CL_CheckSyntaxMode)
     {
      SetCL_EvaluationError(theEnv,false);
      if (CL_EvaluateExpression(theEnv,assignPtr,&assignValue))
        {
         CL_ReturnExpression(theEnv,assignPtr);
         *defglobalError = true;
         return false;
        }
     }
   else
     { CL_ReturnExpression(theEnv,assignPtr); }

   CL_SavePPBuffer(theEnv,")");

   /*======================================*/
   /* Add the variable to the global list. */
   /*======================================*/

   if (! ConstructData(theEnv)->CL_CheckSyntaxMode)
     { AddDefglobal(theEnv,variableName,&assignValue,assignPtr); }

   /*==================================================*/
   /* Return true to indicate that the global variable */
   /* definition was successfully parsed.              */
   /*==================================================*/

   return true;
  }

/*********************************************************/
/* AddDefglobal: Adds a defglobal to the current module. */
/*********************************************************/
static void AddDefglobal(
  Environment *theEnv,
  CLIPSLexeme *name,
  UDFValue *vPtr,
  struct expr *ePtr)
  {
   Defglobal *defglobalPtr;
   bool newGlobal = false;
#if DEBUGGING_FUNCTIONS
   bool globalHadCL_Watch = false;
#endif

   /*========================================================*/
   /* If the defglobal is already defined, then use the old  */
   /* data structure and substitute new values. If it hasn't */
   /* been defined, then create a new data structure.        */
   /*========================================================*/

   defglobalPtr = QCL_FindDefglobal(theEnv,name);
   if (defglobalPtr == NULL)
     {
      newGlobal = true;
      defglobalPtr = get_struct(theEnv,defglobal);
     }
   else
     {
      CL_DeinstallConstructHeader(theEnv,&defglobalPtr->header);
#if DEBUGGING_FUNCTIONS
      globalHadCL_Watch = defglobalPtr->watch;
#endif
     }

   /*===========================================*/
   /* Remove the old values from the defglobal. */
   /*===========================================*/

   if (newGlobal == false)
     {
      CL_Release(theEnv,defglobalPtr->current.header);
      if (defglobalPtr->current.header->type == MULTIFIELD_TYPE)
        { CL_ReturnMultifield(theEnv,defglobalPtr->current.multifieldValue); }

      CL_RemoveHashedExpression(theEnv,defglobalPtr->initial);
     }

   /*=======================================*/
   /* Copy the new values to the defglobal. */
   /*=======================================*/

   if (vPtr->header->type != MULTIFIELD_TYPE)
     { defglobalPtr->current.value = vPtr->value; }
   else
     { defglobalPtr->current.value = CL_CopyMultifield(theEnv,vPtr->multifieldValue); }
   CL_Retain(theEnv,defglobalPtr->current.header);

   defglobalPtr->initial = CL_AddHashedExpression(theEnv,ePtr);
   CL_ReturnExpression(theEnv,ePtr);
   DefglobalData(theEnv)->ChangeToGlobals = true;

   /*=================================*/
   /* Restore the old watch value to  */
   /* the defglobal if redefined.     */
   /*=================================*/

#if DEBUGGING_FUNCTIONS
   defglobalPtr->watch = globalHadCL_Watch ? true : DefglobalData(theEnv)->CL_WatchGlobals;
#endif

   /*======================================*/
   /* CL_Save the name and pretty print foCL_rm. */
   /*======================================*/

   defglobalPtr->header.name = name;
   defglobalPtr->header.usrData = NULL;
   defglobalPtr->header.constructType = DEFGLOBAL;
   defglobalPtr->header.env = theEnv;
   IncrementLexemeCount(name);

   CL_SavePPBuffer(theEnv,"\n");
   if (CL_GetConserveMemory(theEnv) == true)
     { defglobalPtr->header.ppFoCL_rm = NULL; }
   else
     { defglobalPtr->header.ppFoCL_rm = CL_CopyPPBuffer(theEnv); }

   defglobalPtr->inScope = true;

   /*=============================================*/
   /* If the defglobal was redefined, we're done. */
   /*=============================================*/

   if (newGlobal == false) return;

   /*===================================*/
   /* Copy the defglobal variable name. */
   /*===================================*/

   defglobalPtr->busyCount = 0;
   defglobalPtr->header.whichModule = (struct defmoduleItemHeader *)
                               CL_GetModuleItem(theEnv,NULL,CL_FindModuleItem(theEnv,"defglobal")->moduleIndex);

   /*=============================================*/
   /* Add the defglobal to the list of defglobals */
   /* for the current module.                     */
   /*=============================================*/

   CL_AddConstructToModule(&defglobalPtr->header);
  }

/*****************************************************************/
/* CL_ReplaceGlobalVariable: Replaces a global variable found in an */
/*   expression with the appropriate primitive data type which   */
/*   can later be used to retrieve the global variable's value.  */
/*****************************************************************/
bool CL_ReplaceGlobalVariable(
  Environment *theEnv,
  struct expr *ePtr)
  {
   Defglobal *theGlobal;
   unsigned int count;

   /*=================================*/
   /* Search for the global variable. */
   /*=================================*/

   theGlobal = (Defglobal *)
               CL_FindImportedConstruct(theEnv,"defglobal",NULL,ePtr->lexemeValue->contents,
                                     &count,true,NULL);

   /*=============================================*/
   /* If it wasn't found, print an error message. */
   /*=============================================*/

   if (theGlobal == NULL)
     {
      CL_GlobalReferenceErrorMessage(theEnv,ePtr->lexemeValue->contents);
      return false;
     }

   /*========================================================*/
   /* The current implementation of the defmodules shouldn't */
   /* allow a construct to be defined which would cause an   */
   /* ambiguous reference, but we'll check for it anyway.    */
   /*========================================================*/

   if (count > 1)
     {
      CL_AmbiguousReferenceErrorMessage(theEnv,"defglobal",ePtr->lexemeValue->contents);
      return false;
     }

   /*==============================================*/
   /* Replace the symbolic reference of the global */
   /* variable with a direct pointer reference.    */
   /*==============================================*/

   ePtr->type = DEFGLOBAL_PTR;
   ePtr->value = theGlobal;

   return true;
  }

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

/*****************************************************************/
/* CL_GlobalReferenceErrorMessage: Prints an error message when a   */
/*   symbolic reference to a global variable cannot be resolved. */
/*****************************************************************/
void CL_GlobalReferenceErrorMessage(
  Environment *theEnv,
  const char *variableName)
  {
   CL_PrintErrorID(theEnv,"GLOBLPSR",1,true);
   CL_WriteString(theEnv,STDERR,"\nGlobal variable ?*");
   CL_WriteString(theEnv,STDERR,variableName);
   CL_WriteString(theEnv,STDERR,"* was referenced, but is not defined.\n");
  }

#endif /* DEFGLOBAL_CONSTRUCT */



