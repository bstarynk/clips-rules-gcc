   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/04/17             */
   /*                                                     */
   /*                FACT COMMANDS MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides the facts, assert, retract, save-facts, */
/*   load-facts, set-fact-duplication, get-fact-duplication, */
/*   assert-string, and fact-index commands and functions.   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Added environment parameter to CL_GenClose.       */
/*            Added environment parameter to CL_GenOpen.        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Added code to prevent a clear command from     */
/*            being executed during fact assertions via      */
/*            Increment/Decrement_ClearReadyLocks API.        */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*      6.31: Error messages are now generated when the      */
/*            fact-index function is given a retracted       */
/*            fact.                                          */
/*                                                           */
/*      6.40: Added Env prefix to Get_EvaluationError and     */
/*            Set_EvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to CL_Get_HaltExecution and       */
/*            Set_HaltExecution functions.                    */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*            CL_Watch facts for modify command only prints     */
/*            changed slots.                                 */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <string.h>

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include "argacces.h"
#include "constant.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "extnfunc.h"
#include "facthsh.h"
#include "factmch.h"
#include "factmngr.h"
#include "factrhs.h"
#include "match.h"
#include "memalloc.h"
#include "modulutl.h"
#include "multifld.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "strngrtr.h"
#include "sysdep.h"
#include "tmpltdef.h"
#include "tmpltfun.h"
#include "tmpltpsr.h"
#include "tmpltutl.h"

#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
#include "bload.h"
#endif

#include "factcom.h"

#define INVALID     -2L
#define UNSPECIFIED -1L

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static struct expr            *CL_AssertParse(Environment *,struct expr *,const char *);
#if DEBUGGING_FUNCTIONS
   static long long               Get_FactsArgument(UDFContext *);
#endif
   static struct expr            *Standard_LoadFact(Environment *,const char *,struct token *);
   static Deftemplate           **Get_Save_Facts_DeftemplateNames(Environment *,struct expr *,int,
                                                               unsigned int *,bool *);

/***************************************/
/* CL_FactCommandDefinitions: Initializes */
/*   fact commands and functions.      */
/***************************************/
void CL_FactCommandDefinitions(
  Environment *theEnv)
  {
#if ! RUN_TIME
#if DEBUGGING_FUNCTIONS
   CL_AddUDF(theEnv,"facts","v",0,4,"l;*",CL_FactsCommand,"CL_FactsCommand",NULL);
#endif

   CL_AddUDF(theEnv,"assert","bf",0,UNBOUNDED,NULL,CL_AssertCommand,"CL_AssertCommand",NULL);
   CL_AddUDF(theEnv,"retract", "v",1,UNBOUNDED,"fly",CL_RetractCommand,"CL_RetractCommand",NULL);
   CL_AddUDF(theEnv,"assert-string","bf",1,1,"s",CL_AssertStringFunction,"CL_AssertStringFunction",NULL);
   CL_AddUDF(theEnv,"str-assert","bf",1,1,"s",CL_AssertStringFunction,"CL_AssertStringFunction",NULL);

   CL_AddUDF(theEnv,"get-fact-duplication","b",0,0,NULL,CL_GetFactDuplicationCommand,"CL_GetFactDuplicationCommand", NULL);
   CL_AddUDF(theEnv,"set-fact-duplication","b",1,1,NULL,CL_SetFactDuplicationCommand,"CL_SetFactDuplicationCommand", NULL);

   CL_AddUDF(theEnv,"save-facts","b",1,UNBOUNDED,"y;sy",CL_Save_FactsCommand,"CL_Save_FactsCommand",NULL);
   CL_AddUDF(theEnv,"load-facts","b",1,1,"sy",CL_Load_FactsCommand,"CL_Load_FactsCommand",NULL);
   CL_AddUDF(theEnv,"fact-index","l",1,1,"f",CL_FactIndexFunction,"CL_FactIndexFunction",NULL);

   CL_FuncSeqOvlFlags(theEnv,"assert",false,false);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
   CL_AddFunctionParser(theEnv,"assert",CL_AssertParse);
  }

