   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/29/18             */
   /*                                                     */
   /*                PRINT UTILITY MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Utility routines for printing various items      */
/*   and messages.                                           */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Link error occurs for the CL_SlotExistError       */
/*            function when OBJECT_SYSTEM is set to 0 in     */
/*            setup.h. DR0865                                */
/*                                                           */
/*            Added CL_DataObjectToString function.             */
/*                                                           */
/*            Added CL_SlotExistError function.                 */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Support for DATA_OBJECT_ARRAY primitive.       */
/*                                                           */
/*            Support for typed EXTERNAL_ADDRESS_TYPE.       */
/*                                                           */
/*            Used CL_gensprintf and CL_genstrcat instead of       */
/*            sprintf and strcat.                            */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added code for capturing errors/warnings.      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*      6.31: Added additional error messages for retracted  */
/*            facts, deleted instances, and invalid slots.   */
/*                                                           */
/*            Added under/overflow error message.            */
/*                                                           */
/*      6.40: Added Env prefix to Get_EvaluationError and     */
/*            Set_EvaluationError functions.                  */
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
/*            Removed DATA_OBJECT_ARRAY primitive type.      */
/*                                                           */
/*            File name/line count displayed for errors      */
/*            and warnings during load command.              */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <string.h>

#include "setup.h"

#include "argacces.h"
#include "constant.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "evaluatn.h"
#include "factmngr.h"
#include "inscom.h"
#include "insmngr.h"
#include "memalloc.h"
#include "multifun.h"
#include "router.h"
#include "scanner.h"
#include "strngrtr.h"
#include "symbol.h"
#include "sysdep.h"
#include "utility.h"

#include "prntutil.h"

/*****************************************************/
/* CL_InitializePrintUtilityData: Allocates environment */
/*    data for print utility routines.               */
/*****************************************************/
void CL_InitializePrintUtilityData(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,PRINT_UTILITY_DATA,sizeof(struct printUtilityData),NULL);
  }

/************************************************************/
/* CL_WriteFloat: Controls printout of floating point numbers. */
/************************************************************/
void CL_WriteFloat(
  Environment *theEnv,
  const char *fileid,
  double number)
  {
   const char *theString;

   theString = CL_FloatToString(theEnv,number);
   CL_WriteString(theEnv,fileid,theString);
  }

/************************************************/
/* CL_WriteInteger: Controls printout of integers. */
/************************************************/
void CL_WriteInteger(
  Environment *theEnv,
  const char *logicalName,
  long long number)
  {
   char printBuffer[32];

   CL_gensprintf(printBuffer,"%lld",number);
   CL_WriteString(theEnv,logicalName,printBuffer);
  }

/*******************************************/
/* CL_PrintUnsignedInteger: Controls printout */
/*   of unsigned integers.                 */
/*******************************************/
void CL_PrintUnsignedInteger(
  Environment *theEnv,
  const char *logicalName,
  unsigned long long number)
  {
   char printBuffer[32];

   CL_gensprintf(printBuffer,"%llu",number);
   CL_WriteString(theEnv,logicalName,printBuffer);
  }

