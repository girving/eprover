/*-----------------------------------------------------------------------

File  : cte_termfunc.c

Author: Stephan Schulz

Contents
 
  Most of the user-level functionality for unshared terms.

  Copyright 1998, 1999 by the author.
  This code is released under the GNU General Public Licence.
  See the file COPYING in the main CLIB directory for details.
  Run "eprover -h" for contact information.

Changes

<1> Wed Feb 25 16:50:36 MET 1998
    Ripped from cte_terms.c (should be obsolete by now)

-----------------------------------------------------------------------*/


#include "cte_termfunc.h"



/*---------------------------------------------------------------------*/
/*                        Global Variables                             */
/*---------------------------------------------------------------------*/

bool      TermPrintLists = true;

/*---------------------------------------------------------------------*/
/*                      Forward Declarations                           */
/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/*                         Internal Functions                          */
/*---------------------------------------------------------------------*/


/*-----------------------------------------------------------------------
//
// Function: print_cons_list()
//
//   Print a list of $cons'ed terms, terminated with $nil. Abort on
//   not well-formed lists (no cons pairs!).
//
// Global Variables: -
//
// Side Effects    : Output
//
/----------------------------------------------------------------------*/

static void print_cons_list(FILE* out, Term_p list, Sig_p sig, DerefType deref)
{
   assert(SigSupportLists);
   putc('[', out);
   if(list->f_code == SIG_CONS_CODE)
   {
      assert(list->args);
      TermPrint(out, list->args[0], sig, deref);
      list = list->args[1];
      while(list->f_code == SIG_CONS_CODE)
      {
         putc(',', out);
         /* putc(' ', out); */
         assert(list->args);
         TermPrint(out, list->args[0], sig, deref);
         list = list->args[1];
      }
      assert(list->f_code == SIG_NIL_CODE);
   }
   putc(']', out);
}


/*-----------------------------------------------------------------------
//
// Function: parse_cons_list()
//
//   Parse a LOP list into an internal $cons list.
//
// Global Variables: -
//
// Side Effects    : Input, Memory operations
//
/----------------------------------------------------------------------*/

static Term_p parse_cons_list(Scanner_p in, Sig_p sig, VarBank_p vars)
{
   Term_p handle;
   Term_p current;
   
   assert(SigSupportLists);
   
   AcceptInpTok(in, OpenSquare);

   handle = TermDefaultCellAlloc();

   current = handle;
   
   if(!TestInpTok(in, CloseSquare))
   {

      current->f_code = SIG_CONS_CODE;
      current->arity = 2;
      current->args = TermArgArrayAlloc(2);
      current->args[0] = TermParse(in, sig, vars); 
      current->args[1] = TermDefaultCellAlloc();
      current = current->args[1];
      
      while(TestInpTok(in, Comma))
      {
         NextToken(in);
         current->f_code = SIG_CONS_CODE;             
         current->arity = 2;                          
         current->args = TermArgArrayAlloc(2);
         current->args[0] = TermParse(in, sig, vars); 
         TermCellDelProp(current->args[0], TPTopPos);
         current->args[1] = TermDefaultCellAlloc();
         current = current->args[1];      
      }
   }
   AcceptInpTok(in, CloseSquare);
   current->f_code = SIG_NIL_CODE;             
   
   return handle;
}
   
/*-----------------------------------------------------------------------
//
// Function: term_check_consistency_rek()
//
//   Traverse a tree and check if any one term cell occurs more than
//   once on any branch (which would make the term cyclic). Return the
//   first inconsistency found or NULL.
//
// Global Variables: -
//
// Side Effects    : Memory operations
//
/----------------------------------------------------------------------*/

static Term_p term_check_consistency_rek(Term_p term, PTree_p *branch,
					 DerefType deref)
{
   int      i;
   Term_p   res = NULL;

   term = TermDeref(term, &deref);
   putc('.', stdout);

   if(!PTreeStore(branch, term))
   {
      return term;
   }
   for(i=0; i<term->arity; i++)
   {
      if((res = term_check_consistency_rek(term->args[i], branch, deref)))
      {
	 break;
      }
   }
   PTreeDeleteEntry(branch, term);
   return res;
}


