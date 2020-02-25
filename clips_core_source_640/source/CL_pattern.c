   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*                 RULE PATTERN MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides the mechanism for recognizing and       */
/*   parsing the various types of patterns that can be used  */
/*   in the LHS of a rule. In version 6.0, the only pattern  */
/*   types provided are for deftemplate and instance         */
/*   patterns.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Added support for hashed alpha memories.       */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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

#include <stdio.h>
#include <stdlib.h>

#if DEFRULE_CONSTRUCT

#include "constant.h"
#include "constrnt.h"
#include "cstrnchk.h"
#include "cstrnutl.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "match.h"
#include "memalloc.h"
#include "pprint.h"
#include "prntutil.h"
#include "reteutil.h"
#include "router.h"
#include "rulecmp.h"

#include "pattern.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
static struct lhsParseNode *Conjuctive_RestrictionParse (Environment *,
							 const char *,
							 struct token *,
							 bool *);
static struct lhsParseNode *Literal_RestrictionParse (Environment *,
						      const char *,
						      struct token *, bool *);
static bool CheckForVariableMixing (Environment *, struct lhsParseNode *);
static void TallyFieldTypes (struct lhsParseNode *);
#endif
static void DeallocatePatternData (Environment *);
static struct patternNodeHashEntry **CreatePatternHashTable (Environment *,
							     unsigned long);

/*****************************************************************************/
/* CL_InitializePatterns: Initializes the global data associated with patterns. */
/*****************************************************************************/
void
CL_InitializePatterns (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, PATTERN_DATA,
			      sizeof (struct patternData),
			      DeallocatePatternData);
  PatternData (theEnv)->NextPosition = 1;
  PatternData (theEnv)->PatternHashTable =
    CreatePatternHashTable (theEnv, SIZE_PATTERN_HASH);
  PatternData (theEnv)->PatternHashTableSize = SIZE_PATTERN_HASH;
}

/*******************************************************************/
/* CreatePatternHashTable: Creates and initializes a fact hash table. */
/*******************************************************************/
static struct patternNodeHashEntry **
CreatePatternHashTable (Environment * theEnv, unsigned long tableSize)
{
  unsigned long i;
  struct patternNodeHashEntry **theTable;

  theTable = (struct patternNodeHashEntry **)
    CL_gm2 (theEnv, sizeof (struct patternNodeHashEntry *) * tableSize);

  if (theTable == NULL)
    CL_ExitRouter (theEnv, EXIT_FAILURE);

  for (i = 0; i < tableSize; i++)
    theTable[i] = NULL;

  return (theTable);
}

/**************************************************/
/* DeallocatePatternData: Deallocates environment */
/*    data for rule pattern registration.         */
/**************************************************/
static void
DeallocatePatternData (Environment * theEnv)
{
  struct reservedSymbol *tmpRSPtr, *nextRSPtr;
  struct patternParser *tmpPPPtr, *nextPPPtr;
  struct patternNodeHashEntry *tmpPNEPtr, *nextPNEPtr;
  unsigned long i;

  tmpRSPtr = PatternData (theEnv)->ListOf_ReservedPatternSymbols;
  while (tmpRSPtr != NULL)
    {
      nextRSPtr = tmpRSPtr->next;
      rtn_struct (theEnv, reservedSymbol, tmpRSPtr);
      tmpRSPtr = nextRSPtr;
    }

  tmpPPPtr = PatternData (theEnv)->ListOfPatternParsers;
  while (tmpPPPtr != NULL)
    {
      nextPPPtr = tmpPPPtr->next;
      rtn_struct (theEnv, patternParser, tmpPPPtr);
      tmpPPPtr = nextPPPtr;
    }

  for (i = 0; i < PatternData (theEnv)->PatternHashTableSize; i++)
    {
      tmpPNEPtr = PatternData (theEnv)->PatternHashTable[i];

      while (tmpPNEPtr != NULL)
	{
	  nextPNEPtr = tmpPNEPtr->next;
	  rtn_struct (theEnv, patternNodeHashEntry, tmpPNEPtr);
	  tmpPNEPtr = nextPNEPtr;
	}
    }

  CL_rm (theEnv, PatternData (theEnv)->PatternHashTable,
	 sizeof (struct patternNodeHashEntry *) *
	 PatternData (theEnv)->PatternHashTableSize);
}

/******************************************************************************/
/* CL_AddHashedPatternNode: Adds a pattern node entry to the pattern hash table. */
/******************************************************************************/
void
CL_AddHashedPatternNode (Environment * theEnv,
			 void *parent,
			 void *child, unsigned short keyType, void *keyValue)
{
  size_t hashValue;
  struct patternNodeHashEntry *newhash, *temp;

  hashValue = CL_GetAtomicHashValue (keyType, keyValue, 1) + CL_HashExternalAddress (parent, 0);	/* TBD mult * 30 */

  newhash = get_struct (theEnv, patternNodeHashEntry);
  newhash->parent = parent;
  newhash->child = child;
  newhash->type = keyType;
  newhash->value = keyValue;

  hashValue = (hashValue % PatternData (theEnv)->PatternHashTableSize);

  temp = PatternData (theEnv)->PatternHashTable[hashValue];
  PatternData (theEnv)->PatternHashTable[hashValue] = newhash;
  newhash->next = temp;
}

