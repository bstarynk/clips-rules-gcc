   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/17/17            */
   /*                                                     */
   /*            EXPRESSION PARSER HEADER FILE            */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines for parsing expressions.       */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Module specifier can be used within an         */
/*            expression to refer to a deffunction or        */
/*            defgeneric exported by the specified module,   */
/*            but not necessarily imported by the current    */
/*            module.                                        */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
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
/*************************************************************/

#ifndef _H_exprnpsr

#pragma once

#define _H_exprnpsr

typedef enum
  {
   FAE_NO_ERROR = 0,
   FAE_COUNT_ERROR,
   FAE_TYPE_ERROR
  } FunctionArgumentsError;

#include "extnfunc.h"
#include "scanner.h"

   struct expr                   *CL_Function0Parse(Environment *,const char *);
   struct expr                   *CL_Function1Parse(Environment *,const char *);
   struct expr                   *CL_Function2Parse(Environment *,const char *,const char *);
   void                           CL_PushRtnBrkContexts(Environment *);
   void                           CL_PopRtnBrkContexts(Environment *);
   bool                           CL_ReplaceSequenceExpansionOps(Environment *,struct expr *,struct expr *,
                                                                     void *,void *);
   struct expr                   *CL_CollectArguments(Environment *,struct expr *,const char *);
   struct expr                   *CL_ArgumentParse(Environment *,const char *,bool *);
   struct expr                   *CL_ParseAtomOrExpression(Environment *,const char *,struct token *);
   Expression                    *CL_ParseConstantArguments(Environment *,const char *,bool *);
   struct expr                   *CL_GroupActions(Environment *,const char *,struct token *,
                                                      bool,const char *,bool);
   struct expr                   *CL_RemoveUnneededProgn(Environment *,struct expr *);
   void                           CL_PopulateRestriction(Environment *,unsigned *,unsigned,const char *,unsigned int);


   FunctionArgumentsError         CL_CheckExpressionAgainstRestrictions(Environment *,struct expr *,
                                                                     struct functionDefinition *,const char *);

#if (! RUN_TIME)
   bool                           CL_RestrictionExists(const char *,int);

#endif

#endif /* _H_exprnpsr */