/*---------------------------------------------------------------------*/
/*                         Exported Functions                          */
/*---------------------------------------------------------------------*/

#ifdef SAFELOGIC
#include "../SAFELOGIC/csl_gen_num_term.c"
#endif


/*-----------------------------------------------------------------------
//
// Function: TermPrint()
//
//   Print a term to the given stream.
//
// Global Variables: TermPrintLists
//
// Side Effects    : Output 
//
/----------------------------------------------------------------------*/

void TermPrint(FILE* out, Term_p term, Sig_p sig, DerefType deref)
{
   assert(term);
   assert(sig||TermIsVar(term));
   
   term = TermDeref(term, &deref);

#ifdef NEVER_DEFINED
   if(TermCellQueryProp(term, TPRestricted))
   {
      fprintf(out, "*");
   }
   if(TermCellQueryProp(term, TPIsRewritten))
   {
      if(TermIsTopRewritten(term))
      {
	 fprintf(out, "=");
      }
      else
      {
	 fprintf(out, "+");
      }
   }   
#endif
   if(SigSupportLists && TermPrintLists &&
      ((term->f_code == SIG_NIL_CODE)||
       (term->f_code == SIG_CONS_CODE)))
   {
      print_cons_list(out, term, sig, deref);
   }
   else
   {
      if(TermIsVar(term))
      {
	 fprintf(out, "X%ld", -term->f_code);
      }
      else
      {
	 fprintf(out, SigFindName(sig, term->f_code));
         if(!TermIsConst(term))
         {
            assert(term->args);
            TermPrintArgList(out, term->args, term->arity, sig, deref);
         }
      }
   }
}


/*--------------------------------------------------------------------
//
// Function: TermPrintArgList()
//
//   Print an argument list (i.e. an array with at least one term
//   element) to the given stream.
//
// Global Variables: -
//
// Side Effects    : Output
//
/----------------------------------------------------------------------*/

void TermPrintArgList(FILE* out, Term_p *args, int arity, Sig_p sig,
		      DerefType deref)
{
   int i;

   assert(arity>=1);
   putc('(', out);
 
   TermPrint(out, args[0], sig, deref);
   
   for(i=1; i<arity; i++)
   {
      putc(',', out);
      /* putc(' ', out); */
      TermPrint(out, args[i], sig, deref);
   }
   putc(')', out);
}


/*-----------------------------------------------------------------------
//
// Function: TermParseOperator()
//
//   Parse an operator (i.e. an optional $, followed by an
//   identifier), store the representation into id and determine
//   wether it represents a variable or a function symbol by the
//   following rules:
//   - If it starts with a $, it's a function symbol (LOP global
//     variables are treated as function symbols by me).
//   - If it doesn't start with an upper case character, it's a
//     function symbol. 
//   - If it is followed by an opening bracket, its a function
//     symbol.
//   - Otherwise its a variable.
//   Return value is true if the symbol represents a variable.
//
// Global Variables: SigIdentToken
//
// Side Effects    : Read input
//
/----------------------------------------------------------------------*/

bool TermParseOperator(Scanner_p in, DStr_p id)
{
   bool res = true;

   if(TestInpTok(in, Dollar))
   {
      DStrAppendChar(id, '$');
      res = false;
      NextToken(in);
      if(!TestInpNoSkip(in))
      {
         AktTokenError(in, "Build-In symbol name incomplete", false);
      }
   }
   if(!isupper(DStrView(AktToken(in)->literal)[0]))
   {
      res = false;
   }
   DStrAppendStr(id, DStrView(AktToken(in)->literal));
   AcceptInpTok(in, SigIdentToken);
   if(TestInpTok(in, OpenBracket))
   {
      res = false;
   }
   return res;
}      

/*-----------------------------------------------------------------------
//
// Function: TermParse()
//
//   Parse a term from the given scanner object into the internal term
//   representation.
//
// Global Variables: -
//
// Side Effects    : Input, memory operations, may extend signature
//
/----------------------------------------------------------------------*/