/***************************************************/
/* CL_RemoveHashedPatternNode: Removes a pattern node */
/*   entry from the pattern node hash table.       */
/***************************************************/
bool
CL_RemoveHashedPatternNode (Environment * theEnv,
			    void *parent,
			    void *child,
			    unsigned short keyType, void *keyValue)
{
  size_t hashValue;
  struct patternNodeHashEntry *hptr, *prev;

  hashValue = CL_GetAtomicHashValue (keyType, keyValue, 1) + CL_HashExternalAddress (parent, 0);	/* TBD mult * 30 */
  hashValue = (hashValue % PatternData (theEnv)->PatternHashTableSize);

  for (hptr = PatternData (theEnv)->PatternHashTable[hashValue], prev = NULL;
       hptr != NULL; hptr = hptr->next)
    {
      if (hptr->child == child)
	{
	  if (prev == NULL)
	    {
	      PatternData (theEnv)->PatternHashTable[hashValue] = hptr->next;
	      rtn_struct (theEnv, patternNodeHashEntry, hptr);
	      return true;
	    }
	  else
	    {
	      prev->next = hptr->next;
	      rtn_struct (theEnv, patternNodeHashEntry, hptr);
	      return true;
	    }
	}
      prev = hptr;
    }

  return false;
}

/***********************************************/
/* CL_FindHashedPatternNode: Finds a pattern node */
/*   entry in the pattern node hash table.     */
/***********************************************/
void *
CL_FindHashedPatternNode (Environment * theEnv,
			  void *parent,
			  unsigned short keyType, void *keyValue)
{
  size_t hashValue;
  struct patternNodeHashEntry *hptr;

  hashValue = CL_GetAtomicHashValue (keyType, keyValue, 1) + CL_HashExternalAddress (parent, 0);	/* TBD mult * 30 */
  hashValue = (hashValue % PatternData (theEnv)->PatternHashTableSize);

  for (hptr = PatternData (theEnv)->PatternHashTable[hashValue];
       hptr != NULL; hptr = hptr->next)
    {
      if ((hptr->parent == parent) &&
	  (keyType == hptr->type) && (keyValue == hptr->value))
	{
	  return (hptr->child);
	}
    }

  return NULL;
}

/******************************************************************/
/* CL_Add_ReservedPatternSymbol: Adds a symbol to the list of symbols */
/*  that are restricted for use in patterns. For example, the     */
/*  deftemplate construct cannot use the symbol "object", since   */
/*  this needs to be reserved for object patterns. Some symbols,  */
/*  such as "exists" are completely reserved and can not be used  */
/*  to start any type of pattern CE.                              */
/******************************************************************/
void
CL_Add_ReservedPatternSymbol (Environment * theEnv,
			      const char *theSymbol, const char *reservedBy)
{
  struct reservedSymbol *newSymbol;

  newSymbol = get_struct (theEnv, reservedSymbol);
  newSymbol->theSymbol = theSymbol;
  newSymbol->reservedBy = reservedBy;
  newSymbol->next = PatternData (theEnv)->ListOf_ReservedPatternSymbols;
  PatternData (theEnv)->ListOf_ReservedPatternSymbols = newSymbol;
}

/******************************************************************/
/* CL_ReservedPatternSymbol: Returns true if the specified symbol is */
/*   a reserved pattern symbol, otherwise false is returned. If   */
/*   the construct which is trying to use the symbol is the same  */
/*   construct that reserved the symbol, then false is returned.  */
/******************************************************************/
bool
CL_ReservedPatternSymbol (Environment * theEnv,
			  const char *theSymbol, const char *checkedBy)
{
  struct reservedSymbol *currentSymbol;

  for (currentSymbol = PatternData (theEnv)->ListOf_ReservedPatternSymbols;
       currentSymbol != NULL; currentSymbol = currentSymbol->next)
    {
      if (strcmp (theSymbol, currentSymbol->theSymbol) == 0)
	{
	  if ((currentSymbol->reservedBy == NULL) || (checkedBy == NULL))
	    {
	      return true;
	    }

	  if (strcmp (checkedBy, currentSymbol->reservedBy) == 0)
	    return false;

	  return true;
	}
    }

  return false;
}

/********************************************************/
/* CL_ReservedPatternSymbolErrorMsg: Generic error message */
/*   for attempting to use a reserved pattern symbol.   */
/********************************************************/
void
CL_ReservedPatternSymbolErrorMsg (Environment * theEnv,
				  const char *theSymbol, const char *usedFor)
{
  CL_PrintErrorID (theEnv, "PATTERN", 1, true);
  CL_WriteString (theEnv, STDERR, "The symbol '");
  CL_WriteString (theEnv, STDERR, theSymbol);
  CL_WriteString (theEnv, STDERR, "' has special meaning ");
  CL_WriteString (theEnv, STDERR, "and may not be used as ");
  CL_WriteString (theEnv, STDERR, usedFor);
  CL_WriteString (theEnv, STDERR, ".\n");
}

/************************************************************/
/* GetNextEntity: Utility routine for accessing all of the  */
/*   data entities that can match patterns. Currently facts */
/*   and instances are the only data entities available.    */
/************************************************************/
void
CL_GetNextPatternEntity (Environment * theEnv,
			 struct patternParser **theParser,
			 struct patternEntity **theEntity)
{

