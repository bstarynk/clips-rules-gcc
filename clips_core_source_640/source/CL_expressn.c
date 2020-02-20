   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/01/16             */
   /*                                                     */
   /*                  EXPRESSION MODULE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains routines for creating, deleting,        */
/*   compacting, installing, and hashing expressions.        */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Corrected link errors with non-default         */
/*            setup.h configuration settings.                */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Changed expression hashing value.              */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

#include "setup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bload.h"
#include "envrnmnt.h"
#include "evaluatn.h"
#include "exprnops.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"

#include "expressn.h"

#define PRIME_ONE   257
#define PRIME_TWO   263
#define PRIME_THREE 269

/****************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS  */
/****************************************/

#if (! RUN_TIME)
   static unsigned long           ListToPacked(struct expr *,struct expr *,unsigned long);
#endif
   static EXPRESSION_HN          *FindHashedExpression(Environment *,Expression *,unsigned *,EXPRESSION_HN **);
   static unsigned                HashExpression(Expression *);
   static void                    DeallocateExpressionData(Environment *);

/**************************************************/
/* CL_InitExpressionData: Initializes the function   */
/*   pointers used in generating some expressions */
/*   and the expression hash table.               */
/**************************************************/
void CL_InitExpressionData(
  Environment *theEnv)
  {
   unsigned i;

   CL_AllocateEnvironmentData(theEnv,EXPRESSION_DATA,sizeof(struct expressionData),DeallocateExpressionData);

#if ! RUN_TIME
   CL_InitExpressionPointers(theEnv);
#endif

   ExpressionData(theEnv)->ExpressionHashTable = (EXPRESSION_HN **)
     CL_gm2(theEnv,sizeof(EXPRESSION_HN *) * EXPRESSION_HASH_SIZE);
   for (i = 0 ; i < EXPRESSION_HASH_SIZE ; i++)
     ExpressionData(theEnv)->ExpressionHashTable[i] = NULL;
  }

/*****************************************/
/* DeallocateExpressionData: Deallocates */
/*    environment data for expressions.  */
/*****************************************/
static void DeallocateExpressionData(
  Environment *theEnv)
  {
   int i;
   EXPRESSION_HN *tmpPtr, *nextPtr;

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)
   if (! CL_Bloaded(theEnv))
#endif
     {
      for (i = 0; i < EXPRESSION_HASH_SIZE; i++)
        {
         tmpPtr = ExpressionData(theEnv)->ExpressionHashTable[i];
         while (tmpPtr != NULL)
           {
            nextPtr = tmpPtr->next;
            CL_ReturnPackedExpression(theEnv,tmpPtr->exp);
            rtn_struct(theEnv,exprHashNode,tmpPtr);
            tmpPtr = nextPtr;
           }
        }
     }

   CL_rm(theEnv,ExpressionData(theEnv)->ExpressionHashTable,
      sizeof(EXPRESSION_HN *) * EXPRESSION_HASH_SIZE);

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)
   if ((ExpressionData(theEnv)->NumberOfExpressions != 0) && CL_Bloaded(theEnv))
     {
      CL_genfree(theEnv,ExpressionData(theEnv)->ExpressionArray,
              ExpressionData(theEnv)->NumberOfExpressions * sizeof(struct expr));
     }
#endif
  }

/****************************************************/
/* CL_InitExpressionPointers: Initializes the function */
/*   pointers used in generating some expressions.  */
/****************************************************/
void CL_InitExpressionPointers(
  Environment *theEnv)
  {
   ExpressionData(theEnv)->PTR_AND = CL_FindFunction(theEnv,"and");
   ExpressionData(theEnv)->PTR_OR = CL_FindFunction(theEnv,"or");
   ExpressionData(theEnv)->PTR_EQ = CL_FindFunction(theEnv,"eq");
   ExpressionData(theEnv)->PTR_NEQ = CL_FindFunction(theEnv,"neq");
   ExpressionData(theEnv)->PTR_NOT = CL_FindFunction(theEnv,"not");

   if ((ExpressionData(theEnv)->PTR_AND == NULL) || (ExpressionData(theEnv)->PTR_OR == NULL) ||
       (ExpressionData(theEnv)->PTR_EQ == NULL) || (ExpressionData(theEnv)->PTR_NEQ == NULL) || (ExpressionData(theEnv)->PTR_NOT == NULL))
     {
      CL_SystemError(theEnv,"EXPRESSN",1);
      CL_ExitRouter(theEnv,EXIT_FAILURE);
     }
  }

/***************************************************/
/* CL_ExpressionInstall: Increments the busy count of */
/*   atomic data values found in an expression.    */
/***************************************************/
void CL_ExpressionInstall(
  Environment *theEnv,
  struct expr *expression)
  {
   if (expression == NULL) return;

   while (expression != NULL)
     {
      CL_AtomInstall(theEnv,expression->type,expression->value);
      CL_ExpressionInstall(theEnv,expression->argList);
      expression = expression->nextArg;
     }
  }