/**************************************/
/* CL_PrintAtom: Prints an atomic value. */
/**************************************/
void CL_PrintAtom(
  Environment *theEnv,
  const char *logicalName,
  unsigned short type,
  void *value)
  {
   CLIPSExternalAddress *theAddress;
   char buffer[20];

   switch (type)
     {
      case FLOAT_TYPE:
        CL_WriteFloat(theEnv,logicalName,((CLIPSFloat *) value)->contents);
        break;
      case INTEGER_TYPE:
        CL_WriteInteger(theEnv,logicalName,((CLIPSInteger *) value)->contents);
        break;
      case SYMBOL_TYPE:
        CL_WriteString(theEnv,logicalName,((CLIPSLexeme *) value)->contents);
        break;
      case STRING_TYPE:
        if (PrintUtilityData(theEnv)->PreserveEscapedCharacters)
          { CL_WriteString(theEnv,logicalName,StringPrintFo_rm(theEnv,((CLIPSLexeme *) value)->contents)); }
        else
          {
           CL_WriteString(theEnv,logicalName,"\"");
           CL_WriteString(theEnv,logicalName,((CLIPSLexeme *) value)->contents);
           CL_WriteString(theEnv,logicalName,"\"");
          }
        break;

      case EXTERNAL_ADDRESS_TYPE:
        theAddress = (CLIPSExternalAddress *) value;

        if (PrintUtilityData(theEnv)->AddressesToStrings) CL_WriteString(theEnv,logicalName,"\"");

        if ((CL_EvaluationData(theEnv)->ExternalAddressTypes[theAddress->type] != NULL) &&
            (CL_EvaluationData(theEnv)->ExternalAddressTypes[theAddress->type]->long_PrintFunction != NULL))
          { (*CL_EvaluationData(theEnv)->ExternalAddressTypes[theAddress->type]->long_PrintFunction)(theEnv,logicalName,value); }
        else
          {
           CL_WriteString(theEnv,logicalName,"<Pointer-");

           CL_gensprintf(buffer,"%d-",theAddress->type);
           CL_WriteString(theEnv,logicalName,buffer);

           CL_gensprintf(buffer,"%p",((CLIPSExternalAddress *) value)->contents);
           CL_WriteString(theEnv,logicalName,buffer);
           CL_WriteString(theEnv,logicalName,">");
          }

        if (PrintUtilityData(theEnv)->AddressesToStrings) CL_WriteString(theEnv,logicalName,"\"");
        break;

#if OBJECT_SYSTEM
      case INSTANCE_NAME_TYPE:
        CL_WriteString(theEnv,logicalName,"[");
        CL_WriteString(theEnv,logicalName,((CLIPSLexeme *) value)->contents);
        CL_WriteString(theEnv,logicalName,"]");
        break;
#endif

      case VOID_TYPE:
        break;

      default:
        if (CL_EvaluationData(theEnv)->PrimitivesArray[type] == NULL) break;
        if (CL_EvaluationData(theEnv)->PrimitivesArray[type]->long_PrintFunction == NULL)
          {
           CL_WriteString(theEnv,logicalName,"<unknown atom type>");
           break;
          }
        (*CL_EvaluationData(theEnv)->PrimitivesArray[type]->long_PrintFunction)(theEnv,logicalName,value);
        break;
     }
  }

/**********************************************************/
/* CL_PrintTally: Prints a tally count indicating the number */
/*   of items that have been displayed. Used by functions */
/*   such as list-defrules.                               */
/**********************************************************/
void CL_PrintTally(
  Environment *theEnv,
  const char *logicalName,
  unsigned long long count,
  const char *singular,
  const char *plural)
  {
   if (count == 0) return;

   CL_WriteString(theEnv,logicalName,"For a total of ");
   CL_PrintUnsignedInteger(theEnv,logicalName,count);
   CL_WriteString(theEnv,logicalName," ");

   if (count == 1) CL_WriteString(theEnv,logicalName,singular);
   else CL_WriteString(theEnv,logicalName,plural);

   CL_WriteString(theEnv,logicalName,".\n");
  }

/********************************************/
/* CL_PrintErrorID: Prints the module name and */
/*   error ID for an error message.         */
/********************************************/
void CL_PrintErrorID(
  Environment *theEnv,
  const char *module,
  int errorID,
  bool printCR)
  {
#if (! RUN_TIME) && (! BLOAD_ONLY)
   CL_FlushParsingMessages(theEnv);
   CL_SetErrorFileName(theEnv,CL_GetParsingFileName(theEnv));
   ConstructData(theEnv)->ErrLineNumber = CL_GetLineCount(theEnv);
#endif
   if (printCR) CL_WriteString(theEnv,STDERR,"\n");
   CL_WriteString(theEnv,STDERR,"[");
   CL_WriteString(theEnv,STDERR,module);
   CL_WriteInteger(theEnv,STDERR,errorID);
   CL_WriteString(theEnv,STDERR,"] ");

   /*==================================================*/
   /* Print the file name and line number if available */
   /* and there is no callback for errors/warnings.    */
   /*==================================================*/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   if ((ConstructData(theEnv)->ParserErrorCallback == NULL) &&
       (CL_Get_LoadInProgress(theEnv) == true))
     {
      const char *fileName;

      fileName = CL_GetParsingFileName(theEnv);
      if (fileName != NULL)
        {
         CL_WriteString(theEnv,STDERR,fileName);
         CL_WriteString(theEnv,STDERR,", Line ");
         CL_WriteInteger(theEnv,STDERR,CL_GetLineCount(theEnv));
         CL_WriteString(theEnv,STDERR,": ");
        }
     }
#endif
  }