   /*=============================================================*/
  /* If the current parser is NULL, then we want to retrieve the */
  /* very first data entity. The traversal of entities is done   */
  /* by entity type (e.g. all facts are traversed followed by    */
  /* all instances). To get the first entity type to traverse,   */
  /* the current parser is set to the first parser on the list   */
  /* of pattern parsers.                                         */
   /*=============================================================*/

  if (*theParser == NULL)
    {
      *theParser = PatternData (theEnv)->ListOfPatternParsers;
      *theEntity = NULL;
    }

   /*================================================================*/
  /* Otherwise try to retrieve the next entity following the entity */
  /* returned by the last call to GetNextEntity. If that entity was */
  /* the last of its data type, then move on to the next pattern    */
  /* parser, otherwise return that entity as the next one.          */
   /*================================================================*/

  else if (theEntity != NULL)
    {
      *theEntity = (struct patternEntity *)
	(*(*theParser)->entityType->base.getNextFunction) (theEnv,
							   *theEntity);

      if ((*theEntity) != NULL)
	return;

      *theParser = (*theParser)->next;
    }

   /*===============================================================*/
  /* Otherwise, we encountered a situation which should not occur. */
  /* Once a NULL entity is returned from GetNextEntity, it should  */
  /* not be passed back to GetNextEntity.                          */
   /*===============================================================*/

  else
    {
      CL_SystemError (theEnv, "PATTERN", 1);
      CL_ExitRouter (theEnv, EXIT_FAILURE);
    }

   /*================================================*/
  /* Keep looping through the lists of entities and */
  /* pattern parsers until an entity is found.      */
   /*================================================*/

  while ((*theEntity == NULL) && (*theParser != NULL))
    {
      *theEntity = (struct patternEntity *)
	(*(*theParser)->entityType->base.getNextFunction) (theEnv,
							   *theEntity);

      if (*theEntity != NULL)
	return;

      *theParser = (*theParser)->next;
    }

  return;
}

/**************************************************************/
/* CL_DetachPattern: Detaches a pattern from the pattern network */
/*   by calling the appropriate function for the data type    */
/*   associated with the pattern.                             */
/**************************************************************/
void
CL_DetachPattern (Environment * theEnv,
		  unsigned short rhsType, struct patternNodeHeader *theHeader)
{
  if (rhsType == 0)
    return;

  if (PatternData (theEnv)->PatternParserArray[rhsType - 1] != NULL)
    {
      CL_FlushAlphaMemory (theEnv, theHeader);
      (*PatternData (theEnv)->PatternParserArray[rhsType - 1]->
       removePatternFunction) (theEnv, theHeader);
    }
}

/**************************************************/
/* CL_AddPatternParser: Adds a pattern type to the   */
/*   list of pattern parsers used to detect valid */
/*   patterns in the LHS of a rule.               */
/**************************************************/
bool
CL_AddPatternParser (Environment * theEnv, struct patternParser *newPtr)
{
  struct patternParser *currentPtr, *lastPtr = NULL;

   /*============================================*/
  /* Check to see that the limit for the number */
  /* of pattern parsers has not been exceeded.  */
   /*============================================*/

  if (PatternData (theEnv)->NextPosition >= MAX_POSITIONS)
    return false;

   /*================================*/
  /* Create the new pattern parser. */
   /*================================*/

  newPtr->positionInArray = PatternData (theEnv)->NextPosition;
  PatternData (theEnv)->PatternParserArray[PatternData (theEnv)->
					   NextPosition - 1] = newPtr;
  PatternData (theEnv)->NextPosition++;

   /*================================*/
  /* Add the parser to the list of  */
  /* parsers based on its priority. */
   /*================================*/

  if (PatternData (theEnv)->ListOfPatternParsers == NULL)
    {
      newPtr->next = NULL;
      PatternData (theEnv)->ListOfPatternParsers = newPtr;
      return true;
    }

  currentPtr = PatternData (theEnv)->ListOfPatternParsers;
  while ((currentPtr != NULL) ? (newPtr->priority <
				 currentPtr->priority) : false)
    {
      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
    }

  if (lastPtr == NULL)
    {
      newPtr->next = PatternData (theEnv)->ListOfPatternParsers;
      PatternData (theEnv)->ListOfPatternParsers = newPtr;
    }
  else
    {
      newPtr->next = currentPtr;
      lastPtr->next = newPtr;
    }

  return true;
}

/****************************************************/
/* CL_FindPatternParser: Searches for a pattern parser */
/*  that can parse a pattern beginning with the     */
/*  specified keyword (e.g. "object").              */
/****************************************************/
struct patternParser *
CL_FindPatternParser (Environment * theEnv, const char *name)
{
  struct patternParser *tempParser;

  for (tempParser = PatternData (theEnv)->ListOfPatternParsers;
       tempParser != NULL; tempParser = tempParser->next)
    {
      if (strcmp (tempParser->name, name) == 0)
	return (tempParser);
    }

  return NULL;
}

/******************************************************/
/* CL_GetPatternParser: Returns a pointer to the pattern */
/*    parser for the specified data entity.           */
/******************************************************/
struct patternParser *
CL_GetPatternParser (Environment * theEnv, unsigned short rhsType)
{
  if (rhsType == 0)
    return NULL;

  return (PatternData (theEnv)->PatternParserArray[rhsType - 1]);
}

#if CONSTRUCT_COMPILER && (! RUN_TIME)

