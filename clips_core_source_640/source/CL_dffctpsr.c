   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*                DEFFACTS PARSER MODULE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: Parses a deffacts construct.                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            CL_GetConstructNameAndComment API change.         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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

#include "setup.h"

#if DEFFACTS_CONSTRUCT

#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif

#include "cstrcpsr.h"
#include "dffctbsc.h"
#include "dffctdef.h"
#include "envrnmnt.h"
#include "factrhs.h"
#include "memalloc.h"
#include "modulutl.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"

#include "dffctpsr.h"

/************************************************************/
/* CL_ParseDeffacts: Coordinates all actions necessary for the */
/*   addition of a deffacts construct into the current      */
/*   environment. Called when parsing a construct after the */
/*   deffacts keyword has been found.                       */
/************************************************************/
bool CL_ParseDeffacts(
  Environment *theEnv,
  const char *readSource)
  {
#if (! RUN_TIME) && (! BLOAD_ONLY)
   CLIPSLexeme *deffactsName;
   struct expr *temp;
   Deffacts *newDeffacts;
   bool deffactsError;
   struct token inputToken;

   /*=========================*/
   /* Parsing initialization. */
   /*=========================*/

   deffactsError = false;
   CL_SetPPBufferStatus(theEnv,true);

   CL_FlushPPBuffer(theEnv);
   CL_SetIndentDepth(theEnv,3);
   CL_SavePPBuffer(theEnv,"(deffacts ");

   /*==========================================================*/
   /* Deffacts can not be added when a binary image is loaded. */
   /*==========================================================*/

#if BLOAD || BLOAD_AND_BSAVE
   if ((CL_Bloaded(theEnv) == true) && (! ConstructData(theEnv)->CL_CheckSyntaxMode))
     {
      Cannot_LoadWith_BloadMessage(theEnv,"deffacts");
      return true;
     }
#endif

   /*============================*/
   /* Parse the deffacts header. */
   /*============================*/

   deffactsName = CL_GetConstructNameAndComment(theEnv,readSource,&inputToken,"deffacts",
                                             (CL_FindConstructFunction *) CL_FindDeffactsInModule,
                                             (DeleteConstructFunction *) CL_Undeffacts,"$",true,
                                             true,true,false);
   if (deffactsName == NULL) { return true; }

   /*===============================================*/
   /* Parse the list of facts in the deffacts body. */
   /*===============================================*/

   temp = CL_BuildRHS_Assert(theEnv,readSource,&inputToken,&deffactsError,false,false,"deffacts");

   if (deffactsError == true) { return true; }

   if (CL_ExpressionContainsVariables(temp,false))
     {
      CL_LocalVariableErrorMessage(theEnv,"a deffacts construct");
      CL_ReturnExpression(theEnv,temp);
      return true;
     }

   CL_SavePPBuffer(theEnv,"\n");

   /*==============================================*/
   /* If we're only checking syntax, don't add the */
   /* successfully parsed deffacts to the KB.      */
   /*==============================================*/

   if (ConstructData(theEnv)->CL_CheckSyntaxMode)
     {
      CL_ReturnExpression(theEnv,temp);
      return false;
     }

   /*==========================*/
   /* Create the new deffacts. */
   /*==========================*/

   CL_ExpressionInstall(theEnv,temp);
   newDeffacts = get_struct(theEnv,deffacts);
   IncrementLexemeCount(deffactsName);
   CL_InitializeConstructHeader(theEnv,"deffacts",DEFFACTS,&newDeffacts->header,deffactsName);

   newDeffacts->assertList = CL_PackExpression(theEnv,temp);
   CL_ReturnExpression(theEnv,temp);

   /*=======================================================*/
   /* CL_Save the pretty print representation of the deffacts. */
   /*=======================================================*/

   if (CL_GetConserveMemory(theEnv) == true)
     { newDeffacts->header.ppFo_rm = NULL; }
   else
     { newDeffacts->header.ppFo_rm = CL_CopyPPBuffer(theEnv); }

   /*=============================================*/
   /* Add the deffacts to the appropriate module. */
   /*=============================================*/

   CL_AddConstructToModule(&newDeffacts->header);

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

   /*================================================================*/
   /* Return false to indicate the deffacts was successfully parsed. */
   /*================================================================*/

   return false;
  }

#endif /* DEFFACTS_CONSTRUCT */


