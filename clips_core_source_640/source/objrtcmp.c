   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  02/03/18             */
   /*                                                     */
   /*    OBJECT PATTERN NETWORK CONSTRUCTS-TO-C MODULE    */
   /*******************************************************/

/*************************************************************/
/* Purpose: CL_Saves object pattern network for constructs-to-c */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*            Added environment parameter to CL_GenClose.       */
/*                                                           */
/*      6.30: Added support for path name argument to        */
/*            constructs-to-c.                               */
/*                                                           */
/*            Added support for hashed comparisons to        */
/*            constants.                                     */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.31: Optimization for marking relevant alpha nodes  */
/*            in the object pattern network.                 */
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

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM && (! RUN_TIME) && CONSTRUCT_COMPILER

#include <stdio.h>

#include "conscomp.h"
#include "classcom.h"
#include "envrnmnt.h"
#include "objrtfnx.h"
#include "objrtmch.h"
#include "pattern.h"
#include "sysdep.h"

#include "objrtcmp.h"

/* =========================================
   *****************************************
                 MACROS AND TYPES
   =========================================
   ***************************************** */
#define ObjectPNPrefix() ArbitraryPrefix(ObjectReteData(theEnv)->ObjectPatternCodeItem,0)
#define ObjectANPrefix() ArbitraryPrefix(ObjectReteData(theEnv)->ObjectPatternCodeItem,1)
#define ObjectALPrefix() ArbitraryPrefix(ObjectReteData(theEnv)->ObjectPatternCodeItem,2)

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    BeforeObjectPatternsToCode(Environment *);
   static OBJECT_PATTERN_NODE    *GetNextObjectPatternNode(OBJECT_PATTERN_NODE *);
   static void                    InitObjectPatternsCode(Environment *,FILE *,unsigned int,unsigned int);
   static bool                    ObjectPatternsToCode(Environment *,const char *,const char *,char *,
                                                       unsigned int,FILE *,unsigned int,unsigned int);
   static void                    InteCL_rmediatePatternNodeReference(Environment *,OBJECT_PATTERN_NODE *,FILE *,
                                                                   unsigned int,unsigned int);
   static unsigned                InteCL_rmediatePatternNodesToCode(Environment *,const char *,const char *,
                                                                 char *,unsigned int,FILE *,unsigned int,
                                                                 unsigned int,unsigned int);
   static unsigned                AlphaPatternNodesToCode(Environment *,const char *,const char *,char *,
                                                          unsigned int,FILE *,unsigned int,unsigned int,unsigned int);
   static unsigned                ClassAlphaLinksToCode(Environment *,const char *,const char *,char *,
                                                        unsigned int,FILE *,unsigned int,unsigned int,unsigned int);
   static CLASS_ALPHA_LINK       *GetNextAlphaLink(Environment *,struct defmodule **,Defclass **theClass,CLASS_ALPHA_LINK *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : CL_ObjectPatternsCompilerSetup
  DESCRIPTION  : Sets up interface for object
                 patterns to construct compiler
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Code generator item added
  NOTES        : None
 ***************************************************/
void CL_ObjectPatternsCompilerSetup(
  Environment *theEnv)
  {
   ObjectReteData(theEnv)->ObjectPatternCodeItem =
         CL_AddCodeGeneratorItem(theEnv,"object-patterns",0,BeforeObjectPatternsToCode,
                              InitObjectPatternsCode,ObjectPatternsToCode,3);
  }

/***************************************************
  NAME         : CL_ObjectPatternNodeReference
  DESCRIPTION  : Prints out a reference to an
                 object pattern alpha memory for
                 the join network interface to the
                 construct compiler
  INPUTS       : 1) A pointer to the object pattern
                    alpha memory
                 2) A pointer to the output file
                 3) The id of constructs-to-c image
                 4) The maximum number of indices
                    allowed in any single array
                    in the image
  RETURNS      : Nothing useful
  SIDE EFFECTS : Reference to object pattern alpha
                 memory printed
  NOTES        : None
 ***************************************************/
void CL_ObjectPatternNodeReference(
  Environment *theEnv,
  void *theVPattern,
  FILE *theFile,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   OBJECT_ALPHA_NODE *thePattern;

   if (theVPattern == NULL)
     fprintf(theFile,"NULL");
   else
     {
      thePattern = (OBJECT_ALPHA_NODE *) theVPattern;
      fprintf(theFile,"&%s%u_%lu[%lu]",
                      ObjectANPrefix(),imageID,
                      ((thePattern->bsaveID) / maxIndices) + 1,
                      (thePattern->bsaveID) % maxIndices);
     }
  }

/***************************************************
  NAME         : CL_ClassAlphaLinkReference
  DESCRIPTION  : Prints out a reference to a
                 class alpha link for
                 the pattern network interface to the
                 construct compiler
  INPUTS       : 1) A pointer to a class alpha link
                 2) A pointer to the output file
                 3) The id of constructs-to-c image
                 4) The maximum number of indices
                    allowed in any single array
                    in the image
  RETURNS      : Nothing useful
  SIDE EFFECTS : Reference to the class alpha link printed
  NOTES        : None
 ***************************************************/
void CL_ClassAlphaLinkReference(
  Environment *theEnv,
  void *theVLink,
  FILE *theFile,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   CLASS_ALPHA_LINK *theLink;

   if (theVLink == NULL)
     fprintf(theFile,"NULL");
   else
     {
      theLink = (CLASS_ALPHA_LINK *) theVLink;
      fprintf(theFile,"&%s%u_%lu[%lu]",
                      ObjectALPrefix(),imageID,
                      ((theLink->bsaveID) / maxIndices) + 1,
                      (theLink->bsaveID) % maxIndices);
     }
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/*****************************************************
  NAME         : BeforeObjectPatternsToCode
  DESCRIPTION  : Marks all object pattern inteCL_rmediate
                 and alpha memory nodes with a
                 unique integer id prior to the
                 constructs-to-c execution
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : bsaveIDs of nodes set
  NOTES        : None
 *****************************************************/
static void BeforeObjectPatternsToCode(
  Environment *theEnv)
  {
   unsigned long whichPattern;
   OBJECT_PATTERN_NODE *inteCL_rmediateNode;
   OBJECT_ALPHA_NODE *alphaNode;
   Defmodule *theModule;
   Defclass *theDefclass;
   CLASS_ALPHA_LINK *theLink;

   whichPattern = 0L;
   inteCL_rmediateNode = CL_ObjectNetworkPointer(theEnv);
   while (inteCL_rmediateNode != NULL)
     {
      inteCL_rmediateNode->bsaveID = whichPattern++;
      inteCL_rmediateNode = GetNextObjectPatternNode(inteCL_rmediateNode);
     }

   whichPattern = 0L;
   alphaNode = CL_ObjectNetworkTeCL_rminalPointer(theEnv);
   while (alphaNode != NULL)
     {
      alphaNode->bsaveID = whichPattern++;
      alphaNode = alphaNode->nxtTeCL_rminal;
     }
     
   whichPattern = 0L;
   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      CL_SetCurrentModule(theEnv,theModule);
      for (theDefclass = CL_GetNextDefclass(theEnv,NULL) ;
           theDefclass != NULL ;
           theDefclass = CL_GetNextDefclass(theEnv,theDefclass))
        {
         for (theLink = theDefclass->relevant_teCL_rminal_alpha_nodes;
              theLink != NULL;
              theLink = theLink->next)
           { theLink->bsaveID = whichPattern++; }
        }
     }
  }

/***************************************************
  NAME         : GetNextObjectPatternNode
  DESCRIPTION  : Grabs the next node in a depth
                 first perusal of the object pattern
                 inteCL_rmediate nodes
  INPUTS       : The previous node
  RETURNS      : The next node (NULL if done)
  SIDE EFFECTS : None
  NOTES        : Alpha meory nodes are ignored
 ***************************************************/
static OBJECT_PATTERN_NODE *GetNextObjectPatternNode(
  OBJECT_PATTERN_NODE *thePattern)
  {
   if (thePattern->nextLevel != NULL)
     return thePattern->nextLevel;
   while (thePattern->rightNode == NULL)
     {
      thePattern = thePattern->lastLevel;
      if (thePattern == NULL)
        return NULL;
     }
   return thePattern->rightNode;
  }

/***************************************************
  NAME         : InitObjectPatternsCode
  DESCRIPTION  : Prints out run-time initialization
                 code for object patterns
  INPUTS       : 1) A pointer to the output file
                 2) The id of constructs-to-c image
                 3) The maximum number of indices
                    allowed in any single array
                    in the image
  RETURNS      : Nothing useful
  SIDE EFFECTS : Initialization code written
  NOTES        : None
 ***************************************************/
static void InitObjectPatternsCode(
  Environment *theEnv,
  FILE *initFP,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   unsigned long firstInteCL_rmediateNode, firstAlphaNode;

   if (CL_ObjectNetworkPointer(theEnv) != NULL)
     {
      firstInteCL_rmediateNode = CL_ObjectNetworkPointer(theEnv)->bsaveID;
      firstAlphaNode = CL_ObjectNetworkTeCL_rminalPointer(theEnv)->bsaveID;
      fprintf(initFP,"   SetCL_ObjectNetworkPointer(theEnv,&%s%u_%lu[%lu]);\n",
                       ObjectPNPrefix(),imageID,
                       ((firstInteCL_rmediateNode / maxIndices) + 1),
                       (firstInteCL_rmediateNode % maxIndices));
      fprintf(initFP,"   SetCL_ObjectNetworkTeCL_rminalPointer(theEnv,&%s%u_%lu[%lu]);\n",
                       ObjectANPrefix(),imageID,
                       ((firstAlphaNode / maxIndices) + 1),
                       (firstAlphaNode % maxIndices));
     }
   else
     {
      fprintf(initFP,"   SetCL_ObjectNetworkPointer(theEnv,NULL);\n");
      fprintf(initFP,"   SetCL_ObjectNetworkTeCL_rminalPointer(theEnv,NULL);\n");
     }
  }

/***********************************************************
  NAME         : ObjectPatternsToCode
  DESCRIPTION  : CL_Writes out data structures for run-time
                 creation of object patterns
  INPUTS       : 1) The base image output file name
                 2) The base image file id
                 3) A pointer to the header output file
                 4) The id of constructs-to-c image
                 5) The maximum number of indices
                    allowed in any single array
                    in the image
  RETURNS      : 1 if OK, 0 if could not open a file
  SIDE EFFECTS : Object patterns code written to files
  NOTES        : None
 ***********************************************************/
static bool ObjectPatternsToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer,
  unsigned int fileID,
  FILE *headerFP,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   unsigned int version;
   
   version = ClassAlphaLinksToCode(theEnv,fileName,pathName,fileNameBuffer,
                                   fileID,headerFP,imageID,maxIndices,1);
   if (version == 0)
     return(0);

   version = InteCL_rmediatePatternNodesToCode(theEnv,fileName,pathName,fileNameBuffer,
                                            fileID,headerFP,imageID,maxIndices,version);
   if (version == 0)
     return false;
   if (! AlphaPatternNodesToCode(theEnv,fileName,pathName,fileNameBuffer,
                                 fileID,headerFP,imageID,maxIndices,version))
     return false;
   return true;
  }