/*************************************************************/
/* CL_PatternNodeHeaderToCode: CL_Writes the C code representation */
/*   of a patternNodeHeader data structure.                  */
/*************************************************************/
void
CL_PatternNodeHeaderToCode (Environment * theEnv,
			    FILE * fp,
			    struct patternNodeHeader *theHeader,
			    unsigned int imageID, unsigned int maxIndices)
{
  fprintf (fp, "{NULL,NULL,");

  if (theHeader->entryJoin == NULL)
    {
      fprintf (fp, "NULL,");
    }
  else
    {
      fprintf (fp, "&%s%u_%lu[%lu],",
	       JoinPrefix (), imageID,
	       (theHeader->entryJoin->bsaveID / maxIndices) + 1,
	       theHeader->entryJoin->bsaveID % maxIndices);
    }

  CL_PrintHashedExpressionReference (theEnv, fp, theHeader->rightHash,
				     imageID, maxIndices);

  fprintf (fp, ",%d,%d,%d,0,0,%d,%d,%d}", theHeader->singlefieldNode,
	   theHeader->multifieldNode,
	   theHeader->stopNode,
	   theHeader->beginSlot, theHeader->endSlot, theHeader->selector);
}

#endif /* CONSTRUCT_COMPILER && (! RUN_TIME) */

#if (! RUN_TIME) && (! BLOAD_ONLY)

/****************************************************************/
/* CL_PostPatternAnalysis: Calls the post analysis routines for    */
/*   each of the pattern parsers to allow additional processing */
/*   by the pattern parser after the variable analysis routines */
/*   have analyzed the LHS patterns.                            */
/****************************************************************/
bool
CL_PostPatternAnalysis (Environment * theEnv, struct lhsParseNode *theLHS)
{
  struct lhsParseNode *patternPtr;
  struct patternParser *tempParser;

  for (patternPtr = theLHS; patternPtr != NULL;
       patternPtr = patternPtr->bottom)
    {
      if ((patternPtr->pnType == PATTERN_CE_NODE)
	  && (patternPtr->patternType != NULL))
	{
	  tempParser = patternPtr->patternType;
	  if (tempParser->postAnalysisFunction != NULL)
	    {
	      if ((*tempParser->postAnalysisFunction) (theEnv, patternPtr))
		return true;
	    }
	}
    }

  return false;
}

