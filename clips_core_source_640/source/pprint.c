   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/02/17             */
   /*                                                     */
   /*                 PRETTY PRINT MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Routines for processing the pretty print         */
/*   representation of constructs.                           */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Chris Culbert                                        */
/*      Brian Dantes                                         */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Corrected code generating compilation          */
/*            warnings.                                      */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Used CL_genstrcpy instead of strcpy.              */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.31: Fix for pretty print buffer overflow.          */
/*                                                           */
/*      6.40: Added NULL pointer check in CL_CopyPPBuffer.      */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "sysdep.h"
#include "utility.h"

#include "pprint.h"

#define PP_CR_FIXED_BUFFER_SIZE 120

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    DeallocatePrettyPrintData(Environment *);

/****************************************************/
/* CL_InitializePrettyPrintData: Allocates environment */
/*    data for pretty print routines.               */
/****************************************************/
void CL_InitializePrettyPrintData(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,PRETTY_PRINT_DATA,sizeof(struct prettyPrintData),DeallocatePrettyPrintData);

   PrettyPrintData(theEnv)->PPBufferEnabled = true;
  }

/******************************************************/
/* DeallocatePrettyPrintData: Deallocates environment */
/*    data for the pretty print routines.             */
/******************************************************/
static void DeallocatePrettyPrintData(
  Environment *theEnv)
  {
   if (PrettyPrintData(theEnv)->PrettyPrintBuffer != NULL)
     { CL_rm(theEnv,PrettyPrintData(theEnv)->PrettyPrintBuffer,PrettyPrintData(theEnv)->PPBufferMax); }
  }

/*******************************************************/
/* CL_FlushPPBuffer: CL_Resets the pretty print save buffer. */
/*******************************************************/
void CL_FlushPPBuffer(
  Environment *theEnv)
  {
   if (PrettyPrintData(theEnv)->PrettyPrintBuffer == NULL) return;
   PrettyPrintData(theEnv)->CL_PPBackupOnce = 0;
   PrettyPrintData(theEnv)->CL_PPBackupTwice = 0;
   PrettyPrintData(theEnv)->PPBufferPos = 0;
   PrettyPrintData(theEnv)->PrettyPrintBuffer[0] = EOS;
   return;
  }

/*********************************************************************/
/* CL_DestroyPPBuffer: CL_Resets and removes the pretty print save buffer. */
/*********************************************************************/
void CL_DestroyPPBuffer(
  Environment *theEnv)
  {
   PrettyPrintData(theEnv)->CL_PPBackupOnce = 0;
   PrettyPrintData(theEnv)->CL_PPBackupTwice = 0;
   PrettyPrintData(theEnv)->PPBufferPos = 0;
   if (PrettyPrintData(theEnv)->PrettyPrintBuffer != NULL) CL_rm(theEnv,PrettyPrintData(theEnv)->PrettyPrintBuffer,PrettyPrintData(theEnv)->PPBufferMax);
   PrettyPrintData(theEnv)->PrettyPrintBuffer = NULL;
   PrettyPrintData(theEnv)->PPBufferMax = 0;
  }

/*********************************************/
/* CL_SavePPBuffer: Appends a string to the end */
/*   of the pretty print save buffer.        */
/*********************************************/
void CL_SavePPBuffer(
  Environment *theEnv,
  const char *str)
  {
   size_t increment;

   /*==========================================*/
   /* If the pretty print buffer isn't needed, */
   /* then don't bother writing to it.         */
   /*==========================================*/

   if ((PrettyPrintData(theEnv)->PPBufferStatus == false) || (! PrettyPrintData(theEnv)->PPBufferEnabled))
     { return; }

   /*===============================*/
   /* DeteCL_rmine the increment size. */
   /*===============================*/

   increment = 512;
   if (PrettyPrintData(theEnv)->PPBufferPos > increment)
     { increment = PrettyPrintData(theEnv)->PPBufferPos * 3; }

   /*================================================*/
   /* If the pretty print buffer isn't big enough to */
   /* contain the string, then increase its size.    */
   /*================================================*/

   if (strlen(str) + PrettyPrintData(theEnv)->PPBufferPos + 1 >= PrettyPrintData(theEnv)->PPBufferMax)
     {
      PrettyPrintData(theEnv)->PrettyPrintBuffer =
         (char *) CL_genrealloc(theEnv,PrettyPrintData(theEnv)->PrettyPrintBuffer,
                                    PrettyPrintData(theEnv)->PPBufferMax,
                                    PrettyPrintData(theEnv)->PPBufferMax + increment);
      PrettyPrintData(theEnv)->PPBufferMax += increment;
     }

   /*==================================================*/
   /* Remember the previous tokens saved to the pretty */
   /* print buffer in case it is necessary to back up. */
   /*==================================================*/

   PrettyPrintData(theEnv)->CL_PPBackupTwice = PrettyPrintData(theEnv)->CL_PPBackupOnce;
   PrettyPrintData(theEnv)->CL_PPBackupOnce = PrettyPrintData(theEnv)->PPBufferPos;

   /*=============================================*/
   /* CL_Save the string to the pretty print buffer. */
   /*=============================================*/

   PrettyPrintData(theEnv)->PrettyPrintBuffer = CL_AppendToString(theEnv,str,PrettyPrintData(theEnv)->PrettyPrintBuffer,&PrettyPrintData(theEnv)->PPBufferPos,&PrettyPrintData(theEnv)->PPBufferMax);
  }