/***************************************************
  NAME         : InteCL_rmediatePatternNodeReference
  DESCRIPTION  : Prints out a reference to an
                 object pattern inteCL_rmediate node
  INPUTS       : 1) A pointer to the object pattern
                    inteCL_rmediate node
                 2) A pointer to the output file
                 3) The id of constructs-to-c image
                 4) The maximum number of indices
                    allowed in any single array
                    in the image
  RETURNS      : 1 if OK, 0 if could not open a file
  SIDE EFFECTS : Reference to object pattern alpha
                 memory printed
  NOTES        : None
 ***************************************************/
static void InteCL_rmediatePatternNodeReference(
  Environment *theEnv,
  OBJECT_PATTERN_NODE *thePattern,
  FILE *theFile,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   if (thePattern == NULL)
     fprintf(theFile,"NULL");
   else
     {
      fprintf(theFile,"&%s%u_%lu[%lu]",
                    ObjectPNPrefix(),imageID,
                    ((thePattern->bsaveID) / maxIndices) + 1,
                    thePattern->bsaveID % maxIndices);
     }
  }

/*************************************************************
  NAME         : InteCL_rmediatePatternNodesToCode
  DESCRIPTION  : CL_Writes out data structures for run-time
                 creation of object pattern inteCL_rmediate nodes
  INPUTS       : 1) The base image output file name
                 2) The base image file id
                 3) A pointer to the header output file
                 4) The id of constructs-to-c image
                 5) The maximum number of indices
                    allowed in any single array
                    in the image
  RETURNS      : Next version file to open, 0 if error
  SIDE EFFECTS : Object patterns code written to files
  NOTES        : None
 *************************************************************/