/******************************************************************/
/* CL_RestrictionParse: Parses a single field within a pattern. This */
/*    field may either be a single field wildcard, a multifield   */
/*    wildcard, a single field variable, a multifield variable,   */
/*    or a series of connected constraints.                       */
/*                                                                */
/* <constraint> ::= ? |                                           */
/*                  $? |                                          */
/*                  <connected-constraint>                        */
/******************************************************************/
struct lhsParseNode *
CL_RestrictionParse (Environment * theEnv,
		     const char *readSource,
		     struct token *theToken,
		     bool multifieldSlot,
		     CLIPSLexeme * theSlot,
		     unsigned short slotNumber,
		     CONSTRAINT_RECORD * theConstraints,
		     unsigned short position)
{
  struct lhsParseNode *topNode = NULL, *lastNode = NULL, *nextNode;
  int numberOfSingleFields = 0;
  int numberOfMultifields = 0;
  unsigned short startPosition = position;
  bool error = false;
  CONSTRAINT_RECORD *tempConstraints;

   /*==================================================*/
  /* Keep parsing fields until a right parenthesis is */
  /* encountered. This will either indicate the end   */
  /* of an instance or deftemplate slot or the end of */
  /* an ordered fact.                                 */
   /*==================================================*/

  while (theToken->tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      /*========================================*/
      /* Look for either a single or multifield */
      /* wildcard or a conjuctive restriction.  */
      /*========================================*/

      if ((theToken->tknType == SF_WILDCARD_TOKEN) ||
	  (theToken->tknType == MF_WILDCARD_TOKEN))
	{
	  nextNode = CL_GetLHSParseNode (theEnv);
	  if (theToken->tknType == SF_WILDCARD_TOKEN)
	    {
	      nextNode->pnType = SF_WILDCARD_NODE;
	    }
	  else
	    {
	      nextNode->pnType = MF_WILDCARD_NODE;
	    }
	  nextNode->negated = false;
	  nextNode->exists = false;
	  CL_GetToken (theEnv, readSource, theToken);
	}
      else
	{
	  nextNode =
	    Conjuctive_RestrictionParse (theEnv, readSource, theToken,
					 &error);
	  if (nextNode == NULL)
	    {
	      CL_ReturnLHSParseNodes (theEnv, topNode);
	      return NULL;
	    }
	}

      /*========================================================*/
      /* Fix up the pretty print representation of a multifield */
      /* slot so that the fields don't run together.            */
      /*========================================================*/

      if ((theToken->tknType != RIGHT_PARENTHESIS_TOKEN)
	  && (multifieldSlot == true))
	{
	  CL_PPBackup (theEnv);
	  CL_SavePPBuffer (theEnv, " ");
	  CL_SavePPBuffer (theEnv, theToken->printFo_rm);
	}

      /*========================================*/
      /* Keep track of the number of single and */
      /* multifield restrictions encountered.   */
      /*========================================*/

      if ((nextNode->pnType == SF_WILDCARD_NODE) ||
	  (nextNode->pnType == SF_VARIABLE_NODE))
	{
	  numberOfSingleFields++;
	}
      else
	{
	  numberOfMultifields++;
	}

      /*===================================*/
      /* Assign the slot name and indices. */
      /*===================================*/

      nextNode->slot = theSlot;
      nextNode->slotNumber = slotNumber;
      nextNode->index = position++;

      /*==============================================*/
      /* If we're not dealing with a multifield slot, */
      /* attach the constraints directly to the node  */
      /* and return.                                  */
      /*==============================================*/

      if (!multifieldSlot)
	{
	  if (theConstraints == NULL)
	    {
	      if (nextNode->pnType == SF_VARIABLE_NODE)
		{
		  nextNode->constraints = CL_GetConstraintRecord (theEnv);
		}
	      else
		nextNode->constraints = NULL;
	    }
	  else
	    nextNode->constraints = theConstraints;
	  return (nextNode);
	}

      /*====================================================*/
      /* Attach the restriction to the list of restrictions */
      /* already parsed for this slot or ordered fact.      */
      /*====================================================*/

      if (lastNode == NULL)
	topNode = nextNode;
      else
	lastNode->right = nextNode;

      lastNode = nextNode;
    }

   /*=====================================================*/
  /* Once we're through parsing, check to make sure that */
  /* a single field slot was given a restriction. If the */
  /* following test fails, then we know we're dealing    */
  /* with a multifield slot.                             */
   /*=====================================================*/

  if ((topNode == NULL) && (!multifieldSlot))
    {
      CL_SyntaxErrorMessage (theEnv, "defrule");
      return NULL;
    }

   /*===============================================*/
  /* Loop through each of the restrictions in the  */
  /* list of restrictions for the multifield slot. */
   /*===============================================*/

  for (nextNode = topNode; nextNode != NULL; nextNode = nextNode->right)
    {
      /*===================================================*/
      /* Assign a constraint record to each constraint. If */
      /* the slot has an explicit constraint, then copy    */
      /* this and store it with the constraint. Otherwise, */
      /* create a constraint record for a single field     */
      /* constraint and skip the constraint modifications  */
      /* for a multifield constraint.                      */
      /*===================================================*/

      if (theConstraints == NULL)
	{
	  if (nextNode->pnType == SF_VARIABLE_NODE)
	    {
	      nextNode->constraints = CL_GetConstraintRecord (theEnv);
	    }
	  else
	    {
	      continue;
	    }
	}
      else
	{
	  nextNode->constraints =
	    CL_CopyConstraintRecord (theEnv, theConstraints);
	}

      /*==========================================*/
      /* Remove the min and max field constraints */
      /* for the entire slot from the constraint  */
      /* record for this single constraint.       */
      /*==========================================*/

      CL_ReturnExpression (theEnv, nextNode->constraints->minFields);
      CL_ReturnExpression (theEnv, nextNode->constraints->maxFields);
      nextNode->constraints->minFields =
	CL_GenConstant (theEnv, SYMBOL_TYPE,
			SymbolData (theEnv)->NegativeInfinity);
      nextNode->constraints->maxFields =
	CL_GenConstant (theEnv, SYMBOL_TYPE,
			SymbolData (theEnv)->PositiveInfinity);
      nextNode->derivedConstraints = true;

      /*====================================================*/
      /* If we're not dealing with a multifield constraint, */
      /* then no further modifications are needed to the    */
      /* min and max constraints for this constraint.       */
      /*====================================================*/

      if ((nextNode->pnType != MF_WILDCARD_NODE)
	  && (nextNode->pnType != MF_VARIABLE_NODE))
	{
	  continue;
	}

      /*==========================================================*/
      /* Create a separate constraint record to keep track of the */
      /* cardinality info_rmation for this multifield constraint.  */
      /*==========================================================*/

      tempConstraints = CL_GetConstraintRecord (theEnv);
      CL_SetConstraintType (MULTIFIELD_TYPE, tempConstraints);
      tempConstraints->singlefieldsAllowed = false;
      tempConstraints->multifield = nextNode->constraints;
      nextNode->constraints = tempConstraints;

      /*=====================================================*/
      /* Adjust the min and max field values for this single */
      /* multifield constraint based on the min and max      */
      /* fields for the entire slot and the number of single */
      /* field values contained in the slot.                 */
      /*=====================================================*/

      if (theConstraints->maxFields->value !=
	  SymbolData (theEnv)->PositiveInfinity)
	{
	  CL_ReturnExpression (theEnv, tempConstraints->maxFields);
	  tempConstraints->maxFields =
	    CL_GenConstant (theEnv, INTEGER_TYPE,
			    CL_CreateInteger (theEnv,
					      theConstraints->maxFields->
					      integerValue->contents -
					      numberOfSingleFields));
	}

      if ((numberOfMultifields == 1)
	  && (theConstraints->minFields->value !=
	      SymbolData (theEnv)->NegativeInfinity))
	{
	  CL_ReturnExpression (theEnv, tempConstraints->minFields);
	  tempConstraints->minFields =
	    CL_GenConstant (theEnv, INTEGER_TYPE,
			    CL_CreateInteger (theEnv,
					      theConstraints->minFields->
					      integerValue->contents -
					      numberOfSingleFields));
	}
    }

   /*================================================*/
  /* If a multifield slot is being parsed, place a  */
  /* node on top of the list of constraints parsed. */
   /*================================================*/

  if (multifieldSlot)
    {
      nextNode = CL_GetLHSParseNode (theEnv);
      nextNode->pnType = MF_WILDCARD_NODE;
      nextNode->multifieldSlot = true;
      nextNode->bottom = topNode;
      nextNode->slot = theSlot;
      nextNode->slotNumber = slotNumber;
      nextNode->index = startPosition;
      nextNode->constraints = theConstraints;
      topNode = nextNode;
      TallyFieldTypes (topNode->bottom);
    }

   /*=================================*/
  /* Return the list of constraints. */
   /*=================================*/

  return (topNode);
}

