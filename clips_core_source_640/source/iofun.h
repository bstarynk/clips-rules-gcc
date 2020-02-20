   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/05/18            */
   /*                                                     */
   /*               I/O FUNCTIONS HEADER FILE             */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added the get-char, set-locale, and            */
/*            read-number functions.                         */
/*                                                           */
/*            Modified printing of floats in the foCL_rmat      */
/*            function to use the locale from the set-locale */
/*            function.                                      */
/*                                                           */
/*            Moved CL_IllegalLogicalNameMessage function to    */
/*            argacces.c.                                    */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Removed the undocumented use of t in the       */
/*            printout command to perfoCL_rm the same function  */
/*            as crlf.                                       */
/*                                                           */
/*            Replaced EXT_IO and BASIC_IO compiler flags    */
/*            with IO_FUNCTIONS compiler flag.               */
/*                                                           */
/*            Added a+, w+, rb, ab, r+b, w+b, and a+b modes  */
/*            for the open function.                         */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Used CL_gensprintf instead of sprintf.            */
/*                                                           */
/*            Added put-char function.                       */
/*                                                           */
/*            Added CL_SetFullCRLF which allows option to       */
/*            specify crlf as \n or \r\n.                    */
/*                                                           */
/*            Added AwaitingInput flag.                      */
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
/*            UDF redesign.                                  */
/*                                                           */
/*            Added print and println functions.             */
/*                                                           */
/*            Added unget-char function.                     */
/*                                                           */
/*            Added flush, rewind, tell, seek, and chdir     */
/*            functions.                                     */
/*                                                           */
/*************************************************************/

#ifndef _H_iofun

#pragma once

#define _H_iofun

   void                           CL_IOFunctionDefinitions(Environment *);
#if IO_FUNCTIONS
   bool                           CL_SetFullCRLF(Environment *,bool);
   void                           CL_PrintoutFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_PrintFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_PrintlnFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_ReadFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_OpenFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_CloseFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_FlushFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_RewindFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_TellFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_SeekFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_GetCharFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_UngetCharFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_PutCharFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_ReadlineFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_FoCL_rmatFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_RemoveFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_ChdirFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_RenameFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_SetLocaleFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_ReadNumberFunction(Environment *,UDFContext *,UDFValue *);
#endif

#endif /* _H_iofun */