static unsigned InteCL_rmediatePatternNodesToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer,
  unsigned int fileID,
  FILE *headerFP,
  unsigned int imageID,
  unsigned int maxIndices,
  unsigned int version)
  {
   FILE *fp;
   int arrayVersion;
   bool newHeader;
   unsigned int i;
   OBJECT_PATTERN_NODE *thePattern;

   /* ================
      Create the file.
      ================ */
   if (CL_ObjectNetworkPointer(theEnv) == NULL)
     { return 1; }

   fprintf(headerFP,"#include \"objrtmch.h\"\n");

   /* =================================
      Dump the pattern node structures.
      ================================= */
   if ((fp = CL_NewCFile(theEnv,fileName,pathName,fileNameBuffer,fileID,version,false)) == NULL)
     { return 0; }
   newHeader = true;

   arrayVersion = 1;
   i = 1;

   thePattern = CL_ObjectNetworkPointer(theEnv);
   while (thePattern != NULL)
     {
      if (newHeader)
        {
         fprintf(fp,"OBJECT_PATTERN_NODE %s%u_%u[] = {\n",
                     ObjectPNPrefix(),imageID,arrayVersion);
         fprintf(headerFP,"extern OBJECT_PATTERN_NODE %s%u_%u[];\n",
                     ObjectPNPrefix(),imageID,arrayVersion);
         newHeader = false;
        }
      fprintf(fp,"{0,%u,%u,%u,%u,%u,0L,%u,",thePattern->multifieldNode,
                                        thePattern->endSlot,
                                        thePattern->selector,
                                        thePattern->whichField,
                                        thePattern->leaveFields,
                                        thePattern->slotNameID);

      CL_PrintHashedExpressionReference(theEnv,fp,thePattern->networkTest,imageID,maxIndices);
      fprintf(fp,",");
      InteCL_rmediatePatternNodeReference(theEnv,thePattern->nextLevel,fp,imageID,maxIndices);
      fprintf(fp,",");
      InteCL_rmediatePatternNodeReference(theEnv,thePattern->lastLevel,fp,imageID,maxIndices);
      fprintf(fp,",");
      InteCL_rmediatePatternNodeReference(theEnv,thePattern->leftNode,fp,imageID,maxIndices);
      fprintf(fp,",");
      InteCL_rmediatePatternNodeReference(theEnv,thePattern->rightNode,fp,imageID,maxIndices);
      fprintf(fp,",");
      CL_ObjectPatternNodeReference(theEnv,thePattern->alphaNode,fp,imageID,maxIndices);
      fprintf(fp,",0L}");

      i++;
      thePattern = GetNextObjectPatternNode(thePattern);

      if ((i > maxIndices) || (thePattern == NULL))
        {
         fprintf(fp,"};\n");
         CL_GenClose(theEnv,fp);
         i = 1;
         version++;
         arrayVersion++;
         if (thePattern != NULL)
           {
            if ((fp = CL_NewCFile(theEnv,fileName,pathName,fileNameBuffer,fileID,version,false)) == NULL)
              { return 0; }
            newHeader = true;
           }
        }
      else if (thePattern != NULL)
        { fprintf(fp,",\n"); }
     }

   return version;
  }