/***************************************************************/
/* TallyFieldTypes: Dete_rmines the number of single field and  */
/*   multifield variables and wildcards that appear before and */
/*   after each restriction found in a multifield slot.        */
/***************************************************************/
static void
TallyFieldTypes (struct lhsParseNode *theRestrictions)
{
  struct lhsParseNode *tempNode1, *tempNode2, *tempNode3;
  unsigned short totalSingleFields = 0, totalMultiFields = 0;
  unsigned short runningSingleFields = 0, runningMultiFields = 0;

   /*========================================*/
  /* Compute the total number of single and */
  /* multifield variables and wildcards.    */
   /*========================================*/

  for (tempNode1 = theRestrictions; tempNode1 != NULL;
       tempNode1 = tempNode1->right)
    {
      if ((tempNode1->pnType == SF_VARIABLE_NODE)
	  || (tempNode1->pnType == SF_WILDCARD_NODE))
	{
	  totalSingleFields++;
	}
      else
	{
	  totalMultiFields++;
	}
    }

   /*======================================================*/
  /* Loop through each constraint tallying the numbers of */
  /* the variable types before and after along the way.   */
   /*======================================================*/

  for (tempNode1 = theRestrictions; tempNode1 != NULL;
       tempNode1 = tempNode1->right)
    {
      /*===================================*/
      /* Assign the values to the "binding */
      /* variable" node of the constraint. */
      /*===================================*/

      tempNode1->singleFieldsBefore = runningSingleFields;
      tempNode1->multiFieldsBefore = runningMultiFields;
      tempNode1->withinMultifieldSlot = true;

      if ((tempNode1->pnType == SF_VARIABLE_NODE)
	  || (tempNode1->pnType == SF_WILDCARD_NODE))
	{
	  tempNode1->singleFieldsAfter =
	    totalSingleFields - (runningSingleFields + 1);
	  tempNode1->multiFieldsAfter = totalMultiFields - runningMultiFields;
	}
      else
	{
	  tempNode1->singleFieldsAfter =
	    totalSingleFields - runningSingleFields;
	  tempNode1->multiFieldsAfter =
	    totalMultiFields - (runningMultiFields + 1);
	}

      /*=====================================================*/
      /* Assign the values to each of the and (&) and or (|) */
      /* connected constraints within the constraint.        */
      /*=====================================================*/

      for (tempNode2 = tempNode1->bottom; tempNode2 != NULL;
	   tempNode2 = tempNode2->bottom)
	{
	  for (tempNode3 = tempNode2; tempNode3 != NULL;
	       tempNode3 = tempNode3->right)
	    {
	      tempNode3->singleFieldsBefore = tempNode1->singleFieldsBefore;
	      tempNode3->singleFieldsAfter = tempNode1->singleFieldsAfter;
	      tempNode3->multiFieldsBefore = tempNode1->multiFieldsBefore;
	      tempNode3->multiFieldsAfter = tempNode1->multiFieldsAfter;
	      tempNode3->withinMultifieldSlot = true;
	    }
	}

      /*=======================================*/
      /* Calculate the running total of single */
      /* and multifield constraints.           */
      /*=======================================*/

      if ((tempNode1->pnType == SF_VARIABLE_NODE)
	  || (tempNode1->pnType == SF_WILDCARD_NODE))
	{
	  runningSingleFields++;
	}
      else
	{
	  runningMultiFields++;
	}
    }
}

/*******************************************************************/
/* Conjuctive_RestrictionParse: Parses a single constraint field in */
/*   a pattern that is not a single field wildcard, multifield     */
/*   wildcard, or multifield variable. The field may consist of a  */
/*   number of subfields tied together using the & connective      */
/*   constraint and/or the | connective constraint.                */
/*                                                                 */
/* <connected-constraint>                                          */
/*            ::= <single-constraint> |                            */
/*                <single-constraint> & <connected-constraint> |   */
/*                <single-constraint> | <connected-constraint>     */
/*******************************************************************/
static struct lhsParseNode *
Conjuctive_RestrictionParse (Environment * theEnv,
			     const char *readSource,
			     struct token *theToken, bool *error)
{
  struct lhsParseNode *bindNode;
  struct lhsParseNode *theNode, *nextOr, *nextAnd;
  TokenType connectorType;

   /*=====================================*/
  /* Get the first node and dete_rmine if */
  /* it is a binding variable.           */
   /*=====================================*/

  theNode = Literal_RestrictionParse (theEnv, readSource, theToken, error);

  if (*error == true)
    {
      return NULL;
    }

  CL_GetToken (theEnv, readSource, theToken);