Term_p TermParse(Scanner_p in, Sig_p sig, VarBank_p vars)
{
   Term_p     handle;
   DStr_p     id;
   DStr_p     source_name, errpos;
   long       line, column;
   StreamType type;

   if(SigSupportLists && TestInpTok(in, OpenSquare))
   {
      handle =  parse_cons_list(in, sig, vars);
   }
   else
   {
      id = DStrAlloc();
      line = AktToken(in)->line;
      column = AktToken(in)->column;
      source_name = DStrGetRef(AktToken(in)->source);
      type = AktToken(in)->stream_type;

#ifdef SAFELOGIC
      if(SigInterpreteNumbers(sig) && TestInpTok(in, PosInt))
      {
	 handle = TermIntRepresentation(sig, AktToken(in)->numval);
	 AcceptInpTok(in, PosInt);
      }
      else
#endif
      if(TermParseOperator(in, id))
      {
         handle = VarBankExtNameAssertAlloc(vars, DStrView(id));
      }      
      else 
      {
	 handle = TermDefaultCellAlloc();

         if(TestInpTok(in, OpenBracket))
         {
            handle->arity = TermParseArgList(in, &(handle->args), sig,
                                             vars);
         }
         else
         {
            handle->arity = 0;
         }
         handle->f_code = SigInsertId(sig, DStrView(id),
				      handle->arity, normal);
         if(!handle->f_code)
         {
            errpos = DStrAlloc();

            DStrAppendStr(errpos, PosRep(type, source_name, line, column));
            DStrAppendChar(errpos, ' ');
            DStrAppendStr(errpos, DStrView(id));
            DStrAppendStr(errpos, " used with arity ");
            DStrAppendInt(errpos, (long)handle->arity);
            DStrAppendStr(errpos, " but registered with arity ");
            DStrAppendInt(errpos,
                          (long)SigFindArity(sig, SigFindFCode(sig, DStrView(id))));
            Error(DStrView(errpos), SYNTAX_ERROR);
            DStrFree(errpos);
         }
      }
      DStrReleaseRef(source_name);
      DStrFree(id);
   }
   return handle;
}


/*-----------------------------------------------------------------------
//
// Function: TermParseArgList()
//
//   Parse a list of terms (comma-separated and enclosed in brackets)
//   into an array of term pointers. Return the number of terms
//   parsed, and make arg_anchor point to the array. Note: The array
//   has to have exactly the right size, as it will be handled by
//   Size[Malloc|Free] for efficiency reasons and may otherwise lead
//   to a memory leak. This leads to some complexity...
//   If the arglist is empty, return 0 and use the NULL pointer as
//   anchor. 
//
// Global Variables: -
//
// Side Effects    : Input, memory operations
//
/----------------------------------------------------------------------*/

int TermParseArgList(Scanner_p in, Term_p** arg_anchor, Sig_p sig,
                     VarBank_p vars) 
{
   Term_p *handle;
   int    arity;
   int    size;
   int    i;

   AcceptInpTok(in, OpenBracket);
   if(TestInpTok(in, CloseBracket))
   {
      NextToken(in);
      *arg_anchor = NULL;
      return 0;
   }
   size = TERMS_INITIAL_ARGS;
   handle = (Term_p*)SizeMalloc(size*sizeof(Term_p));
   arity = 0;
   handle[arity] = TermParse(in, sig, vars);
   arity++;
   while(TestInpTok(in, Comma))
   {
      NextToken(in);
      if(arity==size)
      {
         size+=TERMS_INITIAL_ARGS;
         handle = (Term_p*)SecureRealloc(handle, size*sizeof(Term_p));
	 printf("TermParseArgList()-Realloc()\n");
      }
      handle[arity] = TermParse(in, sig, vars);
      arity++;
   }
   AcceptInpTok(in, CloseBracket);
   *arg_anchor = TermArgArrayAlloc(arity);
   for(i=0;i<arity;i++)
   {
      (*arg_anchor)[i] = handle[i];
   }
   SizeFree(handle, size*sizeof(Term_p));
   
   return arity;
}


/*-----------------------------------------------------------------------
//
// Function: TermCopy()
//
//   Return a copy of a given term. The new term will be unshared
//   (except, of coure, for the variables) even if the original term
//   was shared. Variable cells will be allocated from the VarBank
//   given to the function.
//
// Global Variables: -
//
// Side Effects    : Memory operations
//
/----------------------------------------------------------------------*/