/***************************************************/
/* CL_PPBackup:  Removes the last string added to the */
/*   pretty print save buffer. Only capable of     */
/*   backing up for the two most recent additions. */
/***************************************************/
void CL_PPBackup(
  Environment *theEnv)
  {
   if ((PrettyPrintData(theEnv)->PPBufferStatus == false) ||
       (PrettyPrintData(theEnv)->PrettyPrintBuffer == NULL) ||
       (! PrettyPrintData(theEnv)->PPBufferEnabled))
     { return; }

   PrettyPrintData(theEnv)->PPBufferPos = PrettyPrintData(theEnv)->CL_PPBackupOnce;
   PrettyPrintData(theEnv)->CL_PPBackupOnce = PrettyPrintData(theEnv)->CL_PPBackupTwice;
   PrettyPrintData(theEnv)->PrettyPrintBuffer[PrettyPrintData(theEnv)->PPBufferPos] = EOS;
  }

/**************************************************/
/* CL_CopyPPBuffer: Makes a copy of the pretty print */
/*   save buffer.                                 */
/**************************************************/
char *CL_CopyPPBuffer(
  Environment *theEnv)
  {
   size_t length;
   char *newString;
   char *theBuffer = PrettyPrintData(theEnv)->PrettyPrintBuffer;

   if (theBuffer == NULL) return NULL;

   length = (1 + strlen(theBuffer)) * sizeof(char);
   newString = (char *) CL_gm2(theEnv,length);

   CL_genstrcpy(newString,theBuffer);
   return(newString);
  }

/************************************************************/
/* CL_GetPPBuffer: Returns a pointer to the PrettyPrintBuffer. */
/************************************************************/
char *CL_GetPPBuffer(
  Environment *theEnv)
  {
   return(PrettyPrintData(theEnv)->PrettyPrintBuffer);
  }

/*******************************************/
/* CL_PPCRAndIndent: Prints white spaces into */
/*   the pretty print buffer.              */
/*******************************************/
void CL_PPCRAndIndent(
  Environment *theEnv)
  {
   size_t i;
   char *buffer;
   char fixedBuffer[PP_CR_FIXED_BUFFER_SIZE];
   
   if ((PrettyPrintData(theEnv)->PPBufferStatus == false) ||
       (! PrettyPrintData(theEnv)->PPBufferEnabled))
     { return; }

   if ((PrettyPrintData(theEnv)->IndentationDepth + 2) > PP_CR_FIXED_BUFFER_SIZE)
     { buffer = (char *) CL_genalloc(theEnv,PrettyPrintData(theEnv)->IndentationDepth + 2);}
   else
     { buffer = fixedBuffer; }
   
   buffer[0] = '\n';

   for (i = 1 ; i <= PrettyPrintData(theEnv)->IndentationDepth ; i++)
     { buffer[i] = ' '; }
   buffer[i] = EOS;

   CL_SavePPBuffer(theEnv,buffer);
   
   if ((PrettyPrintData(theEnv)->IndentationDepth + 2) > PP_CR_FIXED_BUFFER_SIZE)
     { CL_genfree(theEnv,buffer,PrettyPrintData(theEnv)->IndentationDepth + 2);}
  }

/************************************************/
/* CL_IncrementIndentDepth: Increments indentation */
/*   depth for pretty printing.                 */
/************************************************/
void CL_IncrementIndentDepth(
  Environment *theEnv,
  size_t value)
  {
   PrettyPrintData(theEnv)->IndentationDepth += value;
  }

/************************************************/
/* CL_DecrementIndentDepth: Decrements indentation */
/*   depth for pretty printing.                 */
/************************************************/
void CL_DecrementIndentDepth(
  Environment *theEnv,
  size_t value)
  {
   PrettyPrintData(theEnv)->IndentationDepth -= value;
  }

/************************************/
/* CL_SetIndentDepth: Sets indentation */
/*   depth for pretty printing.     */
/************************************/
void CL_SetIndentDepth(
  Environment *theEnv,
  size_t value)
  {
   PrettyPrintData(theEnv)->IndentationDepth = value;
  }

/******************************************/
/* CL_SetPPBufferStatus: Sets PPBufferStatus */
/*   flag to boolean value of ON or OFF.  */
/******************************************/
void CL_SetPPBufferStatus(
  Environment *theEnv,
  bool value)
  {
   PrettyPrintData(theEnv)->PPBufferStatus = value;
  }

/************************************/
/* CL_GetPPBufferStatus: Returns value */
/*   of the PPBufferStatus flag.    */
/************************************/
bool CL_GetPPBufferStatus(
  Environment *theEnv)
  {
   return(PrettyPrintData(theEnv)->PPBufferStatus);
  }

/***********************/
/* CL_SetPPBufferEnabled: */
/***********************/
bool CL_SetPPBufferEnabled(
  Environment *theEnv,
  bool value)
  {
   bool oldValue;

   oldValue = PrettyPrintData(theEnv)->PPBufferEnabled;
   PrettyPrintData(theEnv)->PPBufferEnabled = value;
   return oldValue;
  }

/***********************/
/* CL_GetPPBufferEnabled: */
/***********************/
bool CL_GetPPBufferEnabled(
  Environment *theEnv)
  {
   return PrettyPrintData(theEnv)->PPBufferEnabled;
  }