  if (((theNode->pnType == SF_VARIABLE_NODE)
       || (theNode->pnType == MF_VARIABLE_NODE))
      && (theNode->negated == false)
      && (theToken->tknType != OR_CONSTRAINT_TOKEN))
    {
      theNode->bindingVariable = true;
      bindNode = theNode;
      nextOr = NULL;
      nextAnd = NULL;
    }
  else
    {
      bindNode = CL_GetLHSParseNode (theEnv);
      if (theNode->pnType == MF_VARIABLE_NODE)
	bindNode->pnType = MF_WILDCARD_NODE;
      else
	bindNode->pnType = SF_WILDCARD_NODE;
      bindNode->negated = false;
      bindNode->bottom = theNode;
      nextOr = theNode;
      nextAnd = theNode;
    }

   /*===================================*/
  /* Process the connected constraints */
  /* within the constraint             */
   /*===================================*/

  while ((theToken->tknType == OR_CONSTRAINT_TOKEN) ||
	 (theToken->tknType == AND_CONSTRAINT_TOKEN))
    {
      /*==========================*/
      /* Get the next constraint. */
      /*==========================*/

      connectorType = theToken->tknType;

      CL_GetToken (theEnv, readSource, theToken);
      theNode =
	Literal_RestrictionParse (theEnv, readSource, theToken, error);

      if (*error == true)
	{
	  CL_ReturnLHSParseNodes (theEnv, bindNode);
	  return NULL;
	}

      /*=======================================*/
      /* Attach the new constraint to the list */
      /* of constraints for this field.        */
      /*=======================================*/

      if (connectorType == OR_CONSTRAINT_TOKEN)
	{
	  if (nextOr == NULL)
	    {
	      bindNode->bottom = theNode;
	    }
	  else
	    {
	      nextOr->bottom = theNode;
	    }
	  nextOr = theNode;
	  nextAnd = theNode;
	}
      else if (connectorType == AND_CONSTRAINT_TOKEN)
	{
	  if (nextAnd == NULL)
	    {
	      bindNode->bottom = theNode;
	      nextOr = theNode;
	    }
	  else
	    {
	      nextAnd->right = theNode;
	    }
	  nextAnd = theNode;
	}
      else
	{
	  CL_SystemError (theEnv, "RULEPSR", 1);
	  CL_ExitRouter (theEnv, EXIT_FAILURE);
	}

      /*==================================================*/
      /* Dete_rmine if any more restrictions are connected */
      /* to the current list of restrictions.             */
      /*==================================================*/

      CL_GetToken (theEnv, readSource, theToken);
    }

   /*==========================================*/
  /* Check for illegal mixing of single and   */
  /* multifield values within the constraint. */
   /*==========================================*/

  if (CheckForVariableMixing (theEnv, bindNode))
    {
      *error = true;
      CL_ReturnLHSParseNodes (theEnv, bindNode);
      return NULL;
    }

   /*========================*/
  /* Return the constraint. */
   /*========================*/

  return (bindNode);
}

/*****************************************************/
/* CheckForVariableMixing: Checks a field constraint */
/*   to dete_rmine if single and multifield variables */
/*   are illegally mixed within it.                  */
/*****************************************************/
static bool
CheckForVariableMixing (Environment * theEnv,
			struct lhsParseNode *theRestriction)
{
  struct lhsParseNode *tempRestriction;
  CONSTRAINT_RECORD *theConstraint;
  bool multifield = false;
  bool singlefield = false;
  bool constant = false;
  bool singleReturnValue = false;
  bool multiReturnValue = false;

   /*================================================*/
  /* If the constraint contains a binding variable, */
  /* dete_rmine whether it is a single field or      */
  /* multifield variable.                           */
   /*================================================*/

  if (theRestriction->pnType == SF_VARIABLE_NODE)
    singlefield = true;
  else if (theRestriction->pnType == MF_VARIABLE_NODE)
    multifield = true;

   /*===========================================*/
  /* Loop through each of the or (|) connected */
  /* constraints within the constraint.        */
   /*===========================================*/

  for (theRestriction = theRestriction->bottom;
       theRestriction != NULL; theRestriction = theRestriction->bottom)
    {
      /*============================================*/
      /* Loop through each of the and (&) connected */
      /* constraints within the or (|) constraint.  */
      /*============================================*/

      for (tempRestriction = theRestriction;
	   tempRestriction != NULL; tempRestriction = tempRestriction->right)
	{
	 /*=====================================================*/
	  /* Dete_rmine if the constraint contains a single field */
	  /* variable, multifield variable, constant (a single   */
	  /* field), a return value constraint of a function     */
	  /* returning a single field value, or a return value   */
	  /* constraint of a function returning a multifield     */
	  /* value.                                              */
	 /*=====================================================*/

	  if (tempRestriction->pnType == SF_VARIABLE_NODE)
	    singlefield = true;
	  else if (tempRestriction->pnType == MF_VARIABLE_NODE)
	    multifield = true;
	  else if (CL_ConstantNode (tempRestriction))
	    constant = true;
	  else if (tempRestriction->pnType == RETURN_VALUE_CONSTRAINT_NODE)
	    {
	      theConstraint =
		CL_FunctionCallToConstraintRecord (theEnv,
						   tempRestriction->
						   expression->value);
	      if (theConstraint->anyAllowed)
		{		/* Do nothing. */
		}
	      else if (theConstraint->multifieldsAllowed)
		multiReturnValue = true;
	      else
		singleReturnValue = true;
	      CL_RemoveConstraint (theEnv, theConstraint);
	    }
	}
    }