Term_p TermCopy(Term_p source, VarBank_p vars, DerefType deref)
{
   Term_p handle;
   int i;

   assert(source);

   source = TermDeref(source, &deref);
   
   handle = TermEquivCellAlloc(source, vars);
   
   for(i=0; i<handle->arity; i++) /* Hack: Loop will not be entered if
				     arity = 0 */
   {
      handle->args[i] = TermCopy(handle->args[i], vars, deref);
   }
   return handle;
}


/*-----------------------------------------------------------------------
//
// Function: TermEquivCellAlloc()
//
//   Return a pointer to a unshared termcell equivalent to source. If
//   source is a variable, get the cell from the varbank, otherwise
//   copy the cell via TermTopCopy().
//
// Global Variables: -
//
// Side Effects    : Memory operations
//
/----------------------------------------------------------------------*/

Term_p TermEquivCellAlloc(Term_p source, VarBank_p vars)
{
   Term_p handle;
   
   if(TermIsVar(source))
   {
      handle = VarBankFCodeAssertAlloc(vars, source->f_code);
   }
   else
   {
      handle = TermTopCopy(source);
   }
   return handle;
}


/*-----------------------------------------------------------------------
//
// Function: TermStructEqual()
//
//   Return true if the two terms have the same structure. Follows
//   bindings. 
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

bool TermStructEqual(Term_p t1, Term_p t2)
{
   PStack_p stack1 = PStackAlloc();
   PStack_p stack2 = PStackAlloc();
   int i;
   bool termeq = true;
   DerefType deref = DEREF_ALWAYS;

   PStackPushP(stack1, t1);
   PStackPushP(stack2, t2);
   
   while(!PStackEmpty(stack1) || !PStackEmpty(stack2))
   {
      if(PStackEmpty(stack1) || PStackEmpty(stack2))
      {
         termeq = false;
         break;
      }
      t1 = PStackPopP(stack1);
      t2 = PStackPopP(stack2);
      t1 = TermDeref(t1, &deref);
      t2 = TermDeref(t2, &deref);
      if(t1->f_code != t2->f_code)
      {
	 termeq = false;
	 break;
      }
      assert(t1->arity == t2->arity);
      for(i=0; i<t1->arity; i++)
      {
	 PStackPushP(stack1, t1->args[i]);
	 PStackPushP(stack2, t2->args[i]);
      }
   }
   PStackFree(stack1);
   PStackFree(stack2);
   return termeq;
}


/*-----------------------------------------------------------------------
//
// Function: TermStructEqualNoDeref()
//
//   Return true if the two terms have the same structures. Ignores
//   bindings. 
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

bool TermStructEqualNoDeref(Term_p t1, Term_p t2)
{
   PStack_p stack1 = PStackAlloc();
   PStack_p stack2 = PStackAlloc();
   int i;
   bool termeq = true;

   PStackPushP(stack1, t1);
   PStackPushP(stack2, t2);
   
   while(!PStackEmpty(stack1) && !PStackEmpty(stack2))
   {
      t1 = PStackPopP(stack1);
      t2 = PStackPopP(stack2);
      if(!t1 || !t2)
      {
         termeq = false;
         break;
      }
      else
      {
         if(t1->f_code != t2->f_code)
         {
            termeq = false;
            break;
         }
         else
         {
            assert(t1->arity == t2->arity);
            for(i=0; i<t1->arity; i++)
            {
               PStackPushP(stack1, t1->args[i]);
               PStackPushP(stack2, t2->args[i]);
            }
         }
      }
   }
   PStackFree(stack1);
   PStackFree(stack2);
   return termeq;
}


/*-----------------------------------------------------------------------
//
// Function: TermStructEqualDeref()
//
//   Return true if the two terms have the same
//   structures. Dereference both terms as designated by deref_1,
//   deref_2. 
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

bool TermStructEqualDeref(Term_p t1, Term_p t2, DerefType deref_1,
			  DerefType deref_2) 
{
   PStack_p stack1 = PStackAlloc();
   PStack_p stack2 = PStackAlloc();
   int i;
   bool termeq = true;
   
   PStackPushP(stack1, t1);
   PStackPushInt(stack1, deref_1);   
   PStackPushP(stack2, t2);
   PStackPushInt(stack2, deref_2);   
   
   while(!PStackEmpty(stack1))
   {
      assert(!PStackEmpty(stack2));
      
      deref_1 = PStackPopInt(stack1);
      t1 = PStackPopP(stack1);
      deref_2 = PStackPopInt(stack2);
      t2 = PStackPopP(stack2);
      
      t1 = TermDeref(t1, &deref_1);
      t2 = TermDeref(t2, &deref_2);
      
      if(t1->f_code != t2->f_code)
      {
	 termeq = false;
	 break;
      }
      else
      {
	 assert(t1->arity == t2->arity);
	 for(i=0; i<t1->arity; i++)
	 {
	    PStackPushP(stack1, t1->args[i]);
	    PStackPushInt(stack1, deref_1);   
	    PStackPushP(stack2, t2->args[i]);
	    PStackPushInt(stack2, deref_2);   
	 }
      }
   }
   PStackFree(stack1);
   PStackFree(stack2);
   return termeq;
}


/*-----------------------------------------------------------------------
//
// Function: TermStructWeightCompare()
//
//   Compare two terms based on just structural criteria: First
//   compare standard-weight, then compare top-symbol arity, then
//   compare subterms lexicographically. $true is always minimal.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

int TermStructWeightCompare(Term_p t1, Term_p t2)
{
   long res;
   short i;
   CompareResult subres;
   
   assert(t1);
   assert(t2);

   if(t1->f_code == SIG_TRUE_CODE)
   {
      assert(t1->arity == 0);
      if(t2->f_code == SIG_TRUE_CODE)
      {
	 assert(t2->arity == 0);
	 return 0;
      }
      return -1;
   }   
   if(t2->f_code == SIG_TRUE_CODE)
   {
      assert(t2->arity == 0);
      return 1;
   }
   res = TermStandardWeight(t1) - TermStandardWeight(t2);
   if(res)
   {
      return res;
   }
   if(TermIsVar(t1))
   { /* Then t2 also is a variable due to equal weights! */
      assert(TermIsVar(t2));
      return 0;
   }
   res = t1->arity - t2->arity;
   if(res)
   {
      return res;
   }
   for(i=0; i<t1->arity; i++)
   {
      subres = TermStructWeightCompare(t1->args[i], t2->args[i]);
      if(subres)
      {
	 return subres;
      }
   }
   return 0;
}