/***************************************/
/* CL_AssertCommand: H/L access routine   */
/*   for the assert function.          */
/***************************************/
void CL_AssertCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSValue *theField;
   UDFValue theValue;
   struct expr *theExpression;
   struct templateSlot *slotPtr;
   Fact *newFact;
   bool error = false;
   int i;
   Fact *theFact;

   /*================================*/
   /* Get the deftemplate associated */
   /* with the fact being asserted.  */
   /*================================*/

   theExpression = GetFirstArgument();
   theDeftemplate = (Deftemplate *) theExpression->value;

   /*=======================================*/
   /* Create the fact and store the name of */
   /* the deftemplate as the 1st field.     */
   /*=======================================*/

   if (theDeftemplate->implied == false)
     {
      newFact = CL_CreateFactBySize(theEnv,theDeftemplate->numberOfSlots);
      slotPtr = theDeftemplate->slotList;
     }
   else
     {
      newFact = CL_CreateFactBySize(theEnv,1);
      if (theExpression->nextArg == NULL)
        {
         newFact->theProposition.contents[0].multifieldValue = CL_CreateUnmanagedMultifield(theEnv,0L);
        }
      slotPtr = NULL;
     }

   newFact->whichDeftemplate = theDeftemplate;

   /*===================================================*/
   /* CL_Evaluate the expression associated with each slot */
   /* and store the result in the appropriate slot of   */
   /* the newly created fact.                           */
   /*===================================================*/

   Increment_ClearReadyLocks(theEnv);

   theField = newFact->theProposition.contents;

   for (theExpression = theExpression->nextArg, i = 0;
        theExpression != NULL;
        theExpression = theExpression->nextArg, i++)
     {
      /*===================================================*/
      /* CL_Evaluate the expression to be stored in the slot. */
      /*===================================================*/

      CL_EvaluateExpression(theEnv,theExpression,&theValue);

      /*============================================================*/
      /* A multifield value can't be stored in a single field slot. */
      /*============================================================*/

      if ((slotPtr != NULL) ?
          (slotPtr->multislot == false) && (theValue.header->type == MULTIFIELD_TYPE) :
          false)
        {
         CL_MultiIntoSingleFieldSlotError(theEnv,slotPtr,theDeftemplate);
         theValue.value = FalseSymbol(theEnv);
         error = true;
        }

      /*==============================*/
      /* Store the value in the slot. */
      /*==============================*/

      theField[i].value = theValue.value;

      /*========================================*/
      /* Get the info_rmation for the next slot. */
      /*========================================*/

      if (slotPtr != NULL) slotPtr = slotPtr->next;
     }

   Decrement_ClearReadyLocks(theEnv);

   /*============================================*/
   /* If an error occured while generating the   */
   /* fact's slot values, then abort the assert. */
   /*============================================*/

   if (error)
     {
      CL_ReturnFact(theEnv,newFact);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*================================*/
   /* Add the fact to the fact-list. */
   /*================================*/

   theFact = CL_Assert(newFact);

   /*========================================*/
   /* The asserted fact is the return value. */
   /*========================================*/

   if (theFact != NULL)
     { returnValue->factValue = theFact; }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }

   return;
  }

/****************************************/
/* CL_RetractCommand: H/L access routine   */
/*   for the retract command.           */
/****************************************/
void CL_RetractCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   long long factIndex;
   Fact *ptr;
   UDFValue theArg;

   /*================================*/
   /* Iterate through each argument. */
   /*================================*/

   while (UDFHasNextArgument(context))
     {
      /*========================*/
      /* CL_Evaluate the argument. */
      /*========================*/

      if (! CL_UDFNextArgument(context,INTEGER_BIT | FACT_ADDRESS_BIT | SYMBOL_BIT,&theArg))
        { return; }

      /*======================================*/
      /* If the argument evaluates to a fact  */
      /* address, we can directly retract it. */
      /*======================================*/

      if (CVIsType(&theArg,FACT_ADDRESS_BIT))
        { CL_Retract(theArg.factValue); }

      /*===============================================*/
      /* If the argument evaluates to an integer, then */
      /* it's assumed to be the fact index of the fact */
      /* to be retracted.                              */
      /*===============================================*/

      else if (CVIsType(&theArg,INTEGER_BIT))
        {
         /*==========================================*/
         /* A fact index must be a positive integer. */
         /*==========================================*/

         factIndex = theArg.integerValue->contents;
         if (factIndex < 0)
           {
            CL_UDFInvalidArgumentMessage(context,"fact-address, fact-index, or the symbol *");
            return;
           }

         /*================================================*/
         /* See if a fact with the specified index exists. */
         /*================================================*/

         ptr = CL_FindIndexedFact(theEnv,factIndex);

         /*=====================================*/
         /* If the fact exists then retract it, */
         /* otherwise print an error message.   */
         /*=====================================*/

         if (ptr != NULL)
           { CL_Retract(ptr); }
         else
           {
            char tempBuffer[20];
            CL_gensprintf(tempBuffer,"f-%lld",factIndex);
            CL_CantFindItemErrorMessage(theEnv,"fact",tempBuffer,false);
           }
        }

      /*============================================*/
      /* Otherwise if the argument evaluates to the */
      /* symbol *, then all facts are retracted.    */
      /*============================================*/

      else if ((CVIsType(&theArg,SYMBOL_BIT)) ?
               (strcmp(theArg.lexemeValue->contents,"*") == 0) : false)
        {
         CL_RetractAll_Facts(theEnv);
         return;
        }

      /*============================================*/
      /* Otherwise the argument has evaluated to an */
      /* illegal value for the retract command.     */
      /*============================================*/

      else
        {
         CL_UDFInvalidArgumentMessage(context,"fact-address, fact-index, or the symbol *");
         Set_EvaluationError(theEnv,true);
        }
     }
  }