/**********************************************/
/* CL_PrintWarningID: Prints the module name and */
/*   warning ID for a warning message.        */
/**********************************************/
void CL_PrintWarningID(
  Environment *theEnv,
  const char *module,
  int warningID,
  bool printCR)
  {
#if (! RUN_TIME) && (! BLOAD_ONLY)
   CL_FlushParsingMessages(theEnv);
   CL_SetWarningFileName(theEnv,CL_GetParsingFileName(theEnv));
   ConstructData(theEnv)->WrnLineNumber = CL_GetLineCount(theEnv);
#endif
   if (printCR) CL_WriteString(theEnv,STDWRN,"\n");
   CL_WriteString(theEnv,STDWRN,"[");
   CL_WriteString(theEnv,STDWRN,module);
   CL_WriteInteger(theEnv,STDWRN,warningID);
   CL_WriteString(theEnv,STDWRN,"] ");

   /*==================================================*/
   /* Print the file name and line number if available */
   /* and there is no callback for errors/warnings.    */
   /*==================================================*/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   if ((ConstructData(theEnv)->ParserErrorCallback == NULL) &&
       (CL_Get_LoadInProgress(theEnv) == true))
     {
      const char *fileName;

      fileName = CL_GetParsingFileName(theEnv);
      if (fileName != NULL)
        {
         CL_WriteString(theEnv,STDERR,fileName);
         CL_WriteString(theEnv,STDERR,", Line ");
         CL_WriteInteger(theEnv,STDERR,CL_GetLineCount(theEnv));
         CL_WriteString(theEnv,STDERR,", ");
        }
     }
#endif

   CL_WriteString(theEnv,STDWRN,"WARNING: ");
  }

/***************************************************/
/* CL_CantFindItemErrorMessage: Generic error message */
/*  when an "item" can not be found.               */
/***************************************************/
void CL_CantFindItemErrorMessage(
  Environment *theEnv,
  const char *itemType,
  const char *itemName,
  bool useQuotes)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",1,false);
   CL_WriteString(theEnv,STDERR,"Unable to find ");
   CL_WriteString(theEnv,STDERR,itemType);
   CL_WriteString(theEnv,STDERR," ");
   if (useQuotes) CL_WriteString(theEnv,STDERR,"'");
   CL_WriteString(theEnv,STDERR,itemName);
   if (useQuotes) CL_WriteString(theEnv,STDERR,"'");
   CL_WriteString(theEnv,STDERR,".\n");
  }

/*****************************************************/
/* CL_CantFindItemInFunctionErrorMessage: Generic error */
/*  message when an "item" can not be found.         */
/*****************************************************/
void CL_CantFindItemInFunctionErrorMessage(
  Environment *theEnv,
  const char *itemType,
  const char *itemName,
  const char *func,
  bool useQuotes)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",1,false);
   CL_WriteString(theEnv,STDERR,"Unable to find ");
   CL_WriteString(theEnv,STDERR,itemType);
   CL_WriteString(theEnv,STDERR," ");
   if (useQuotes) CL_WriteString(theEnv,STDERR,"'");
   CL_WriteString(theEnv,STDERR,itemName);
   if (useQuotes) CL_WriteString(theEnv,STDERR,"'");
   CL_WriteString(theEnv,STDERR," in function '");
   CL_WriteString(theEnv,STDERR,func);
   CL_WriteString(theEnv,STDERR,"'.\n");
  }

/*****************************************************/
/* CL_CantDeleteItemErrorMessage: Generic error message */
/*  when an "item" can not be deleted.               */
/*****************************************************/
void CL_CantDeleteItemErrorMessage(
  Environment *theEnv,
  const char *itemType,
  const char *itemName)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",4,false);
   CL_WriteString(theEnv,STDERR,"Unable to delete ");
   CL_WriteString(theEnv,STDERR,itemType);
   CL_WriteString(theEnv,STDERR," '");
   CL_WriteString(theEnv,STDERR,itemName);
   CL_WriteString(theEnv,STDERR,"'.\n");
  }