/*-----------------------------------------------------------------------
//
// Function: TermLexCompare()
//
//   Compare two terms lexicographically by f_codes.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

int TermLexCompare(Term_p t1, Term_p t2)
{
   long res;
   int i;

   res = t1->f_code - t2->f_code;
   if(res)
   {
      return res;
   }
   assert(t1->arity == t2->arity);
   for(i=0; i<t1->arity; i++)
   {
      res = TermLexCompare(t1->args[i], t2->args[i]);
      if(res)
      {
	 return res;
      }
   }   
   return 0;
}


/*-----------------------------------------------------------------------
//
// Function: TermIsSubterm()
//
//   Return true if test is a subterm to super. Parameterized by the
//   Equal-Funktion for terms.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

bool TermIsSubterm(Term_p super, Term_p test, DerefType deref,
                   TermEqualTestFun EqualTest)
{
   PStack_p stack = PStackAlloc();
   int i;
   bool subterm = false;

   PStackPushP(stack, super);
   PStackPushInt(stack, deref);
   
   while(!PStackEmpty(stack))
   {
      deref = PStackPopInt(stack);
      super = PStackPopP(stack);

      super = TermDeref(super, &deref);

      if(EqualTest(super, test))
      {
         subterm = true;
         break;
      }
      else
      {
         for(i=0; i<super->arity; i++)
         {
            PStackPushP(stack, super->args[i]);
	    PStackPushInt(stack, deref);
         }
      }
   }
   PStackFree(stack);

   return subterm;
}


/*-----------------------------------------------------------------------
//
// Function: TermIsSubtermDeref()
//
//   Return true if test is a subterm to super. Uses
//   TermStructEqualDeref() for equal test.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

bool TermIsSubtermDeref(Term_p super, Term_p test, DerefType
			deref_super, DerefType deref_test)
{
   PStack_p stack = PStackAlloc();
   int i;
   bool subterm = false;

   PStackPushP(stack, super);
   PStackPushInt(stack, deref_super);
   
   while(!PStackEmpty(stack))
   {
      deref_super = PStackPopInt(stack);
      super = PStackPopP(stack);

      super = TermDeref(super, &deref_super);

      if(TermStructEqualDeref(super, test, deref_super, deref_test))
      {
         subterm = true;
         break;
      }
      else
      {
         for(i=0; i<super->arity; i++)
         {
            PStackPushP(stack, super->args[i]);
	    PStackPushInt(stack, deref_super);
         }
      }
   }
   PStackFree(stack);
   
   return subterm;
}


/*-----------------------------------------------------------------------
//
// Function: TermWeight()
//
//   Compute the weight of a term, counting variables as vweight and
//   function symbols as fweight.
//
// Global Variables: -
//
// Side Effects    : Memory operations for the stack used.
//
/----------------------------------------------------------------------*/