/***************************************************/
/* CL_SetFactDuplicationCommand: H/L access routine   */
/*   for the set-fact-duplication command.         */
/***************************************************/
void CL_SetFactDuplicationCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   /*=====================================================*/
   /* Get the old value of the fact duplication behavior. */
   /*=====================================================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_GetFactDuplication(theEnv));

   /*========================*/
   /* CL_Evaluate the argument. */
   /*========================*/

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&theArg))
     { return; }

   /*===============================================================*/
   /* If the argument evaluated to false, then the fact duplication */
   /* behavior is disabled, otherwise it is enabled.                */
   /*===============================================================*/

   CL_SetFactDuplication(theEnv,theArg.value != FalseSymbol(theEnv));
  }

/***************************************************/
/* CL_GetFactDuplicationCommand: H/L access routine   */
/*   for the get-fact-duplication command.         */
/***************************************************/
void CL_GetFactDuplicationCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_GetFactDuplication(theEnv));
  }

/*******************************************/
/* CL_FactIndexFunction: H/L access routine   */
/*   for the fact-index function.          */
/*******************************************/
void CL_FactIndexFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   /*======================================*/
   /* The argument must be a fact address. */
   /*======================================*/

   if (! CL_UDFFirstArgument(context,FACT_ADDRESS_BIT,&theArg))
     { return; }

   /*================================================*/
   /* Return the fact index associated with the fact */
   /* address. If the fact has been retracted, then  */
   /* return -1 for the fact index.                  */
   /*================================================*/

   if (theArg.factValue->garbage)
     {
      CL_Fact_RetractedErrorMessage(theEnv,theArg.factValue);
      returnValue->integerValue = CL_CreateInteger(theEnv,-1L);
      return;
     }

   returnValue->integerValue = CL_CreateInteger(theEnv,CL_FactIndex(theArg.factValue));
  }

#if DEBUGGING_FUNCTIONS

/**************************************/
/* CL_FactsCommand: H/L access routine   */
/*   for the facts command.           */
/**************************************/
void CL_FactsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   long long start = UNSPECIFIED, end = UNSPECIFIED, max = UNSPECIFIED;
   Defmodule *theModule;
   UDFValue theArg;

   /*==================================*/
   /* The default module for the facts */
   /* command is the current module.   */
   /*==================================*/

   theModule = CL_GetCurrentModule(theEnv);

   /*==========================================*/
   /* If no arguments were specified, then use */
   /* the default values to list the facts.    */
   /*==========================================*/

   if (! UDFHasNextArgument(context))
     {
      CL_Facts(theEnv,STDOUT,theModule,start,end,max);
      return;
     }

   /*========================================================*/
   /* Since there are one or more arguments, see if a module */
   /* or start index was specified as the first argument.    */
   /*========================================================*/

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT | INTEGER_BIT,&theArg)) return;

   /*===============================================*/
   /* If the first argument is a symbol, then check */
   /* to see that a valid module was specified.     */
   /*===============================================*/

   if (CVIsType(&theArg,SYMBOL_BIT))
     {
      theModule = CL_FindDefmodule(theEnv,theArg.lexemeValue->contents);
      if ((theModule == NULL) && (strcmp(theArg.lexemeValue->contents,"*") != 0))
        {
         Set_EvaluationError(theEnv,true);
         CL_CantFindItemErrorMessage(theEnv,"defmodule",theArg.lexemeValue->contents,true);
         return;
        }

      if ((start = Get_FactsArgument(context)) == INVALID) return;
     }

   /*================================================*/
   /* Otherwise if the first argument is an integer, */
   /* check to see that a valid index was specified. */
   /*================================================*/

   else if (CVIsType(&theArg,INTEGER_BIT))
     {
      start = theArg.integerValue->contents;
      if (start < 0)
        {
         CL_ExpectedTypeError1(theEnv,"facts",1,"symbol or 'positive number'");
         CL_UDFThrowError(context);
         return;
        }
     }

   /*==========================================*/
   /* Otherwise the first argument is invalid. */
   /*==========================================*/

   else
     {
      CL_UDFInvalidArgumentMessage(context,"symbol or 'positive number'");
      CL_UDFThrowError(context);
      return;
     }

   /*==========================*/
   /* Get the other arguments. */
   /*==========================*/

   if ((end = Get_FactsArgument(context)) == INVALID) return;
   if ((max = Get_FactsArgument(context)) == INVALID) return;

   /*=================*/
   /* List the facts. */
   /*=================*/

   CL_Facts(theEnv,STDOUT,theModule,start,end,max);
  }