/***********************************************************
  NAME         : AlphaPatternNodesToCode
  DESCRIPTION  : CL_Writes out data structures for run-time
                 creation of object pattern alpha memories
  INPUTS       : 1) The base image output file name
                 2) The base image file id
                 3) A pointer to the header output file
                 4) The id of constructs-to-c image
                 5) The maximum number of indices
                    allowed in any single array
                    in the image
  RETURNS      : Next version file to open, 0 if error
  SIDE EFFECTS : Object patterns code written to files
  NOTES        : None
 ***********************************************************/
static unsigned AlphaPatternNodesToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer,
  unsigned int fileID,
  FILE *headerFP,
  unsigned int imageID,
  unsigned int maxIndices,
  unsigned int version)
  {
   FILE *fp;
   unsigned int arrayVersion;
   bool newHeader;
   unsigned int i;
   OBJECT_ALPHA_NODE *thePattern;

   /* ================
      Create the file.
      ================ */
   if (CL_ObjectNetworkTeCL_rminalPointer(theEnv) == NULL)
     { return version; }

   /* =================================
      Dump the pattern node structures.
      ================================= */
   if ((fp = CL_NewCFile(theEnv,fileName,pathName,fileNameBuffer,fileID,version,false)) == NULL)
     { return 0; }
   newHeader = true;

   arrayVersion = 1;
   i = 1;

   thePattern = CL_ObjectNetworkTeCL_rminalPointer(theEnv);
   while (thePattern != NULL)
     {
      if (newHeader)
        {
         fprintf(fp,"OBJECT_ALPHA_NODE %s%u_%u[] = {\n",
                    ObjectANPrefix(),imageID,arrayVersion);
         fprintf(headerFP,"extern OBJECT_ALPHA_NODE %s%u_%u[];\n",
                          ObjectANPrefix(),imageID,arrayVersion);
         newHeader = false;
        }

      fprintf(fp,"{");

      CL_PatternNodeHeaderToCode(theEnv,fp,&thePattern->header,imageID,maxIndices);

      fprintf(fp,",0L,");
      CL_PrintBitMapReference(theEnv,fp,thePattern->classbmp);
      fprintf(fp,",");
      CL_PrintBitMapReference(theEnv,fp,thePattern->slotbmp);
      fprintf(fp,",");
      InteCL_rmediatePatternNodeReference(theEnv,thePattern->patternNode,fp,imageID,maxIndices);
      fprintf(fp,",");
      CL_ObjectPatternNodeReference(theEnv,thePattern->nxtInGroup,fp,imageID,maxIndices);
      fprintf(fp,",");
      CL_ObjectPatternNodeReference(theEnv,thePattern->nxtTeCL_rminal,fp,imageID,maxIndices);
      fprintf(fp,",0L}");

      i++;
      thePattern = thePattern->nxtTeCL_rminal;

      if ((i > maxIndices) || (thePattern == NULL))
        {
         fprintf(fp,"};\n");
         CL_GenClose(theEnv,fp);
         i = 1;
         version++;
         arrayVersion++;
         if (thePattern != NULL)
           {
            if ((fp = CL_NewCFile(theEnv,fileName,pathName,fileNameBuffer,fileID,version,false)) == NULL)
              { return 0; }
            newHeader = true;
           }
        }
      else if (thePattern != NULL)
        { fprintf(fp,",\n"); }
     }

   return version;
  }

