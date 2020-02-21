   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/01/16             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Deffunction Parsing Routines                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Gary D. Riley                                        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            If the last construct in a loaded file is a    */
/*            deffunction or defmethod with no closing right */
/*            parenthesis, an error should be issued, but is */
/*            not. DR0872                                    */
/*                                                           */
/*            Added pragmas to prevent unused variable       */
/*            warnings.                                      */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            ENVIRONMENT_API_ONLY no longer supported.      */
/*                                                           */
/*            CL_GetConstructNameAndComment API change.         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
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

#if DEFFUNCTION_CONSTRUCT && (! BLOAD_ONLY) && (! RUN_TIME)

#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif

#if DEFRULE_CONSTRUCT
#include "network.h"
#endif

#if DEFGENERIC_CONSTRUCT
#include "genrccom.h"
#endif

#include "constant.h"
#include "cstrccom.h"
#include "cstrcpsr.h"
#include "constrct.h"
#include "dffnxfun.h"
#include "envrnmnt.h"
#include "expressn.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "modulutl.h"
#include "pprint.h"
#include "prccode.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "symbol.h"

#include "dffnxpsr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static bool                    ValidCL_DeffunctionName(Environment *,const char *);
   static Deffunction            *AddDeffunction(Environment *,CLIPSLexeme *,Expression *,unsigned short,unsigned short,unsigned short,bool);

/***************************************************************************
  NAME         : CL_ParseDeffunction
  DESCRIPTION  : Parses the deffunction construct
  INPUTS       : The input logical name
  RETURNS      : False if successful parse, true otherwise
  SIDE EFFECTS : Creates valid deffunction definition
  NOTES        : H/L Syntax :
                 (deffunction <name> [<comment>]
                    (<single-field-varible>* [<multifield-variable>])
                    <action>*)
 ***************************************************************************/