/**************************************************/
/* CL_Facts: C access routine for the facts command. */
/**************************************************/
void CL_Facts(
  Environment *theEnv,
  const char *logicalName,
  Defmodule *theModule,
  long long start,
  long long end,
  long long max)
  {
   Fact *factPtr;
   unsigned long count = 0;
   Defmodule *oldModule;
   bool allModules = false;

   /*==========================*/
   /* CL_Save the current module. */
   /*==========================*/

   oldModule = CL_GetCurrentModule(theEnv);

   /*=========================================================*/
   /* Dete_rmine if facts from all modules are to be displayed */
   /* or just facts from the current module.                  */
   /*=========================================================*/

   if (theModule == NULL) allModules = true;
   else CL_SetCurrentModule(theEnv,theModule);

   /*=====================================*/
   /* Get the first fact to be displayed. */
   /*=====================================*/

   if (allModules) factPtr = CL_GetNextFact(theEnv,NULL);
   else factPtr = CL_GetNextFactInScope(theEnv,NULL);

   /*===============================*/
   /* Display facts until there are */
   /* no more facts to display.     */
   /*===============================*/

   while (factPtr != NULL)
     {
      /*==================================================*/
      /* Abort the display of facts if the CL_Halt Execution */
      /* flag has been set (no_rmally by user action).     */
      /*==================================================*/

      if (CL_Get_HaltExecution(theEnv) == true)
        {
         CL_SetCurrentModule(theEnv,oldModule);
         return;
        }

      /*===============================================*/
      /* If the maximum fact index of facts to display */
      /* has been reached, then stop displaying facts. */
      /*===============================================*/

      if ((factPtr->factIndex > end) && (end != UNSPECIFIED))
        {
         CL_PrintTally(theEnv,logicalName,count,"fact","facts");
         CL_SetCurrentModule(theEnv,oldModule);
         return;
        }

      /*================================================*/
      /* If the maximum number of facts to be displayed */
      /* has been reached, then stop displaying facts.  */
      /*================================================*/

      if (max == 0)
        {
         CL_PrintTally(theEnv,logicalName,count,"fact","facts");
         CL_SetCurrentModule(theEnv,oldModule);
         return;
        }

      /*======================================================*/
      /* If the index of the fact is greater than the minimum */
      /* starting fact index, then display the fact.          */
      /*======================================================*/

      if (factPtr->factIndex >= start)
        {
         CL_PrintFactWithIdentifier(theEnv,logicalName,factPtr,NULL);
         CL_WriteString(theEnv,logicalName,"\n");
         count++;
         if (max > 0) max--;
        }

      /*========================================*/
      /* Proceed to the next fact to be listed. */
      /*========================================*/

      if (allModules) factPtr = CL_GetNextFact(theEnv,factPtr);
      else factPtr = CL_GetNextFactInScope(theEnv,factPtr);
     }

   /*===================================================*/
   /* Print the total of the number of facts displayed. */
   /*===================================================*/

   CL_PrintTally(theEnv,logicalName,count,"fact","facts");

   /*=============================*/
   /* Restore the current module. */
   /*=============================*/

   CL_SetCurrentModule(theEnv,oldModule);
  }