   /*================================================================*/
  /* Using a single field value (a single field variable, constant, */
  /* or function returning a single field value) together with a    */
  /* multifield value (a multifield variable or function returning  */
  /* a multifield value) is illegal. Return true if this occurs.    */
   /*================================================================*/

  if ((singlefield || constant || singleReturnValue) &&
      (multifield || multiReturnValue))

    {
      CL_PrintErrorID (theEnv, "PATTERN", 2, true);
      CL_WriteString (theEnv, STDERR,
		      "Single and multifield constraints cannot be mixed in a field constraint.\n");
      return true;
    }

   /*=======================================*/
  /* Otherwise return false to indicate no */
  /* illegal variable mixing was detected. */
   /*=======================================*/

  return false;
}

/***********************************************************/
/* Literal_RestrictionParse: Parses a single constraint.    */
/*   The constraint may be a literal constraint, a         */
/*   predicate constraint, a return value constraint, or a */
/*   variable constraint. The constraints may also be      */
/*   negated using the ~ connective constraint.            */
/*                                                         */
/* <single-constraint>     ::= <te_rm> | ~<te_rm>            */
/*                                                         */
/*  <te_rm>                 ::= <constant> |                */
/*                             <single-field-variable> |   */
/*                             <multi-field-variable> |    */
/*                             :<function-call> |          */
/*                             =<function-call>            */
/***********************************************************/
static struct lhsParseNode *
Literal_RestrictionParse (Environment * theEnv,
			  const char *readSource,
			  struct token *theToken, bool *error)
{
  struct lhsParseNode *topNode;
  struct expr *theExpression;

   /*============================================*/
  /* Create a node to represent the constraint. */
   /*============================================*/

  topNode = CL_GetLHSParseNode (theEnv);

   /*=================================================*/
  /* Dete_rmine if the constraint has a '~' preceding */
  /* it. If it  does, then the field is negated      */
  /* (e.g. ~red means "not the constant red."        */
   /*=================================================*/

  if (theToken->tknType == NOT_CONSTRAINT_TOKEN)
    {
      CL_GetToken (theEnv, readSource, theToken);
      topNode->negated = true;
    }
  else
    {
      topNode->negated = false;
    }

   /*===========================================*/
  /* Dete_rmine if the constraint is one of the */
  /* recognized types. These are ?variables,   */
  /* symbols, strings, numbers, :(expression), */
  /* and =(expression).                        */
   /*===========================================*/

   /*============================================*/
  /* Any symbol is valid, but an = signifies a  */
  /* return value constraint and an : signifies */
  /* a predicate constraint.                    */
   /*============================================*/

  if (theToken->tknType == SYMBOL_TOKEN)
    {
      /*==============================*/
      /* If the symbol is an =, parse */
      /* a return value constraint.   */
      /*==============================*/

      if (strcmp (theToken->lexemeValue->contents, "=") == 0)
	{
	  theExpression = CL_Function0Parse (theEnv, readSource);
	  if (theExpression == NULL)
	    {
	      *error = true;
	      CL_ReturnLHSParseNodes (theEnv, topNode);
	      return NULL;
	    }
	  topNode->pnType = RETURN_VALUE_CONSTRAINT_NODE;
	  topNode->expression =
	    CL_ExpressionToLHSParseNodes (theEnv, theExpression);
	  CL_ReturnExpression (theEnv, theExpression);
	}

      /*=============================*/
      /* If the symbol is a :, parse */
      /* a predicate constraint.     */
      /*=============================*/

      else if (strcmp (theToken->lexemeValue->contents, ":") == 0)
	{
	  theExpression = CL_Function0Parse (theEnv, readSource);
	  if (theExpression == NULL)
	    {
	      *error = true;
	      CL_ReturnLHSParseNodes (theEnv, topNode);
	      return NULL;
	    }
	  topNode->pnType = PREDICATE_CONSTRAINT_NODE;
	  topNode->expression =
	    CL_ExpressionToLHSParseNodes (theEnv, theExpression);
	  CL_ReturnExpression (theEnv, theExpression);
	}

      /*==============================================*/
      /* Otherwise, treat the constraint as a symbol. */
      /*==============================================*/

      else
	{
	  topNode->pnType = SYMBOL_NODE;
	  topNode->value = theToken->value;
	}
    }

   /*=====================================================*/
  /* Single and multifield variables and float, integer, */
  /* string, and instance name constants are also valid. */
   /*=====================================================*/

  else if ((theToken->tknType == SF_VARIABLE_TOKEN) ||
	   (theToken->tknType == MF_VARIABLE_TOKEN) ||
	   (theToken->tknType == FLOAT_TOKEN) ||
	   (theToken->tknType == INTEGER_TOKEN) ||
	   (theToken->tknType == STRING_TOKEN) ||
	   (theToken->tknType == INSTANCE_NAME_TOKEN))
    {
      topNode->pnType =
	CL_TypeToNodeType (CL_TokenTypeToType (theToken->tknType));
      topNode->value = theToken->value;
    }

   /*===========================*/
  /* Anything else is invalid. */
   /*===========================*/

  else
    {
      CL_SyntaxErrorMessage (theEnv, "defrule");
      *error = true;
      CL_ReturnLHSParseNodes (theEnv, topNode);
      return NULL;
    }

   /*===============================*/
  /* Return the parsed constraint. */
   /*===============================*/

  return topNode;
}

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

#endif /* DEFRULE_CONSTRUCT */