/****************************************************/
/* CL_AlreadyParsedErrorMessage: Generic error message */
/*  when an "item" has already been parsed.         */
/****************************************************/
void CL_AlreadyParsedErrorMessage(
  Environment *theEnv,
  const char *itemType,
  const char *itemName)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",5,true);
   CL_WriteString(theEnv,STDERR,"The ");
   if (itemType != NULL) CL_WriteString(theEnv,STDERR,itemType);
   if (itemName != NULL)
     {
      CL_WriteString(theEnv,STDERR,"'");
      CL_WriteString(theEnv,STDERR,itemName);
      CL_WriteString(theEnv,STDERR,"'");
     }
   CL_WriteString(theEnv,STDERR," has already been parsed.\n");
  }

/*********************************************************/
/* CL_SyntaxErrorMessage: Generalized syntax error message. */
/*********************************************************/
void CL_SyntaxErrorMessage(
  Environment *theEnv,
  const char *location)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",2,true);
   CL_WriteString(theEnv,STDERR,"Syntax Error");
   if (location != NULL)
     {
      CL_WriteString(theEnv,STDERR,":  Check appropriate syntax for ");
      CL_WriteString(theEnv,STDERR,location);
     }

   CL_WriteString(theEnv,STDERR,".\n");
   Set_EvaluationError(theEnv,true);
  }

/****************************************************/
/* CL_LocalVariableErrorMessage: Generic error message */
/*  when a local variable is accessed by an "item"  */
/*  which can not access local variables.           */
/****************************************************/
void CL_LocalVariableErrorMessage(
  Environment *theEnv,
  const char *byWhat)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",6,true);
   CL_WriteString(theEnv,STDERR,"Local variables can not be accessed by ");
   CL_WriteString(theEnv,STDERR,byWhat);
   CL_WriteString(theEnv,STDERR,".\n");
  }

/******************************************/
/* CL_SystemError: Generalized error message */
/*   for major internal errors.           */
/******************************************/
void CL_SystemError(
  Environment *theEnv,
  const char *module,
  int errorID)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",3,true);

   CL_WriteString(theEnv,STDERR,"\n*** ");
   CL_WriteString(theEnv,STDERR,APPLICATION_NAME);
   CL_WriteString(theEnv,STDERR," SYSTEM ERROR ***\n");

   CL_WriteString(theEnv,STDERR,"ID = ");
   CL_WriteString(theEnv,STDERR,module);
   CL_WriteInteger(theEnv,STDERR,errorID);
   CL_WriteString(theEnv,STDERR,"\n");

   CL_WriteString(theEnv,STDERR,APPLICATION_NAME);
   CL_WriteString(theEnv,STDERR," data structures are in an inconsistent or corrupted state.\n");
   CL_WriteString(theEnv,STDERR,"This error may have occurred from errors in user defined code.\n");
   CL_WriteString(theEnv,STDERR,"**************************\n");
  }

/*******************************************************/
/* CL_DivideByZeroErrorMessage: Generalized error message */
/*   for when a function attempts to divide by zero.   */
/*******************************************************/
void CL_DivideByZeroErrorMessage(
  Environment *theEnv,
  const char *functionName)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",7,false);
   CL_WriteString(theEnv,STDERR,"Attempt to divide by zero in '");
   CL_WriteString(theEnv,STDERR,functionName);
   CL_WriteString(theEnv,STDERR,"' function.\n");
  }

/********************************************************/
/* CL_ArgumentOverUnderflowErrorMessage: Generalized error */
/*   message for an integer under or overflow.          */
/********************************************************/
void CL_ArgumentOverUnderflowErrorMessage(
  Environment *theEnv,
  const char *functionName,
  bool error)
  {
   if (error)
     {
      CL_PrintErrorID(theEnv,"PRNTUTIL",17,false);
      CL_WriteString(theEnv,STDERR,"Over or underflow of long long integer in '");
      CL_WriteString(theEnv,STDERR,functionName);
      CL_WriteString(theEnv,STDERR,"' function.\n");
     }
   else
     {
      CL_PrintWarningID(theEnv,"PRNTUTIL",17,false);
      CL_WriteString(theEnv,STDWRN,"Over or underflow of long long integer in '");
      CL_WriteString(theEnv,STDWRN,functionName);
      CL_WriteString(theEnv,STDWRN,"' function.\n");
     }
  }