/****************************************************************/
/* Get_FactsArgument: Returns an argument for the facts command. */
/*  A return value of -1 indicates that no value was specified. */
/*  A return value of -2 indicates that the value specified is  */
/*  invalid.                                                    */
/****************************************************************/
static long long Get_FactsArgument(
  UDFContext *context)
  {
   long long factIndex;
   UDFValue theArg;

   if (! UDFHasNextArgument(context)) return(UNSPECIFIED);

   if (! CL_UDFNextArgument(context,INTEGER_BIT,&theArg))
     { return(INVALID); }

   factIndex = theArg.integerValue->contents;

   if (factIndex < 0)
     {
      CL_UDFInvalidArgumentMessage(context,"positive number");
      CL_UDFThrowError(context);
      return(INVALID);
     }

   return(factIndex);
  }

#endif /* DEBUGGING_FUNCTIONS */

/**********************************************/
/* CL_AssertStringFunction: H/L access routine   */
/*   for the assert-string function.          */
/**********************************************/
void CL_AssertStringFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;
   Fact *theFact;

   /*=====================================================*/
   /* Check for the correct number and type of arguments. */
   /*=====================================================*/

   if (! CL_UDFFirstArgument(context,STRING_BIT,&theArg))
     { return; }

   /*==========================================*/
   /* Call the driver routine for converting a */
   /* string to a fact and then assert it.     */
   /*==========================================*/

   theFact = CL_AssertString(theEnv,theArg.lexemeValue->contents);
   if (theFact != NULL)
     { returnValue->factValue = theFact; }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/******************************************/