long TermWeight(Term_p term, long vweight, long fweight)
{
   long     res = 0;
   PStack_p stack = PStackAlloc();
   Term_p   handle;

   assert(term);
   
   PStackPushP(stack, term);
   
   while(!PStackEmpty(stack))
   {
      handle = PStackPopP(stack);
      if(TermIsVar(handle))
      {
	 res += vweight;
      }
      else
      {
	 int i;

	 res += fweight;

	 for(i=0; i<handle->arity; i++)
	 {
	    PStackPushP(stack, handle->args[i]);
	 }
      }
   }
   PStackFree(stack);

   return res;
}


/*-----------------------------------------------------------------------
//
// Function: TermNonLinearWeight()
//
//   Compute the weight of a term, counting variables that occur for
//   the first time as vlweight, varaibes that reoccur as vweight, and
//   function symbols as fweight.
//
// Global Variables: -
//
// Side Effects    : Memory operations for the stack used.
//
/----------------------------------------------------------------------*/

long TermNonLinearWeight(Term_p term, long vlweight, long vweight,
			 long fweight)
{ 
   long     res = 0;
   PStack_p stack = PStackAlloc();
   Term_p   handle;
   
   assert(term);
   
   TermDelProp(term, DEREF_NEVER, TPOpFlag);

   PStackPushP(stack, term);
   
   while(!PStackEmpty(stack))
   {
      handle = PStackPopP(stack);
      if(TermIsVar(handle))
      {
	 if(TermCellQueryProp(handle, TPOpFlag))
	 {
	    res += vweight;
	 }
	 else
	 {
	    TermCellSetProp(handle, TPOpFlag);
	    res += vlweight;
	 }
      }
      else
      {
	 int i;
	 
	 res += fweight;
	 
	 for(i=0; i<handle->arity; i++)
	 {
	    PStackPushP(stack, handle->args[i]);
	 }
      }
   }
   PStackFree(stack);

   return res;
}


/*-----------------------------------------------------------------------
//
// Function: TermSymTypeWeight() 
//
//   Compute the weight of a term, giving different weight to
//   variables, constants, function symbols and predicates.
//
// Global Variables: -
//
// Side Effects    : Memory operations for the stack.
//
/----------------------------------------------------------------------*/

long TermSymTypeWeight(Term_p term, long vweight, long fweight, long
		       cweight, long pweight)
{
   long     res = 0;
   PStack_p stack = PStackAlloc();
   Term_p   handle;

   assert(term);
   
   PStackPushP(stack, term);
   
   while(!PStackEmpty(stack))
   {
      handle = PStackPopP(stack);
      if(TermIsVar(handle))
      {
	 res += vweight;
      }
      else
      {
	 int i;

	 if(TermCellQueryProp(handle,TPPredPos))
	 {
	    res += pweight;
	 }
	 else if(handle->arity==0)
	 {
	    res += cweight;
	 }
	 else
	 {
	    res += fweight;
	 }	    
	 for(i=0; i<handle->arity; i++)
	 {
	    PStackPushP(stack, handle->args[i]);
	 }
      }
   }
   PStackFree(stack);

   return res;
}