/*******************************************************/
/* CL_FloatToString: Converts number to KB string fo_rmat. */
/*******************************************************/
const char *CL_FloatToString(
  Environment *theEnv,
  double number)
  {
   char floatString[40];
   int i;
   char x;
   CLIPSLexeme *thePtr;

   CL_gensprintf(floatString,"%.15g",number);

   for (i = 0; (x = floatString[i]) != '\0'; i++)
     {
      if ((x == '.') || (x == 'e'))
        {
         thePtr = CL_CreateString(theEnv,floatString);
         return thePtr->contents;
        }
     }

   CL_genstrcat(floatString,".0");

   thePtr = CL_CreateString(theEnv,floatString);
   return thePtr->contents;
  }

/*******************************************************************/
/* CL_LongIntegerToString: Converts long integer to KB string fo_rmat. */
/*******************************************************************/
const char *CL_LongIntegerToString(
  Environment *theEnv,
  long long number)
  {
   char buffer[50];
   CLIPSLexeme *thePtr;

   CL_gensprintf(buffer,"%lld",number);

   thePtr = CL_CreateString(theEnv,buffer);
   return thePtr->contents;
  }

/******************************************************************/
/* CL_DataObjectToString: Converts a UDFValue to KB string fo_rmat. */
/******************************************************************/
const char *CL_DataObjectToString(
  Environment *theEnv,
  UDFValue *theDO)
  {
   CLIPSLexeme *thePtr;
   const char *theString;
   char *newString;
   const char *prefix, *postfix;
   size_t length;
   CLIPSExternalAddress *theAddress;
   String_Builder *theSB;
   
   char buffer[30];

   switch (theDO->header->type)
     {
      case MULTIFIELD_TYPE:
         prefix = "(";
         theString = CL_ImplodeMultifield(theEnv,theDO)->contents;
         postfix = ")";
         break;

      case STRING_TYPE:
         prefix = "\"";
         theString = theDO->lexemeValue->contents;
         postfix = "\"";
         break;

      case INSTANCE_NAME_TYPE:
         prefix = "[";
         theString = theDO->lexemeValue->contents;
         postfix = "]";
         break;

      case SYMBOL_TYPE:
         return theDO->lexemeValue->contents;

      case FLOAT_TYPE:
         return(CL_FloatToString(theEnv,theDO->floatValue->contents));

      case INTEGER_TYPE:
         return(CL_LongIntegerToString(theEnv,theDO->integerValue->contents));

      case VOID_TYPE:
         return("");

#if OBJECT_SYSTEM
      case INSTANCE_ADDRESS_TYPE:
         if (theDO->instanceValue == &InstanceData(theEnv)->DummyInstance)
           { return("<Dummy Instance>"); }

         if (theDO->instanceValue->garbage)
           {
            prefix = "<Stale Instance-";
            theString = theDO->instanceValue->name->contents;
            postfix = ">";
           }
         else
           {
            prefix = "<Instance-";
            theString = CL_GetFull_InstanceName(theEnv,theDO->instanceValue)->contents;
            postfix = ">";
           }

        break;
#endif

      case EXTERNAL_ADDRESS_TYPE:
        theAddress = theDO->externalAddressValue;
        
        theSB = CL_CreateString_Builder(theEnv,30);

        OpenString_BuilderDestination(theEnv,"DOTS",theSB);

        if ((CL_EvaluationData(theEnv)->ExternalAddressTypes[theAddress->type] != NULL) &&
            (CL_EvaluationData(theEnv)->ExternalAddressTypes[theAddress->type]->long_PrintFunction != NULL))
          { (*CL_EvaluationData(theEnv)->ExternalAddressTypes[theAddress->type]->long_PrintFunction)(theEnv,"DOTS",theAddress); }
        else
          {
           CL_WriteString(theEnv,"DOTS","<Pointer-");

           CL_gensprintf(buffer,"%d-",theAddress->type);
           CL_WriteString(theEnv,"DOTS",buffer);

           CL_gensprintf(buffer,"%p",theAddress->contents);
           CL_WriteString(theEnv,"DOTS",buffer);
           CL_WriteString(theEnv,"DOTS",">");
          }

        thePtr = CL_CreateString(theEnv,theSB->contents);
        CL_SBDispose(theSB);

        CloseString_BuilderDestination(theEnv,"DOTS");
        return thePtr->contents;

#if DEFTEMPLATE_CONSTRUCT
      case FACT_ADDRESS_TYPE:
         if (theDO->factValue == &FactData(theEnv)->DummyFact)
           { return("<Dummy Fact>"); }

         CL_gensprintf(buffer,"<Fact-%lld>",theDO->factValue->factIndex);
         thePtr = CL_CreateString(theEnv,buffer);
         return thePtr->contents;
#endif

      default:
         return("UNK");
     }

   length = strlen(prefix) + strlen(theString) + strlen(postfix) + 1;
   newString = (char *) CL_genalloc(theEnv,length);
   newString[0] = '\0';
   CL_genstrcat(newString,prefix);
   CL_genstrcat(newString,theString);
   CL_genstrcat(newString,postfix);
   thePtr = CL_CreateString(theEnv,newString);
   CL_genfree(theEnv,newString,length);
   return thePtr->contents;
  }