/* CL_Save_FactsCommand: H/L access routine   */
/*   for the save-facts command.          */
/******************************************/
void CL_Save_FactsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileName;
   unsigned int numArgs;
   CL_SaveScope saveCode = LOCAL_SAVE;
   const char *argument;
   UDFValue theValue;
   struct expr *theList = NULL;

   /*============================================*/
   /* Check for the correct number of arguments. */
   /*============================================*/

   numArgs = CL_UDFArgumentCount(context);

   /*=================================================*/
   /* Get the file name to which facts will be saved. */
   /*=================================================*/

   if ((fileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*=============================================================*/
   /* If specified, the second argument to save-facts indicates   */
   /* whether just facts local to the current module or all facts */
   /* visible to the current module will be saved.                */
   /*=============================================================*/

   if (numArgs > 1)
     {
      if (! CL_UDFNextArgument(context,SYMBOL_BIT,&theValue))
        {
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }

      argument = theValue.lexemeValue->contents;

      if (strcmp(argument,"local") == 0)
        { saveCode = LOCAL_SAVE; }
      else if (strcmp(argument,"visible") == 0)
        { saveCode = VISIBLE_SAVE; }
      else
        {
         CL_ExpectedTypeError1(theEnv,"save-facts",2,"symbol with value local or visible");
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
     }

   /*======================================================*/
   /* Subsequent arguments indicate that only those facts  */
   /* associated with the specified deftemplates should be */
   /* saved to the file.                                   */
   /*======================================================*/

   if (numArgs > 2) theList = GetFirstArgument()->nextArg->nextArg;

   /*====================================*/
   /* Call the CL_Save_Facts driver routine. */
   /*====================================*/

   if (CL_Save_FactsDriver(theEnv,fileName,saveCode,theList) == false)
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
   else
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
  }

/******************************************/
/* CL_Load_FactsCommand: H/L access routine   */
/*   for the load-facts command.          */
/******************************************/
void CL_Load_FactsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileName;

   /*====================================================*/
   /* Get the file name from which facts will be loaded. */
   /*====================================================*/

   if ((fileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*====================================*/
   /* Call the CL_Load_Facts driver routine. */
   /*====================================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_Load_Facts(theEnv,fileName));
  }

/***********************************************************/
/* CL_Save_Facts: C access routine for the save-facts command. */
/***********************************************************/
bool CL_Save_Facts(
  Environment *theEnv,
  const char *fileName,
  CL_SaveScope saveCode)
  {
   return CL_Save_FactsDriver(theEnv,fileName,saveCode,NULL);
  }

/*****************************************************************/
/* CL_Save_FactsDriver: C access routine for the save-facts command. */
/*****************************************************************/
bool CL_Save_FactsDriver(
  Environment *theEnv,
  const char *fileName,
  CL_SaveScope saveCode,
  struct expr *theList)
  {
   bool tempValue1, tempValue2, tempValue3;
   Fact *theFact;
   FILE *filePtr;
   Defmodule *theModule;
   Deftemplate **deftemplateArray;
   unsigned int count, i;
   bool printFact, error;

   /*======================================================*/
   /* Open the file. Use either "fast save" or I/O Router. */
   /*======================================================*/

   if ((filePtr = CL_GenOpen(theEnv,fileName,"w")) == NULL)
     {
      CL_OpenErrorMessage(theEnv,"save-facts",fileName);
      return false;
     }

   SetFast_Save(theEnv,filePtr);

   /*===========================================*/
   /* Set the print flags so that addresses and */
   /* strings are printed properly to the file. */
   /*===========================================*/

   tempValue1 = PrintUtilityData(theEnv)->PreserveEscapedCharacters;
   PrintUtilityData(theEnv)->PreserveEscapedCharacters = true;
   tempValue2 = PrintUtilityData(theEnv)->AddressesToStrings;
   PrintUtilityData(theEnv)->AddressesToStrings = true;
   tempValue3 = PrintUtilityData(theEnv)->InstanceAddressesToNames;
   PrintUtilityData(theEnv)->InstanceAddressesToNames = true;

   /*===================================================*/
   /* Dete_rmine the list of specific facts to be saved. */
   /*===================================================*/

   deftemplateArray = Get_Save_Facts_DeftemplateNames(theEnv,theList,saveCode,&count,&error);

   if (error)
     {
      PrintUtilityData(theEnv)->PreserveEscapedCharacters = tempValue1;
      PrintUtilityData(theEnv)->AddressesToStrings = tempValue2;
      PrintUtilityData(theEnv)->InstanceAddressesToNames = tempValue3;
      CL_GenClose(theEnv,filePtr);
      SetFast_Save(theEnv,NULL);
      return false;
     }

   /*=================*/
   /* CL_Save the facts. */
   /*=================*/

   theModule = CL_GetCurrentModule(theEnv);

   for (theFact = CL_GetNextFactInScope(theEnv,NULL);
        theFact != NULL;
        theFact = CL_GetNextFactInScope(theEnv,theFact))
     {
      /*===========================================================*/
      /* If we're doing a local save and the facts's corresponding */
      /* deftemplate isn't in the current module, then don't save  */
      /* the fact.                                                 */
      /*===========================================================*/

      if ((saveCode == LOCAL_SAVE) &&
          (theFact->whichDeftemplate->header.whichModule->theModule != theModule))
        { printFact = false; }

      /*=====================================================*/
      /* Otherwise, if the list of facts to be printed isn't */
      /* restricted, then set the print flag to true.        */
      /*=====================================================*/

      else if (theList == NULL)
        { printFact = true; }

      /*=======================================================*/
      /* Otherwise see if the fact's corresponding deftemplate */
      /* is in the list of deftemplates whose facts are to be  */
      /* saved. If it's in the list, then set the print flag   */
      /* to true, otherwise set it to false.                   */
      /*=======================================================*/

      else
        {
         printFact = false;
         for (i = 0; i < count; i++)
           {
            if (deftemplateArray[i] == theFact->whichDeftemplate)
              {
               printFact = true;
               break;
              }
           }
        }

      /*===================================*/
      /* If the print flag is set to true, */
      /* then save the fact to the file.   */
      /*===================================*/

      if (printFact)
        {
         CL_PrintFact(theEnv,(char *) filePtr,theFact,false,false,NULL);
         CL_WriteString(theEnv,(char *) filePtr,"\n");
        }
     }

   /*==========================*/
   /* Restore the print flags. */
   /*==========================*/

   PrintUtilityData(theEnv)->PreserveEscapedCharacters = tempValue1;
   PrintUtilityData(theEnv)->AddressesToStrings = tempValue2;
   PrintUtilityData(theEnv)->InstanceAddressesToNames = tempValue3;

   /*=================*/
   /* Close the file. */
   /*=================*/

   CL_GenClose(theEnv,filePtr);
   SetFast_Save(theEnv,NULL);

   /*==================================*/
   /* Free the deftemplate name array. */
   /*==================================*/

   if (theList != NULL)
     { CL_rm(theEnv,deftemplateArray,sizeof(Deftemplate *) * count); }

   /*===================================*/
   /* Return true to indicate no errors */
   /* occurred while saving the facts.  */
   /*===================================*/

   return true;
  }

/*******************************************************************/
/* Get_Save_Facts_DeftemplateNames: Retrieves the list of deftemplate */
/*   names for saving specific facts with the save-facts command.  */
/*******************************************************************/
static Deftemplate **Get_Save_Facts_DeftemplateNames(
  Environment *theEnv,
  struct expr *theList,
  int saveCode,
  unsigned int *count,
  bool *error)
  {
   struct expr *tempList;
   Deftemplate **deftemplateArray;
   UDFValue tempArg;
   unsigned int i, tempCount;
   Deftemplate *theDeftemplate = NULL;

   /*=============================*/
   /* Initialize the error state. */
   /*=============================*/

   *error = false;

   /*=====================================================*/
   /* If no deftemplate names were specified as arguments */
   /* then the deftemplate name list is empty.            */
   /*=====================================================*/

   if (theList == NULL)
     {
      *count = 0;
      return NULL;
     }

   /*======================================*/
   /* Dete_rmine the number of deftemplate  */
   /* names to be stored in the name list. */
   /*======================================*/

   for (tempList = theList, *count = 0;
        tempList != NULL;
        tempList = tempList->nextArg, (*count)++)
     { /* Do Nothing */ }

   /*=========================================*/
   /* Allocate the storage for the name list. */
   /*=========================================*/

   deftemplateArray = (Deftemplate **) CL_gm2(theEnv,sizeof(Deftemplate *) * *count);

   /*=====================================*/
   /* Loop through each of the arguments. */
   /*=====================================*/

   for (tempList = theList, i = 0;
        i < *count;
        tempList = tempList->nextArg, i++)
     {
      /*========================*/
      /* CL_Evaluate the argument. */
      /*========================*/

      CL_EvaluateExpression(theEnv,tempList,&tempArg);

      if (CL_EvaluationData(theEnv)->CL_EvaluationError)
        {
         *error = true;
         CL_rm(theEnv,deftemplateArray,sizeof(Deftemplate *) * *count);
         return NULL;
        }

      /*======================================*/
      /* A deftemplate name must be a symbol. */
      /*======================================*/

      if (tempArg.header->type != SYMBOL_TYPE)
        {
         *error = true;
         CL_ExpectedTypeError1(theEnv,"save-facts",3+i,"symbol");
         CL_rm(theEnv,deftemplateArray,sizeof(Deftemplate *) * *count);
         return NULL;
        }

      /*===================================================*/
      /* Find the deftemplate. For a local save, look only */
      /* in the current module. For a visible save, look   */
      /* in all visible modules.                           */
      /*===================================================*/

      if (saveCode == LOCAL_SAVE)
        {
         theDeftemplate = CL_FindDeftemplateInModule(theEnv,tempArg.lexemeValue->contents);
         if (theDeftemplate == NULL)
           {
            *error = true;
            CL_ExpectedTypeError1(theEnv,"save-facts",3+i,"'local deftemplate name'");
            CL_rm(theEnv,deftemplateArray,sizeof(Deftemplate *) * *count);
            return NULL;
           }
        }
      else if (saveCode == VISIBLE_SAVE)
        {
         theDeftemplate = (Deftemplate *)
           CL_FindImportedConstruct(theEnv,"deftemplate",NULL,
                                 tempArg.lexemeValue->contents,
                                 &tempCount,true,NULL);
         if (theDeftemplate == NULL)
           {
            *error = true;
            CL_ExpectedTypeError1(theEnv,"save-facts",3+i,"'visible deftemplate name'");
            CL_rm(theEnv,deftemplateArray,sizeof(Deftemplate *) * *count);
            return NULL;
           }
        }

      /*==================================*/
      /* Add a pointer to the deftemplate */
      /* to the array being created.      */
      /*==================================*/

      deftemplateArray[i] = theDeftemplate;
     }

   /*===================================*/
   /* Return the array of deftemplates. */
   /*===================================*/

   return deftemplateArray;
  }

/***********************************************************/
/* CL_Load_Facts: C access routine for the load-facts command. */
/***********************************************************/
bool CL_Load_Facts(
  Environment *theEnv,
  const char *fileName)
  {
   FILE *filePtr;
   struct token theToken;
   struct expr *testPtr;
   UDFValue rv;
   
   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/
   
   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     { CL_ResetErrorFlags(theEnv); }

   /*======================================================*/
   /* Open the file. Use either "fast save" or I/O Router. */
   /*======================================================*/

   if ((filePtr = CL_GenOpen(theEnv,fileName,"r")) == NULL)
     {
      CL_OpenErrorMessage(theEnv,"load-facts",fileName);
      return false;
     }

   SetFast_Load(theEnv,filePtr);

   /*=================*/
   /* CL_Load the facts. */
   /*=================*/

   theToken.tknType = LEFT_PARENTHESIS_TOKEN;
   while (theToken.tknType != STOP_TOKEN)
     {
      testPtr = Standard_LoadFact(theEnv,(char *) filePtr,&theToken);
      if (testPtr == NULL) theToken.tknType = STOP_TOKEN;
      else CL_EvaluateExpression(theEnv,testPtr,&rv);
      CL_ReturnExpression(theEnv,testPtr);
     }

   /*=================*/
   /* Close the file. */
   /*=================*/

   SetFast_Load(theEnv,NULL);
   CL_GenClose(theEnv,filePtr);

   /*================================================*/
   /* Return true if no error occurred while loading */
   /* the facts, otherwise return false.             */
   /*================================================*/

   if (CL_EvaluationData(theEnv)->CL_EvaluationError) return false;
   return true;
  }

/******************************************/
/* CL_Load_FactsFromString: C access routine. */
/******************************************/
bool CL_Load_FactsFromString(
  Environment *theEnv,
  const char *theString,
  size_t theMax)
  {
   const char *theStrRouter = "*** load-facts-from-string ***";
   struct token theToken;
   struct expr *testPtr;
   UDFValue rv;
   
   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/
   
   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     { CL_ResetErrorFlags(theEnv); }

   /*==========================*/
   /* Initialize string router */
   /*==========================*/

   if ((theMax == SIZE_MAX) ? (! CL_OpenStringSource(theEnv,theStrRouter,theString,0)) :
                              (! CL_OpenTextSource(theEnv,theStrRouter,theString,0,theMax)))
     return false;

   /*=================*/
   /* CL_Load the facts. */
   /*=================*/

   theToken.tknType = LEFT_PARENTHESIS_TOKEN;
   while (theToken.tknType != STOP_TOKEN)
     {
      testPtr = Standard_LoadFact(theEnv,theStrRouter,&theToken);
      if (testPtr == NULL) theToken.tknType = STOP_TOKEN;
      else CL_EvaluateExpression(theEnv,testPtr,&rv);
      CL_ReturnExpression(theEnv,testPtr);
     }

   /*=================*/
   /* Close router.   */
   /*=================*/

   CL_CloseStringSource(theEnv,theStrRouter);

   /*================================================*/
   /* Return true if no error occurred while loading */
   /* the facts, otherwise return false.             */
   /*================================================*/

   if (CL_EvaluationData(theEnv)->CL_EvaluationError) return false;
   return true;
  }

/**************************************************************************/
/* Standard_LoadFact: CL_Loads a single fact from the specified logical name. */
/**************************************************************************/
static struct expr *Standard_LoadFact(
  Environment *theEnv,
  const char *logicalName,
  struct token *theToken)
  {
   bool error = false;
   struct expr *temp;

   CL_GetToken(theEnv,logicalName,theToken);
   if (theToken->tknType != LEFT_PARENTHESIS_TOKEN) return NULL;

   temp = CL_GenConstant(theEnv,FCALL,CL_FindFunction(theEnv,"assert"));
   temp->argList = CL_GetRHSPattern(theEnv,logicalName,theToken,&error,
                                  true,false,true,RIGHT_PARENTHESIS_TOKEN);

   if (error == true)
     {
      CL_WriteString(theEnv,STDERR,"Function load-facts encountered an error\n");
      Set_EvaluationError(theEnv,true);
      CL_ReturnExpression(theEnv,temp);
      return NULL;
     }

   if (CL_ExpressionContainsVariables(temp,true))
     {
      CL_ReturnExpression(theEnv,temp);
      return NULL;
     }

   return(temp);
  }

/****************************************************************/
/* CL_AssertParse: Driver routine for parsing the assert function. */
/****************************************************************/
static struct expr *CL_AssertParse(
  Environment *theEnv,
  struct expr *top,
  const char *logicalName)
  {
   bool error;
   struct expr *rv;
   struct token theToken;

   CL_ReturnExpression(theEnv,top);
   CL_SavePPBuffer(theEnv," ");
   CL_IncrementIndentDepth(theEnv,8);
   rv = CL_BuildRHS_Assert(theEnv,logicalName,&theToken,&error,true,true,"assert command");
   CL_DecrementIndentDepth(theEnv,8);
   return(rv);
  }

#endif /* DEFTEMPLATE_CONSTRUCT */