/*-----------------------------------------------------------------------
//
// Function: TermDepth()
//
//   Return the depth of a term.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

long TermDepth(Term_p term)
{
   long maxdepth = 0, ldepth;
   int  i;

   for(i=0; i<term->arity; i++)
   {
      ldepth = TermDepth(term->args[i]);
      maxdepth = MAX(maxdepth, ldepth);
   }
   return maxdepth+1;
}


/*-----------------------------------------------------------------------
//
// Function: TermIsDefTerm()
//
//   Return true if t is of the form f(X1...Xn) with n>=arity.
//
// Global Variables: -
//
// Side Effects    : Sets TPOpFlag
//
/----------------------------------------------------------------------*/

bool TermIsDefTerm(Term_p term, int min_arity)
{
   int i;

   assert(term);
   
   if(TermIsVar(term))
   {
      return false;
   }
   if(term->arity<min_arity)
   {
      return false;
   }
   if(TermStandardWeight(term)!=(DEFAULT_FWEIGHT+term->arity*DEFAULT_VWEIGHT))
   {
      return false;
   }
   for(i=0; i<term->arity; i++)
   {
      TermCellDelProp(term->args[i], TPOpFlag);
   }
   for(i=0; i<term->arity; i++)
   {
      if(TermCellQueryProp(term->args[i], TPOpFlag))
      {
	 return false;
      }
      TermCellSetProp(term->args[i], TPOpFlag);
   }
   return true;
}


/*-----------------------------------------------------------------------
//
// Function: TermHasFCode()
//
//   Return true if f occurs in term, false otherwise.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

bool TermHasFCode(Term_p term, FunCode f)
{
   int i;

   assert(term);

   if(term->f_code == f)
   {
      return true;
   }
   for(i=0; i<term->arity; i++)
   {
      if(TermHasFCode(term->args[i], f))
      {
	 return true;
      }
   }
   return false;
}


/*-----------------------------------------------------------------------
//
// Function: TermHasVariables()
//
//   Return true if the term contains (unbound) variables, false
//   otherwise. Does not follow bindings! 
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

bool TermHasVariables(Term_p term, bool unbound_only)
{
   PStack_p stack = PStackAlloc();
   bool     res = false;
   int      i;

   PStackPushP(stack,term);

   while(!PStackEmpty(stack) && !res)
   {
      term = PStackPopP(stack);
      if(TermIsVar(term))
      {
	 res = unbound_only ? (!term->binding):true;
      }
      else
      {
	 for(i=0; i<term->arity; i++)
	 {
	    PStackPushP(stack,term->args[i]);
	 }
      }
   }
   PStackFree(stack);
   return res;
}



/*-----------------------------------------------------------------------
//
// Function: VarBankCheckBindings()
//
//   For all variables in bank, check if they are bound. If sig!=0,
//   print the variable and binding as a comment, otherwise just print
//   variable number. Return number of bound variables.
//
// Global Variables: -
//
// Side Effects    : Output, Memory
//
/----------------------------------------------------------------------*/

long VarBankCheckBindings(FILE* out, VarBank_p bank, Sig_p sig)
{
   Term_p    term;
   long      res = 0;
   int       i;

   fprintf(out, "#  VarBankCheckBindings() started...\n");
   for(i=0; i<bank->f_code_index->size; i++)
   {
      term = PDArrayElementP(bank->f_code_index, i);      
      if(term)
      {
	 assert(TermIsVar(term));
	 if(term->binding)
	 {
	    res++;
	    if(sig)
	    {
	       fprintf(out, "# %ld: ", term->f_code);
	       TermPrint(out, term, sig, DEREF_NEVER);
	       fprintf(out, " <--- ");
	       TermPrint(out, term, sig, DEREF_ONCE);
	       fprintf(out, "\n");
	    }
	    else
	    {
	       fprintf(out, "# Var%ld <---- %p\n", 
		       term->f_code,
		       (void*)term->binding);
	    }
	 }
      }
   }
   fprintf(out, "#  ...VarBankCheckBindings() completed\n");
   return res;
}