/************************************************************/
/* SalienceInfo_rmationError: Error message for errors which */
/*   occur during the evaluation of a salience value.       */
/************************************************************/
void SalienceInfo_rmationError(
  Environment *theEnv,
  const char *constructType,
  const char *constructName)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",8,true);
   CL_WriteString(theEnv,STDERR,"This error occurred while evaluating the salience");
   if (constructName != NULL)
     {
      CL_WriteString(theEnv,STDERR," for ");
      CL_WriteString(theEnv,STDERR,constructType);
      CL_WriteString(theEnv,STDERR," '");
      CL_WriteString(theEnv,STDERR,constructName);
      CL_WriteString(theEnv,STDERR,"'");
     }
   CL_WriteString(theEnv,STDERR,".\n");
  }

/**********************************************************/
/* CL_SalienceRangeError: Error message that is printed when */
/*   a salience value does not fall between the minimum   */
/*   and maximum salience values.                         */
/**********************************************************/
void CL_SalienceRangeError(
  Environment *theEnv,
  int min,
  int max)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",9,true);
   CL_WriteString(theEnv,STDERR,"Salience value out of range ");
   CL_WriteInteger(theEnv,STDERR,min);
   CL_WriteString(theEnv,STDERR," to ");
   CL_WriteInteger(theEnv,STDERR,max);
   CL_WriteString(theEnv,STDERR,".\n");
  }

/***************************************************************/
/* CL_SalienceNonIntegerError: Error message that is printed when */
/*   a rule's salience does not evaluate to an integer.        */
/***************************************************************/
void CL_SalienceNonIntegerError(
  Environment *theEnv)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",10,true);
   CL_WriteString(theEnv,STDERR,"Salience value must be an integer value.\n");
  }

/****************************************************/
/* CL_Fact_RetractedErrorMessage: Generic error message */
/*  when a fact has been retracted.                 */
/****************************************************/
void CL_Fact_RetractedErrorMessage(
  Environment *theEnv,
  Fact *theFact)
  {
   char tempBuffer[20];
   
   CL_PrintErrorID(theEnv,"PRNTUTIL",11,false);
   CL_WriteString(theEnv,STDERR,"The fact ");
   CL_gensprintf(tempBuffer,"f-%lld",theFact->factIndex);
   CL_WriteString(theEnv,STDERR,tempBuffer);
   CL_WriteString(theEnv,STDERR," has been retracted.\n");
  }

/****************************************************/
/* CL_FactVarSlotErrorMessage1: Generic error message  */
/*   when a var/slot reference accesses a fact that */
/*   has been retracted.                            */
/****************************************************/
void CL_FactVarSlotErrorMessage1(
  Environment *theEnv,
  Fact *theFact,
  const char *varSlot)
  {
   char tempBuffer[20];
   
   CL_PrintErrorID(theEnv,"PRNTUTIL",12,false);
   
   CL_WriteString(theEnv,STDERR,"The variable/slot reference ?");
   CL_WriteString(theEnv,STDERR,varSlot);
   CL_WriteString(theEnv,STDERR," cannot be resolved because the referenced fact ");
   CL_gensprintf(tempBuffer,"f-%lld",theFact->factIndex);
   CL_WriteString(theEnv,STDERR,tempBuffer);
   CL_WriteString(theEnv,STDERR," has been retracted.\n");
  }