bool CL_ParseDeffunction(
  Environment *theEnv,
  const char *readSource)
  {
   CLIPSLexeme *deffunctionName;
   Expression *actions;
   Expression *parameterList;
   CLIPSLexeme *wildcard;
   unsigned short min, max;
   unsigned short lvars;
   bool deffunctionError = false;
   bool overwrite = false;
   unsigned short owMin = 0, owMax = 0;
   Deffunction *dptr;
   struct token inputToken;
   
   CL_SetPPBufferStatus(theEnv,true);

   CL_FlushPPBuffer(theEnv);
   CL_SetIndentDepth(theEnv,3);
   CL_SavePPBuffer(theEnv,"(deffunction ");

#if BLOAD || BLOAD_AND_BSAVE
   if ((CL_Bloaded(theEnv) == true) && (! ConstructData(theEnv)->CL_CheckSyntaxMode))
     {
      CannotCL_LoadWithCL_BloadMessage(theEnv,"deffunctions");
      return true;
     }
#endif

   /*=======================================================*/
   /* Parse the name and comment fields of the deffunction. */
   /*=======================================================*/

   deffunctionName = CL_GetConstructNameAndComment(theEnv,readSource,&inputToken,"deffunction",
                                                (CL_FindConstructFunction *) CL_FindDeffunctionInModule,
                                                NULL,
                                                "!",true,true,true,false);

   if (deffunctionName == NULL)
     { return true; }

   if (ValidCL_DeffunctionName(theEnv,deffunctionName->contents) == false)
     { return true; }

   /*==========================*/
   /* Parse the argument list. */
   /*==========================*/

   parameterList = CL_ParseProcParameters(theEnv,readSource,&inputToken,
                                       NULL,&wildcard,&min,&max,&deffunctionError,NULL);
   if (deffunctionError)
     { return true; }

   /*===================================================================*/
   /* Go ahead and add the deffunction so it can be recursively called. */
   /*===================================================================*/

   if (ConstructData(theEnv)->CL_CheckSyntaxMode)
     {
      dptr = CL_FindDeffunctionInModule(theEnv,deffunctionName->contents);
      if (dptr == NULL)
        { dptr = AddDeffunction(theEnv,deffunctionName,NULL,min,max,0,true); }
      else
        {
         overwrite = true;
         owMin = dptr->minNumberOfParameters;
         owMax = dptr->maxNumberOfParameters;
         dptr->minNumberOfParameters = min;
         dptr->maxNumberOfParameters = max;
        }
     }
   else
     { dptr = AddDeffunction(theEnv,deffunctionName,NULL,min,max,0,true); }

   if (dptr == NULL)
     {
      CL_ReturnExpression(theEnv,parameterList);
      return true;
     }

   /*==================================================*/
   /* Parse the actions contained within the function. */
   /*==================================================*/

   CL_PPCRAndIndent(theEnv);

   ExpressionData(theEnv)->ReturnContext = true;
   actions = CL_ParseProcActions(theEnv,"deffunction",readSource,
                              &inputToken,parameterList,wildcard,
                              NULL,NULL,&lvars,NULL);

   /*=============================================================*/
   /* Check for the closing right parenthesis of the deffunction. */
   /*=============================================================*/

   if ((inputToken.tknType != RIGHT_PARENTHESIS_TOKEN) && /* DR0872 */
       (actions != NULL))
     {
      CL_SyntaxErrorMessage(theEnv,"deffunction");

      CL_ReturnExpression(theEnv,parameterList);
      CL_ReturnPackedExpression(theEnv,actions);

      if (overwrite)
        {
         dptr->minNumberOfParameters = owMin;
         dptr->maxNumberOfParameters = owMax;
        }

      if ((dptr->busy == 0) && (! overwrite))
        {
         CL_RemoveConstructFromModule(theEnv,&dptr->header);
         CL_RemoveDeffunction(theEnv,dptr);
        }

      return true;
     }

   if (actions == NULL)
     {
      CL_ReturnExpression(theEnv,parameterList);
      if (overwrite)
        {
         dptr->minNumberOfParameters = owMin;
         dptr->maxNumberOfParameters = owMax;
        }

      if ((dptr->busy == 0) && (! overwrite))
        {
         CL_RemoveConstructFromModule(theEnv,&dptr->header);
         CL_RemoveDeffunction(theEnv,dptr);
        }
      return true;
     }

   /*==============================================*/
   /* If we're only checking syntax, don't add the */
   /* successfully parsed deffunction to the KB.   */
   /*==============================================*/

   if (ConstructData(theEnv)->CL_CheckSyntaxMode)
     {
      CL_ReturnExpression(theEnv,parameterList);
      CL_ReturnPackedExpression(theEnv,actions);
      if (overwrite)
        {
         dptr->minNumberOfParameters = owMin;
         dptr->maxNumberOfParameters = owMax;
        }
      else
        {
         CL_RemoveConstructFromModule(theEnv,&dptr->header);
         CL_RemoveDeffunction(theEnv,dptr);
        }
      return false;
     }

   /*=============================*/
   /* RefoCL_rmat the closing token. */
   /*=============================*/

   CL_PPBackup(theEnv);
   CL_PPBackup(theEnv);
   CL_SavePPBuffer(theEnv,inputToken.printFoCL_rm);
   CL_SavePPBuffer(theEnv,"\n");

   /*======================*/
   /* Add the deffunction. */
   /*======================*/

   AddDeffunction(theEnv,deffunctionName,actions,min,max,lvars,false);

   CL_ReturnExpression(theEnv,parameterList);

   return(deffunctionError);
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/************************************************************
  NAME         : ValidCL_DeffunctionName
  DESCRIPTION  : DeteCL_rmines if a new deffunction of the given
                 name can be defined in the current module
  INPUTS       : The new deffunction name
  RETURNS      : True if OK, false otherwise
  SIDE EFFECTS : Error message printed if not OK
  NOTES        : CL_GetConstructNameAndComment() (called before
                 this function) ensures that the deffunction
                 name does not conflict with one from
                 another module
 ************************************************************/
static bool ValidCL_DeffunctionName(
  Environment *theEnv,
  const char *theCL_DeffunctionName)
  {
   Deffunction *theDeffunction;
#if DEFGENERIC_CONSTRUCT
   Defmodule *theModule;
   Defgeneric *theDefgeneric;
#endif

   /*==============================================*/
   /* A deffunction cannot be named the same as a  */
   /* construct type, e.g, defclass, defrule, etc. */
   /*==============================================*/

   if (CL_FindConstruct(theEnv,theCL_DeffunctionName) != NULL)
     {
      CL_PrintErrorID(theEnv,"DFFNXPSR",1,false);
      CL_WriteString(theEnv,STDERR,"Deffunctions are not allowed to replace constructs.\n");
      return false;
     }

   /*========================================*/
   /* A deffunction cannot be named the same */
   /* as a pre-defined system function, e.g, */
   /* watch, list-defrules, etc.             */
   /*========================================*/

   if (CL_FindFunction(theEnv,theCL_DeffunctionName) != NULL)
     {
      CL_PrintErrorID(theEnv,"DFFNXPSR",2,false);
      CL_WriteString(theEnv,STDERR,"Deffunctions are not allowed to replace external functions.\n");
      return false;
     }

#if DEFGENERIC_CONSTRUCT

   /*===========================================*/
   /* A deffunction cannot be named the same as */
   /* a generic function (either in this module */
   /* or imported from another).                */
   /*===========================================*/

   theDefgeneric = CL_LookupDefgenericInScope(theEnv,theCL_DeffunctionName);

   if (theDefgeneric != NULL)
     {
      theModule = CL_GetConstructModuleItem(&theDefgeneric->header)->theModule;
      if (theModule != CL_GetCurrentModule(theEnv))
        {
         CL_PrintErrorID(theEnv,"DFFNXPSR",5,false);
         CL_WriteString(theEnv,STDERR,"Defgeneric ");
         CL_WriteString(theEnv,STDERR,CL_DefgenericName(theDefgeneric));
         CL_WriteString(theEnv,STDERR," imported from module ");
         CL_WriteString(theEnv,STDERR,CL_DefmoduleName(theModule));
         CL_WriteString(theEnv,STDERR," conflicts with this deffunction.\n");
         return false;
        }
      else
        {
         CL_PrintErrorID(theEnv,"DFFNXPSR",3,false);
         CL_WriteString(theEnv,STDERR,"Deffunctions are not allowed to replace generic functions.\n");
        }
      return false;
     }
#endif

   theDeffunction = CL_FindDeffunctionInModule(theEnv,theCL_DeffunctionName);
   if (theDeffunction != NULL)
     {
      /*=============================================*/
      /* And a deffunction in the current module can */
      /* only be redefined if it is not executing.   */
      /*=============================================*/

      if (theDeffunction->executing)
        {
         CL_PrintErrorID(theEnv,"DFFNXPSR",4,false);
         CL_WriteString(theEnv,STDERR,"Deffunction '");
         CL_WriteString(theEnv,STDERR,CL_DeffunctionName(theDeffunction));
         CL_WriteString(theEnv,STDERR,"' may not be redefined while it is executing.\n");
         return false;
        }
     }
   return true;
  }

/****************************************************
  NAME         : AddDeffunction
  DESCRIPTION  : Adds a deffunction to the list of
                 deffunctions
  INPUTS       : 1) The symbolic name
                 2) The action expressions
                 3) The minimum number of arguments
                 4) The maximum number of arguments
                    (can be -1)
                 5) The number of local variables
                 6) A flag indicating if this is
                    a header call so that the
                    deffunction can be recursively
                    called
  RETURNS      : The new deffunction (NULL on errors)
  SIDE EFFECTS : Deffunction structures allocated
  NOTES        : Assumes deffunction is not executing
 ****************************************************/
static Deffunction *AddDeffunction(
  Environment *theEnv,
  CLIPSLexeme *name,
  Expression *actions,
  unsigned short min,
  unsigned short max,
  unsigned short lvars,
  bool headerp)
  {
   Deffunction *dfuncPtr;
   unsigned oldbusy;
#if DEBUGGING_FUNCTIONS
   bool DFHadCL_Watch = false;
#else
#if MAC_XCD
#pragma unused(headerp)
#endif
#endif

   /*===============================================================*/
   /* If the deffunction doesn't exist, create a new structure to   */
   /* contain it and add it to the List of deffunctions. Otherwise, */
   /* use the existing structure and remove the pretty print foCL_rm   */
   /* and interpretive code.                                        */
   /*===============================================================*/

   dfuncPtr = CL_FindDeffunctionInModule(theEnv,name->contents);
   if (dfuncPtr == NULL)
     {
      dfuncPtr = get_struct(theEnv,deffunction);
      CL_InitializeConstructHeader(theEnv,"deffunction",DEFFUNCTION,&dfuncPtr->header,name);
      IncrementLexemeCount(name);
      dfuncPtr->code = NULL;
      dfuncPtr->minNumberOfParameters = min;
      dfuncPtr->maxNumberOfParameters = max;
      dfuncPtr->numberOfLocalVars = lvars;
      dfuncPtr->busy = 0;
      dfuncPtr->executing = 0;
     }
   else
     {
#if DEBUGGING_FUNCTIONS
      DFHadCL_Watch = CL_DeffunctionGetCL_Watch(dfuncPtr);
#endif
      dfuncPtr->minNumberOfParameters = min;
      dfuncPtr->maxNumberOfParameters = max;
      dfuncPtr->numberOfLocalVars = lvars;
      oldbusy = dfuncPtr->busy;
      CL_ExpressionDeinstall(theEnv,dfuncPtr->code);
      dfuncPtr->busy = oldbusy;
      CL_ReturnPackedExpression(theEnv,dfuncPtr->code);
      dfuncPtr->code = NULL;
      SetCL_DeffunctionPPFoCL_rm(theEnv,dfuncPtr,NULL);

      /*======================================*/
      /* Remove the deffunction from the list */
      /* so that it can be added at the end.  */
      /*======================================*/

      CL_RemoveConstructFromModule(theEnv,&dfuncPtr->header);
     }

   CL_AddConstructToModule(&dfuncPtr->header);

   /*====================================*/
   /* Install the new interpretive code. */
   /*====================================*/

   if (actions != NULL)
     {
      /*=================================================*/
      /* If a deffunction is recursive, do not increment */
      /* its busy count based on self-references.        */
      /*=================================================*/

      oldbusy = dfuncPtr->busy;
      CL_ExpressionInstall(theEnv,actions);
      dfuncPtr->busy = oldbusy;
      dfuncPtr->code = actions;
     }

   /*==================================*/
   /* Install the pretty print foCL_rm if */
   /* memory is not being conserved.   */
   /*==================================*/

#if DEBUGGING_FUNCTIONS
   CL_DeffunctionSetCL_Watch(dfuncPtr,DFHadCL_Watch ? true : DeffunctionData(theEnv)->CL_WatchDeffunctions);

   if ((CL_GetConserveMemory(theEnv) == false) && (headerp == false))
     { SetCL_DeffunctionPPFoCL_rm(theEnv,dfuncPtr,CL_CopyPPBuffer(theEnv)); }
#endif

   return dfuncPtr;
  }

#endif /* DEFFUNCTION_CONSTRUCT && (! BLOAD_ONLY) && (! RUN_TIME) */