/*-----------------------------------------------------------------------
//
// Function: TermAddSymbolDistributionLimited()
//
//   Count occurences of function symbols with f_code<limit in
//   dist_array. Terms are not dereferenced!
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

void TermAddSymbolDistributionLimited(Term_p term, long *dist_array, long limit)
{
   PStack_p stack = PStackAlloc();
   assert(term);

   PStackPushP(stack, term);
   
   while(!PStackEmpty(stack))
   {
      term = PStackPopP(stack);
      
      if(!TermIsVar(term))
      {
	 int i;
	 
	 assert(term->f_code > 0);
	 if(term->f_code < limit)
	 {
	    dist_array[term->f_code]++;
	 }
	 for(i=0; i<term->arity; i++)
	 {
	    assert(term->args);
	    PStackPushP(stack, term->args[i]);
	 }
      }
   }
   PStackFree(stack);
}

/*-----------------------------------------------------------------------
//
// Function: TermAddSymbolFeaturesLimited()
//
//   Add function symbol frequencies and deepest depth of a function
//   symbol to the two arrays. This is an extension of the function
//   above, this one does the extendet task in a single term
//   traversal. Note that function symbols >=limit are counted in
//   array[0] for both depth and frequency.
//
// Global Variables: -
//
// Side Effects    : Changes the arrays.
//
/----------------------------------------------------------------------*/

void TermAddSymbolFeaturesLimited(Term_p term, long depth, 
				  long *freq_array, long* depth_array,
				  long limit)
{
   if(!TermIsVar(term))
   {
      int i;

      if(term->f_code < limit)
      {
	 freq_array[term->f_code]++;
	 depth_array[term->f_code] = MAX(depth, depth_array[term->f_code]);
      }
      else
      {
	 freq_array[0]++;
	 depth_array[0] = MAX(depth, depth_array[0]);	 
      }
      for(i=0; i<term->arity; i++)
      {
	 TermAddSymbolFeaturesLimited(term->args[i], depth+1, 
				      freq_array, depth_array, 
				      limit);
      }
   }
}

/*-----------------------------------------------------------------------
//
// Function: TermComputeFunctionRanks()
//
//   Assing an occurance rank to each symbol in term.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

void TermComputeFunctionRanks(Term_p term, long *rank_array, long *count)
{
   int i;

   if(TermIsVar(term))
   {
      return;
   }
   for(i=0; i < term->arity; i++)
   {
      TermComputeFunctionRanks(term->args[i], rank_array, count);
   }
   if(!rank_array[term->f_code])
   {
      rank_array[term->f_code] = (*count)++;
   }
}


/*-----------------------------------------------------------------------
//
// Function: TermCollectPropVariables()
//
//   Insert all variables with properties prop in term into
//   tree. Return number of new variables. 
//
// Global Variables: -
//
// Side Effects    : Memory operations
//
/----------------------------------------------------------------------*/

long TermCollectPropVariables(Term_p term, PTree_p *tree,
			      TermProperties prop)
{
   long res = 0;
   PStack_p stack = PStackAlloc();
   int      i;

   PStackPushP(stack,term);

   while(!PStackEmpty(stack))
   {
      term = PStackPopP(stack);
      if(TermIsVar(term) && 
	 TermCellQueryProp(term,prop))
      {
	 if(PTreeStore(tree, term))
	 {
	    res++;
	 }
      }
      else
      {
	 for(i=0; i<term->arity; i++)
	 {
	    PStackPushP(stack,term->args[i]);
	 }
      }
   }
   PStackFree(stack);

   return res;
}


/*-----------------------------------------------------------------------
//
// Function: TermCheckConsistency()
//
//   Traverse a tree and check if any one term cell occurs more than
//   once on any branch (which would make the term cyclic). Return the
//   first inconsistency found or NULL.
//
// Global Variables: -
//
// Side Effects    : Memory operations
//
/----------------------------------------------------------------------*/

Term_p TermCheckConsistency(Term_p term, DerefType deref)
{
   Term_p   res;
   PTree_p  branch = NULL;
   
   printf("TermCheckConsistency...\n");
   res = term_check_consistency_rek(term, &branch, deref);
   assert(branch == 0);
   printf("...TermCheckConsistency\n");
   return res;
}

/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/