/****************************************************/
/* CL_FactVarSlotErrorMessage2: Generic error message  */
/*   when a var/slot reference accesses an invalid  */
/*   slot.                                          */
/****************************************************/
void CL_FactVarSlotErrorMessage2(
  Environment *theEnv,
  Fact *theFact,
  const char *varSlot)
  {
   char tempBuffer[20];
   
   CL_PrintErrorID(theEnv,"PRNTUTIL",13,false);
   
   CL_WriteString(theEnv,STDERR,"The variable/slot reference ?");
   CL_WriteString(theEnv,STDERR,varSlot);
   CL_WriteString(theEnv,STDERR," is invalid because the referenced fact ");
   CL_gensprintf(tempBuffer,"f-%lld",theFact->factIndex);
   CL_WriteString(theEnv,STDERR,tempBuffer);
   CL_WriteString(theEnv,STDERR," does not contain the specified slot.\n");
  }

/******************************************************/
/* CL_InvalidVarSlotErrorMessage: Generic error message  */
/*   when a var/slot reference accesses an invalid    */
/*   slot.                                            */
/******************************************************/
void CL_InvalidVarSlotErrorMessage(
  Environment *theEnv,
  const char *varSlot)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",14,false);
   
   CL_WriteString(theEnv,STDERR,"The variable/slot reference ?");
   CL_WriteString(theEnv,STDERR,varSlot);
   CL_WriteString(theEnv,STDERR," is invalid because slot names must be symbols.\n");
  }

/*******************************************************/
/* CL_InstanceVarSlotErrorMessage1: Generic error message */
/*   when a var/slot reference accesses an instance    */
/*   that has been deleted.                            */
/*******************************************************/
void CL_InstanceVarSlotErrorMessage1(
  Environment *theEnv,
  Instance *theInstance,
  const char *varSlot)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",15,false);
   
   CL_WriteString(theEnv,STDERR,"The variable/slot reference ?");
   CL_WriteString(theEnv,STDERR,varSlot);
   CL_WriteString(theEnv,STDERR," cannot be resolved because the referenced instance [");
   CL_WriteString(theEnv,STDERR,theInstance->name->contents);
   CL_WriteString(theEnv,STDERR,"] has been deleted.\n");
  }
  
/************************************************/
/* CL_InstanceVarSlotErrorMessage2: Generic error  */
/*   message when a var/slot reference accesses */
/*   an invalid slot.                           */
/************************************************/
void CL_InstanceVarSlotErrorMessage2(
  Environment *theEnv,
  Instance *theInstance,
  const char *varSlot)
  {
   CL_PrintErrorID(theEnv,"PRNTUTIL",16,false);
   
   CL_WriteString(theEnv,STDERR,"The variable/slot reference ?");
   CL_WriteString(theEnv,STDERR,varSlot);
   CL_WriteString(theEnv,STDERR," is invalid because the referenced instance [");
   CL_WriteString(theEnv,STDERR,theInstance->name->contents);
   CL_WriteString(theEnv,STDERR,"] does not contain the specified slot.\n");
  }

/***************************************************/
/* CL_SlotExistError: Prints out an appropriate error */
/*   message when a slot cannot be found for a     */
/*   function. Input to the function is the slot   */
/*   name and the function name.                   */
/***************************************************/
void CL_SlotExistError(
  Environment *theEnv,
  const char *sname,
  const char *func)
  {
   CL_PrintErrorID(theEnv,"INSFUN",3,false);
   CL_WriteString(theEnv,STDERR,"No such slot '");
   CL_WriteString(theEnv,STDERR,sname);
   CL_WriteString(theEnv,STDERR,"' in function '");
   CL_WriteString(theEnv,STDERR,func);
   CL_WriteString(theEnv,STDERR,"'.\n");
   Set_EvaluationError(theEnv,true);
  }