/*****************************************************/
/* CL_ExpressionDeinstall: Decrements the busy count of */
/*   atomic data values found in an expression.      */
/*****************************************************/
void CL_ExpressionDeinstall(
  Environment *theEnv,
  struct expr *expression)
  {
   if (expression == NULL) return;

   while (expression != NULL)
     {
      CL_AtomDeinstall(theEnv,expression->type,expression->value);
      CL_ExpressionDeinstall(theEnv,expression->argList);
      expression = expression->nextArg;
     }
  }

#if (! RUN_TIME)

/***********************************************************************/
/* CL_PackExpression: Copies an expression (created using multiple memory */
/*   requests) into an array (created using a single memory request)   */
/*   while CL_maintaining all appropriate links in the expression. A      */
/*   packed expression requires less total memory because it reduces   */
/*   the overhead required for multiple memory allocations.            */
/***********************************************************************/
struct expr *CL_PackExpression(
  Environment *theEnv,
  struct expr *original)
  {
   struct expr *packPtr;

   if (original == NULL) return NULL;
   
   packPtr = (struct expr *)
             CL_gm2(theEnv,sizeof (struct expr) * CL_ExpressionSize(original));
   ListToPacked(original,packPtr,0);
   
   return packPtr;
  }

/***********************************************************/
/* ListToPacked: Copies a list of expressions to an array. */
/***********************************************************/
static unsigned long ListToPacked(
  struct expr *original,
  struct expr *destination,
  unsigned long count)
  {
   unsigned long i;

   if (original == NULL) { return count; }

   while (original != NULL)
     {
      i = count;
      count++;

      destination[i].type = original->type;
      destination[i].value = original->value;

      if (original->argList == NULL)
        { destination[i].argList = NULL; }
      else
        {
         destination[i].argList =
           (struct expr *) &destination[count];
         count = ListToPacked(original->argList,destination,count);
        }

      if (original->nextArg == NULL)
        { destination[i].nextArg = NULL; }
      else
        {
         destination[i].nextArg = &destination[count];
        }

      original = original->nextArg;
     }

   return count;
  }

#endif /* (! RUN_TIME) */

/***************************************************************/
/* CL_ReturnPackedExpression: Returns a packed expression created */
/*   using CL_PackExpression to the memory manager.               */
/***************************************************************/
void CL_ReturnPackedExpression(
  Environment *theEnv,
  struct expr *packPtr)
  {
   if (packPtr != NULL)
     {
      CL_rm(theEnv,packPtr,sizeof (struct expr) * CL_ExpressionSize(packPtr));
     }
  }

/***********************************************/
/* CL_ReturnExpression: Returns a multiply linked */
/*   list of expr data structures.             */
/***********************************************/
void CL_ReturnExpression(
  Environment *theEnv,
  struct expr *waste)
  {
   struct expr *tmp;

   while (waste != NULL)
     {
      if (waste->argList != NULL) CL_ReturnExpression(theEnv,waste->argList);
      tmp = waste;
      waste = waste->nextArg;
      rtn_struct(theEnv,expr,tmp);
     }
  }

/***************************************************
  NAME         : FindHashedExpression
  DESCRIPTION  : DeteCL_rmines if a given expression
                 is in the expression hash table
  INPUTS       : 1) The expression
                 2) A buffer to hold the hash
                    value
                 3) A buffer to hold the previous
                    node in the hash chain
  RETURNS      : The expression hash table entry
                 (NULL if not found)
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static EXPRESSION_HN *FindHashedExpression(
  Environment *theEnv,
  Expression *theExp,
  unsigned *hashval,
  EXPRESSION_HN **prv)
  {
   EXPRESSION_HN *exphash;

   if (theExp == NULL)
     return NULL;
   *hashval = HashExpression(theExp);
   *prv = NULL;
   exphash = ExpressionData(theEnv)->ExpressionHashTable[*hashval];
   while (exphash != NULL)
     {
      if (CL_IdenticalExpression(exphash->exp,theExp))
        return(exphash);
      *prv = exphash;
      exphash = exphash->next;
     }
   return NULL;
  }

/***************************************************
  NAME         : HashExpression
  DESCRIPTION  : Assigns a deteCL_rministic number to
                 an expression
  INPUTS       : The expression
  RETURNS      : The "value" of the expression
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static unsigned HashExpression(
  Expression *theExp)
  {
   unsigned long tally = PRIME_THREE;
   union
     {
      void *vv;
      unsigned long uv;
     } fis;

   if (theExp->argList != NULL)
     tally += HashExpression(theExp->argList) * PRIME_ONE;
   while (theExp != NULL)
     {
      tally += theExp->type * PRIME_TWO;
      fis.uv = 0;
      fis.vv = theExp->value;
      tally += fis.uv;
      theExp = theExp->nextArg;
     }
   return (unsigned) (tally % EXPRESSION_HASH_SIZE);
  }

/***************************************************
  NAME         : CL_RemoveHashedExpression
  DESCRIPTION  : Removes a hashed expression from
                 the hash table
  INPUTS       : The expression
  RETURNS      : Nothing useful
  SIDE EFFECTS : Hash node removed (or use count
                 decremented).  If the hash node
                 is removed, the expression is
                 deinstalled and deleted
  NOTES        : If the expression is in use by
                 others, then the use count is
                 merely decremented
 ***************************************************/