/***********************************************************
  NAME         : ClassAlphaLinksToCode
  DESCRIPTION  : CL_Writes out data structures for run-time
                 creation of class alpha link
  INPUTS       : 1) The base image output file name
                 2) The base image file id
                 3) A pointer to the header output file
                 4) The id of constructs-to-c image
                 5) The maximum number of indices
                    allowed in any single array
                    in the image
  RETURNS      : Next version file to open, 0 if error
  SIDE EFFECTS : Class alpha links code written to files
  NOTES        : None
 ***********************************************************/
static unsigned ClassAlphaLinksToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer,
  unsigned int fileID,
  FILE *headerFP,
  unsigned int imageID,
  unsigned int maxIndices,
  unsigned int version)
  {
   FILE *fp;
   unsigned int arrayVersion;
   bool newHeader;
   unsigned int i;
   Defmodule *theModule = NULL;
   Defclass *theDefclass = NULL;
   CLASS_ALPHA_LINK *theLink = NULL;

   /* =================================
      Dump the alpha link structures.
      ================================= */
   if ((fp = CL_NewCFile(theEnv,fileName,pathName,fileNameBuffer,fileID,version,false)) == NULL)
     return(0);
   newHeader = true;

   arrayVersion = 1;
   i = 1;
 
   theLink = GetNextAlphaLink(theEnv,&theModule,&theDefclass,theLink);
   while (theLink != NULL)
     {
      if (newHeader)
        {
         fprintf(fp,"CLASS_ALPHA_LINK %s%u_%u[] = {\n",
                    ObjectALPrefix(),imageID,arrayVersion);
         fprintf(headerFP,"extern CLASS_ALPHA_LINK %s%u_%u[];\n",
                          ObjectALPrefix(),imageID,arrayVersion);
         newHeader = false;
        }

      fprintf(fp,"{");
      
      CL_ObjectPatternNodeReference(theEnv,theLink->alphaNode,fp,imageID,maxIndices);

      fprintf(fp,",");
         
      CL_ClassAlphaLinkReference(theEnv,theLink->next,fp,imageID,maxIndices);

      fprintf(fp,"}");
      
      theLink = GetNextAlphaLink(theEnv,&theModule,&theDefclass,theLink);
      
      if ((i > maxIndices) || (theLink == NULL))
        {
         fprintf(fp,"};\n");
         CL_GenClose(theEnv,fp);
         i = 1;
         version++;
         arrayVersion++;
         if (theLink != NULL)
           {
            if ((fp = CL_NewCFile(theEnv,fileName,pathName,fileNameBuffer,fileID,version,false)) == NULL)
              return(0);
            newHeader = true;
           }
        }
      else if (theLink != NULL)
        { fprintf(fp,",\n"); }
     }

   return(version);
  }

/********************/
/* GetNextAlphaLink */
/********************/
static CLASS_ALPHA_LINK *GetNextAlphaLink(
  Environment *theEnv,
  Defmodule **theModule,
  Defclass **theClass,
  CLASS_ALPHA_LINK *theLink)
  {
   while (true)
     {
      if (theLink != NULL)
        {
         theLink = theLink->next;
         
         if (theLink != NULL)
           { return theLink; }
        }
      else if (*theClass != NULL)
        {
         *theClass = CL_GetNextDefclass(theEnv,*theClass);
         if (*theClass != NULL)
           { theLink = (*theClass)->relevant_teCL_rminal_alpha_nodes; }
         if (theLink != NULL)
           { return theLink; }
        }
      else
        {
         *theModule = CL_GetNextDefmodule(theEnv,*theModule);
         if (*theModule == NULL)
           { return NULL; }
         CL_SetCurrentModule(theEnv,*theModule);
         *theClass = CL_GetNextDefclass(theEnv,*theClass);
         if (*theClass != NULL)
           {
            theLink = (*theClass)->relevant_teCL_rminal_alpha_nodes;
            if (theLink != NULL)
              { return theLink; }
           }
        }
     }
     
   return NULL;
  }

#endif
