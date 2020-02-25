   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/01/16            */
   /*                                                     */
   /*                PATTERN HEADER FILE                  */
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
/*      6.24: Removed LOGICAL_DEPENDENCIES compilation flag. */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Added support for hashed alpha memories.       */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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
/*            Removed initial-fact and initial-object        */
/*            support.                                       */
/*                                                           */
/*************************************************************/

#ifndef _H_pattern

#pragma once

#define _H_pattern

#include <stdio.h>

#include "evaluatn.h"
#include "entities.h"


#ifndef _H_symbol
#include "symbol.h"
#endif
#ifndef _H_scanner
#include "scanner.h"
#endif
#ifndef _H_expressn
#include "expressn.h"
#endif
#ifndef _H_match
#include "match.h"
#endif
#ifndef _H_reorder
#include "reorder.h"
#endif
#ifndef _H_constrnt
#include "constrnt.h"
#endif

#define MAXIMUM_NUMBER_OF_PATTERNS 128

struct patternParser
{
  const char *name;
  struct patternEntityRecord *entityType;
  unsigned short positionInArray;
  bool (*recognizeFunction) (CLIPSLexeme *);
  struct lhsParseNode *(*parseFunction) (Environment *, const char *,
					 struct token *);
  bool (*postAnalysisFunction) (Environment *, struct lhsParseNode *);
  struct patternNodeHeader *(*addPatternFunction) (Environment *,
						   struct lhsParseNode *);
  void (*removePatternFunction) (Environment *, struct patternNodeHeader *);
  struct expr *(*genJNConstantFunction) (void *, struct lhsParseNode *, int);
  void (*replaceGetJNValueFunction) (Environment *, struct expr *,
				     struct lhsParseNode *, int);
  struct expr *(*genGetJNValueFunction) (Environment *, struct lhsParseNode *,
					 int);
  struct expr *(*genCompareJNValuesFunction) (Environment *,
					      struct lhsParseNode *,
					      struct lhsParseNode *, bool);
  struct expr *(*genPNConstantFunction) (Environment *,
					 struct lhsParseNode *);
  void (*replaceGetPNValueFunction) (Environment *, struct expr *,
				     struct lhsParseNode *);
  struct expr *(*genGetPNValueFunction) (Environment *,
					 struct lhsParseNode *);
  struct expr *(*genComparePNValuesFunction) (Environment *,
					      struct lhsParseNode *,
					      struct lhsParseNode *);
  void (*returnUserDataFunction) (Environment *, void *);
  void *(*copyUserDataFunction) (Environment *, void *);
  void (*markIRPatternFunction) (Environment *, struct patternNodeHeader *,
				 bool);
  void (*incremental_ResetFunction) (Environment *);
  void (*codeReferenceFunction) (Environment *, void *, FILE *, unsigned int,
				 unsigned int);
  int priority;
  struct patternParser *next;
};

struct reservedSymbol
{
  const char *theSymbol;
  const char *reservedBy;
  struct reservedSymbol *next;
};

#define MAX_POSITIONS 8

#define PATTERN_DATA 19

struct patternData
{
  struct patternParser *ListOfPatternParsers;
  struct patternParser *PatternParserArray[MAX_POSITIONS];
  unsigned short NextPosition;
  struct reservedSymbol *ListOf_ReservedPatternSymbols;
  bool WithinNotCE;
  int GlobalSalience;
  bool GlobalAuto_Focus;
  struct expr *SalienceExpression;
  struct patternNodeHashEntry **PatternHashTable;
  unsigned long PatternHashTableSize;
};

#define PatternData(theEnv) ((struct patternData *) GetEnvironmentData(theEnv,PATTERN_DATA))

void CL_InitializePatterns (Environment *);
bool CL_AddPatternParser (Environment *, struct patternParser *);
struct patternParser *CL_FindPatternParser (Environment *, const char *);
void CL_DetachPattern (Environment *, unsigned short,
		       struct patternNodeHeader *);
void CL_GetNextPatternEntity (Environment *, struct patternParser **,
			      struct patternEntity **);
struct patternParser *CL_GetPatternParser (Environment *, unsigned short);
struct lhsParseNode *CL_RestrictionParse (Environment *, const char *,
					  struct token *, bool, CLIPSLexeme *,
					  unsigned short,
					  struct constraintRecord *,
					  unsigned short);
bool CL_PostPatternAnalysis (Environment *, struct lhsParseNode *);
void CL_PatternNodeHeaderToCode (Environment *, FILE *,
				 struct patternNodeHeader *, unsigned int,
				 unsigned int);
void CL_Add_ReservedPatternSymbol (Environment *, const char *, const char *);
bool CL_ReservedPatternSymbol (Environment *, const char *, const char *);
void CL_ReservedPatternSymbolErrorMsg (Environment *, const char *,
				       const char *);
void CL_AddHashedPatternNode (Environment *, void *, void *, unsigned short,
			      void *);
bool CL_RemoveHashedPatternNode (Environment *, void *, void *,
				 unsigned short, void *);
void *CL_FindHashedPatternNode (Environment *, void *, unsigned short,
				void *);

#endif /* _H_pattern */