void CL_RemoveHashedExpression(
  Environment *theEnv,
  Expression *theExp)
  {
   EXPRESSION_HN *exphash,*prv;
   unsigned hashval;

   exphash = FindHashedExpression(theEnv,theExp,&hashval,&prv);
   if (exphash == NULL)
     return;
   if (--exphash->count != 0)
     return;
   if (prv == NULL)
     ExpressionData(theEnv)->ExpressionHashTable[hashval] = exphash->next;
   else
     prv->next = exphash->next;
   CL_ExpressionDeinstall(theEnv,exphash->exp);
   CL_ReturnPackedExpression(theEnv,exphash->exp);
   rtn_struct(theEnv,exprHashNode,exphash);
  }

#if (! BLOAD_ONLY) && (! RUN_TIME)

/*****************************************************
  NAME         : CL_AddHashedExpression
  DESCRIPTION  : Adds a new expression to the
                 expression hash table (or increments
                 the use count if it is already there)
  INPUTS       : The (new) expression
  RETURNS      : A pointer to the (new) hash node
  SIDE EFFECTS : Adds the new hash node or increments
                 the count of an existing one
  NOTES        : It is the caller's responsibility to
                 delete the passed expression.  This
                 routine copies, packs and installs
                 the given expression
 *****************************************************/
Expression *CL_AddHashedExpression(
  Environment *theEnv,
  Expression *theExp)
  {
   EXPRESSION_HN *prv,*exphash;
   unsigned hashval;

   if (theExp == NULL) return NULL;
   exphash = FindHashedExpression(theEnv,theExp,&hashval,&prv);
   if (exphash != NULL)
     {
      exphash->count++;
      return(exphash->exp);
     }
   exphash = get_struct(theEnv,exprHashNode);
   exphash->hashval = hashval;
   exphash->count = 1;
   exphash->exp = CL_PackExpression(theEnv,theExp);
   CL_ExpressionInstall(theEnv,exphash->exp);
   exphash->next = ExpressionData(theEnv)->ExpressionHashTable[exphash->hashval];
   ExpressionData(theEnv)->ExpressionHashTable[exphash->hashval] = exphash;
   exphash->bsaveID = 0L;
   return(exphash->exp);
  }

#endif /* (! BLOAD_ONLY) && (! RUN_TIME) */

#if (BLOAD_AND_BSAVE || BLOAD_ONLY || BLOAD || CONSTRUCT_COMPILER) && (! RUN_TIME)

/***************************************************
  NAME         : CL_HashedExpressionIndex
  DESCRIPTION  : Finds the expression bload array
                 index for a hashed expression
  INPUTS       : The expression
  RETURNS      : The bload index
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
unsigned long CL_HashedExpressionIndex(
  Environment *theEnv,
  Expression *theExp)
  {
   EXPRESSION_HN *exphash,*prv;
   unsigned hashval;

   if (theExp == NULL)
     return ULONG_MAX;
   exphash = FindHashedExpression(theEnv,theExp,&hashval,&prv);
   return((exphash != NULL) ? exphash->bsaveID : ULONG_MAX);
  }

#endif /* (BLOAD_AND_BSAVE || BLOAD_ONLY || BLOAD || CONSTRUCT_COMPILER) && (! RUN_TIME) */

/********************************************************/
/* CL_SetSequenceOperatorRecognition: C access routine     */
/*   for the set-sequence-operator-recognition function */
/********************************************************/
bool CL_SetSequenceOperatorRecognition(
  Environment *theEnv,
  bool value)
  {
   bool ov;

   ov = ExpressionData(theEnv)->SequenceOpMode;
   ExpressionData(theEnv)->SequenceOpMode = value;
   return ov;
  }

/********************************************************/
/* CL_GetSequenceOperatorRecognition: C access routine     */
/*   for the Get-sequence-operator-recognition function */
/********************************************************/
bool CL_GetSequenceOperatorRecognition(
  Environment *theEnv)
  {
   return ExpressionData(theEnv)->SequenceOpMode;
  }

