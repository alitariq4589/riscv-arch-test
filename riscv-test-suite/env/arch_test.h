// This file is divided into the following sections:
//	RV Arch Test Constants
//	general test and helper macros, required,  optional, or just useful
//	   _ARGn, SIG[BASE/UPD[_F/ID]],BASE_UPD,BIT,LA[U],LI[U],RVTEST_[INIT/SAVE]_GPRS, XCSR_RENAME
//	RV ARCH Test Interrupt Macros ****FIXME:spec which regs must not be altered
//	primary macros used by handle: RVTEST_TRAP{_PROLOG/_HANDLER/_EPILOG/SAVEAREA}
//	required test format spec macros:      RVTEST_{Code)/DATA/SIG}{_BEGIN/_END}
//	macros from Andrew Waterman's risc-v test macros
//	deprecated macro name aliases, just for migration ease

//  The resulting memory layout of the trap handler is (MACRO_NAME, label [function])
//****************************************************************
//  (code section)
// RVMODEL_BOOT
//	rvtest_entry_point: [boot code]
// RVTEST_CODE_BEGIN
//	rvtest_init:	   [TRAP_PROLOG]   (m, ms, or msv)
//			   [INIT_GPRS]
//	rvtest_code_begin:
//*****************************
//********(body of tests)******
//*****************************
// RVTEST_CODE_END
//	rvtest_code_end:   [*optional* SAVE_GPRS routine]
//			   [RVTEST_GOTO_MMODE ] **FIXME** this won't work if MMU enabled unless VA=PA
//	cleanup_epilogs	   [TRAP_EPILOG	  (m, ms, or msv)] (jump to exit_cleanup)
//			   [TRAP_HANDLER  (m, ms, or msv)]
//	exit_cleanup:	   [RVMODEL_HALT macro or a branch to it.]
//
//--------------------------------this could start a new section--------------------------------
//  (Data section) - align to 4K boundary
// RVTEST_DATA_BEGIN
//**************************************
//*****(Ld/St test data is here)********
//**************************************
//
//**************************************
//*****(trap handler data is here)******
//**************************************
//
//    rvtest_trap_sig:	 [global trap signature start (shared by all modes) inited to mtrap_sigptr] **FIXME: needs VA=PA
//    RVTEST_TRAP_SAVEAREA	[handler sv area(m, ms, or msv) temp reg save), CSRs, tramp table, ptrs]
//	rvtest_data_begin: [input data	   (shared by all modes)]
//    RVTEST_DATA_END
//	rvtest_data_end:
//    RVTEST_ROOT_PG_TBL [sets up identity map (VA=PA)
//	sroot_pg_tbl:	(if smode)
//	vroot_pg_tbl:	(if hypervisor)
//--------------------------------this could start a new section--------------------------------
// RVTEST_SIG_BEGIN
//    RVMODEL_DATA_BEGIN
//	rvtest_sig_begin:  [beginning of signature, used by signature dump, can be used by tests]
//	mtrap_sigptr:	   [global trap signature start (shared by all modes)] - defined by tests
//	gpr_save:	   [gpr save area (optional, enabled if rvtest_gpr_save is defined)]
// RVTEST_SIG_END
//    rvtest_sig_end:	[global test   end signature (shared by all modes)] (shouldn't matter what RVMODEL_DATA_END does)
//    RVMODEL_DATA_END
//--------------------------------end of test--------------------------------

/* The following macros are optional if interrupt tests are enabled (defaulted if not defined):
	RVMODEL_SET_[M/V/S]_[SW]_INT
	RVMODEL_CLR_[M/V/S]_[SW/TIMTER/EXT]_INT
	rvtest_[M/V/S]trap_routine
	GOTO_[M/S/U]MODE, INSTANTIATE_MODE_MACRO (prolog/handler/epilog/savearea)
   The following macro is optional, and defaults to fence.i if not defined
	RVMODEL.FENCEI
   The following variables are used	if interrupt tests are enabled (defaulted if not defined):
	 NUM_SPECD_INTCAUSES
   The following variables are optional if exception tests are enabled (defaulted if not defined):
	 DATA_REL_TVAL_MSK     CODE_REL_TVAL_MSK
   The following variables are optional:
	 rvtest_gpr_save: if defined, stores GPR contents into signature at test end (for debug)
   The following labels are required and defined by required macros:
	  rvtest_code_begin:   defined by RVTEST_CODE_BEGIN  macro (boot code can precede this)
	  rvtest_code_end:     defined by RVTEST_CODE_END    macro (trap handlers follow this)
	  rvtest_data_begin:   defined by RVTEST_DATA_BEGIN  macro
	  rvtest_data_end:     defined by RVTEST_DATA_END    macro
	  rvtest_sig_begin:    defined by RVTEST_SIG_BEGIN   macro (after  RVMODEL_DATA_BEGIN) defines signature begin
	  rvtest_sig_end:      defined by RVTEST_SIG_END     macro (before RVMODEL_DATA_END)   defines signature end
	  rvtest_Sroot_pg_tbl: defined by RVTEST_PTE_IDENT_MAP macro inside RVTEST_DATA_BEGIN if  Smode implemented
	  rvtest_Vroot_pg_tbl: defined by RVTEST_PTE_IDENT_MAP macro inside RVTEST_DATA_BEGIN if VSmode implemented
    labels/variables that must be defined by the DUT in model specific macros or #defines
	   mtrap_sigptr:       defined by test if traps are possible, else is defaulted
*/
// don't put C-style macros (#define xxx) inside assembly macros; C-style is evaluated before assembly

#include "encoding.h"
#include "test_macros.h"
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MASK_XLEN(x) ((x) & ((1 << (__riscv_xlen - 1) << 1) - 1))
#define BIT(addr, bit) (((addr)>>bit)&1)
#define SEXT_IMM(x)    ((x&0x0FFF) | (-BIT(x,11)<<12))

#define REGWIDTH (XLEN>>3)	// in units of #bytes
#define MASK (((1<<(XLEN-1))-1) + (1<<(XLEN-1)))	// XLEN bits of 1s

#define ALIGNSZ ((XLEN>>5)+2)	// log2(XLEN): 2,3,4 for XLEN 32,64,128
#if XLEN>FLEN
  #define SIGALIGN REGWIDTH
#else
  #define SIGALIGN FREGWIDTH
#endif

//==============================================================================
// this section has RV Arch Test Constants, mostly YAML based.
// It ensures they're defined  & defaulted if necessary)
//==============================================================================

// set defaults
#ifndef	  NUM_SPECD_INTCAUSES
  #define NUM_SPECD_INTCAUSES 16
#endif

	// set defaults
#ifndef	  NUM_SPECD_EXCPTCAUSES
  #define NUM_SPECD_EXCPTCAUSES 16
#endif

#ifndef	  RVMODEL_FENCEI
  #define RVMODEL_FENCEI fence.i				// make sure ifetches get new code
#endif

#ifndef UNROLLSZ
  #define UNROLLSZ 5
#endif

// **Note** that this is different that previous DATA_REL_TVAL_MASK! This is the OR of Code_Rel+Data_Rel
// if xTVAL is set to zero for some cause, then the corresponding bit in SET_REL_TVAL_MSK should be cleared

#ifndef SET_REL_TVAL_MSK
#define	SET_REL_TVAL_MSK ((1<<CAUSE_MISALIGNED_FETCH | 1<<CAUSE_FETCH_ACCESS	|			      1<<CAUSE_BREAKPOINT   | \
			   1<<CAUSE_MISALIGNED_LOAD  | 1<<CAUSE_LOAD_ACCESS	| 1<<CAUSE_MISALIGNED_STORE | 1<<CAUSE_STORE_ACCESS | \
			   1<<CAUSE_FETCH_PAGE_FAULT | 1<<CAUSE_LOAD_PAGE_FAULT |			      1<<CAUSE_STORE_PAGE_FAULT) \
			& 0xFFFFFFFF)
#endif

#ifndef CODE_REL_TVAL_MSK
#define CODE_REL_TVAL_MSK (1<<CAUSE_MISALIGNED_FETCH | 1<<CAUSE_FETCH_ACCESS				     | 1<<CAUSE_BREAKPOINT   | \
			   (1<<CAUSE_FETCH_PAGE_FAULT) &  0xFFFFFFFF)
#endif

#ifndef GOTO_M_OP
    #define GOTO_M_OP	csrr	t4, CSR_MSTATUS
#endif

//this is a valid global pte entry with all permissions. IF at the root entry, it forms an identity map.
#define RVTEST_PTE_IDENT_MAP  .fill   4096/REGWIDTH, REGWIDTH, (PTE_G | PTE_U | PTE_X | PTE_W | PTE_R | PTE_V)

//_ADDR_SZ_ is a global variable extracted from YAML; set a default if it isn't defined
// This should be the MAX(phy_addr_size, VADDR_SZ) from YAML, where VADDR_SZ is derived from SATP.mode at reset
#ifndef _ADDR_SZ_
  #if XLEN==64
    #define _ADDR_SZ_ 57
  #else
    #define _ADDR_SZ_ 32
  #endif
#endif

// define a bunch of XLEN dependent constants
#if   XLEN==32
    #define SREG sw
    #define LREG lw
    #define XLEN_WIDTH 5
    #define LREGWU lw
#elif XLEN==64
    #define SREG sd
    #define LREG ld
    #define XLEN_WIDTH 6
    #define LREGWU lwu
#else
    #define SREG sq
    #define LREG lq
    #define XLEN_WIDTH 7
    #define LREGWU lwu
#endif

#if FLEN==32
    #define FLREG flw
    #define FSREG fsw
    #define FREGWIDTH 4
#elif FLEN==64
    #define FLREG fld
    #define FSREG fsd
    #define FREGWIDTH 8
#else
    #define FLREG flq
    #define FSREG fsq
    #define FREGWIDTH 16
#endif

#if SIGALIGN==8
  #define CANARY \
      .dword 0x6F5CA309E7D4B281
#else
  #define CANARY \
      .word 0x6F5CA309 
#endif

//---------------------------mode encoding definitions-----------------------------
.set MMODE_SIG,3   /* FIXME why don't the #defines work if they're evaluated first?? */
.set VMODE_SIG,2
.set SMODE_SIG,1
  //#define MMODE_SIG 3
  //#define VMODE_SIG 2
  //#define SMODE_SIG 1
	/* these macros need to be defined because mode is uppercase in mode specific macros */
	/* note that vs mode uses smode return */

#define MPP_LSB	  11	//bit pos of LSB of the mstatus.MPP field
#define MPP_SMODE  (1<<MPP_LSB)
#define MPV_LSB	   7	// bit pos of prev vmod mstatush.MPV in either mstatush or upper half of mstatus

//#ifndef rvtest_sig_end			/* for old tests that didn't define this, AND aren't expecting traps */
//  #define rvtest_sig_end (mtrap_sigptr + 20*4*REGWIDTH)	/* ensure enough space to save trap signatures, 4*REGWIDTH is required for one trap signature, extrapolated to store 20 trap signatures */
//#endif
#define tramp_sz       ((XLEN + 3* NUM_SPECD_INTCAUSES + 17) * 4) /* 17 is #ops from Mend..Mentry */

//define a fixed offsets into the save area (should rename these all to _off instead of _addr)
#define tramp_sv_addr	 (-56-tramp_sz)
#define xsatp_sv_addr	 (-56)
#define mode_rtn_inst	 (-48)
#define trampend_addr	 (-40)
#define xedeleg_sv_addr	 (-32)
#define xtvec_new_addr	 (-24)
#define xtvec_sav_addr	 (-16)
#define xscr_save_addr	 ( -8)
#define trapreg_sv_addr	 ( -0)
#define trapreg_sv_sz	 (8*REGWIDTH)
#define RVTEST_ISA(_STR)	//empty macro used by framework??
#define code_area_sz	rvtest_code_end-rvtest_code_begin
#define data_area_sz	rvtest_data_end-rvtest_data_begin
//==============================================================================
// this section has  general test helper macros, required,  optional, or just useful
//==============================================================================

#define _ARG5(_1ST,_2ND, _3RD,_4TH,_5TH,...) _5TH
#define _ARG4(_1ST,_2ND, _3RD,_4TH,...) _4TH
#define _ARG3(_1ST,_2ND, _3RD, ...) _3RD
#define _ARG2(_1ST,_2ND, ...) _2ND
#define _ARG1(_1ST,...) _1ST
#define NARG(...) _ARG5(__VA_OPT__(__VA_ARGS__,)4,3,2,1,0)
#define RVTEST_CASE(_PNAME,_DSTR,...)

//-----------------------------------------------------------------------
//Fixed length la, li macros; # of ops is ADDR_SZ dependent, not data dependent
//-----------------------------------------------------------------------

#define COND_INCR(incr_reg, incrval, incrpos)					;\
  .if		   ((BIT(incrval,incrpos-1)+0x0FFF & (incrval>>(incrpos)))!=0)	;\
    addi reg, reg,   BIT(incrval,incrpos-1)+0x0FFF & (incrval>>(incrpos))	;\
 .endif

// this generates a constants using the standard addi or lui/addi sequences
// but also handles cases that are contiguous bit masks in any position,
// and also constants handled with the addi/lui/addi but are shifted left
	
/**** fixed length LI macro ****/
/**** fixed length LI macro ****/
#define LI(reg, imm)								                                        ;\
  .option push									                                            ;\
  .option norvc									                                            ;\
  .set immx, imm & MASK	/* trim to XLEN (noeffect on RV64) */		            ;\
  .set imm12, imm&0xFFF       /* Took 1st 12 bits */                       ;\
  .if (((immx>>11)&1)==1)                                                   ;\
      .set  imm12, imm12 - 4096                                             ;\
  .endif                                                                    ;\
  .set absimm, immx		/* convert to positive number to simplify code  */	  ;\
  .if((immx>>(XLEN-1)&1)==1)							                                  ;\
    .set absimm, ((immx)^(-1)) & MASK	/* Invert if negative */					            ;\
  .endif									                                                  ;\
  .if ((absimm>>12)==0)		/* are bits MSB:12 all the same?	     */	        ;\
	    li   reg, imm12			 /* yes, <= 12bit, will be simple li */	            ;\
  .elseif ((absimm>>31)==0)		/* are bits MSB:31 all the same?	     */	    ;\
      .if ((immx >> 11) == 0x01FFFFF)                                       ;\
        li reg, imm12                                                       ;\
      .else                                                                 ;\
        lui  reg, (((immx>>12)+((immx>>11)&1))&0xFFFFF) /* yes<= 32bit, use lui/addi pair  */	;\
        .if ((immx&0x0FFF)!=0)	/* but skip this if lower bits are zero	     */	              ;\
  	      addi reg, reg, imm12		                                          ;\
        .endif                                                              ;\
      .endif                                                                ;\
  .else		/* see if its a single field bitmask */				                    ;\
    .set pos,	    0								                                          ;\
    .set edge1,  -1	/* 1st "1" bit pos scanning r to l   */			            ;\
    .set edge2,  -1	/* 1st "0" bit pos scanning r to l   */			            ;\
    .set imme,  immx	/* imm or complement (if odd) */			                ;\
    .if (immx&1 == 1)								                                        ;\
      .set imme, (immx)^(-1) /* cvt to even, cvt back at end */	            ;\
    .endif									                                                ;\
/****************************************************************************/	;\
/*** find first 0->1, then 1->0 transition fm LSB->MSB give even operand ***/	  ;\
    .rept XLEN										                                          ;\
      .if     ((edge1==-1) && (((imme>>pos)&1)==1)) /*look for falling edge[pos]     */	;\
        .set  edge1,pos		/* found falling edge, don’t check for more  */	              ;\
      .elseif ((edge1>=0) && (edge2==-1) && (((imme>>pos)&1)==0))  /*look for rising edge[pos]     */	;\
	      .set  edge2, pos		/* found rising  edge, don’t check for more  */	                          ;\
      .endif										                                            ;\
      .set    pos,  pos+1		/* keep looking (even if already found	     */	;\
    .endr				/* assert: edge must be found since imm!= 0  */	            ;\
 /****************************************************************************/		;\
    .set shiftimm, (imm>>edge1)&0xFFFFFFFF                                            ;\
    .set imm12, shiftimm&0xFFF       /* Took 1st 12 bits of shifted immediate value */                       ;\
    .if (((shiftimm>>11)&1)==1)                                                   ;\
      .set  imm12, imm12 - 4096                                             ;\
    .endif                                                                    ;\
    .set absimm, (absimm>>edge1)&0xFFFFFFFF		/* convert to 32bit positive number to simplify code  */	  ;\
    .if (edge2==-1)		/* found just 1 edge, so its 111000 or 000111	*/	    ;\
      li	reg, -1									                                          ;\
      .if (immx == imme)								                                    ;\
	      slli	reg, reg, edge1			/* 111s followed by 000s mask	*/	        ;\
      .else										                                              ;\
	      srli	reg, reg, XLEN-edge1		/* 000s followed by 111s mask	*/	    ;\
      .endif										                                            ;\
    .elseif (imme == (1<<edge1))		/* single bit case		*/	              ;\
      li	reg, 1									                                          ;\
	    slli	reg, reg, edge1			/* create 0001000 mask		*/	              ;\
      .if (immx != imme)								                                    ;\
	      xori	reg, reg, -1			/* orig odd, cvt to 1110111 mask*/	        ;\
      .endif										                                            ;\
    .elseif (imme == ((1<<edge2) - (1<<edge1))) /* chk for multibit mask	*/	;\
      li	reg, -1									                                            ;\
	    srli	reg, reg, XLEN-(edge2-edge1) 	/* create 1s mask of right leng */	;\
	    slli	reg, reg, edge1			/* and put it into position     */	          ;\
      .if (immx != imme)								                                      ;\
	      xori	reg, reg, -1			/* orig odd, cvt to 1110111 mask*/	          ;\
      .endif										                                              ;\
    .elseif ((immx==imme) & ((absimm>>12)==0)) /* fits in 12b after shifting?*/	;\
	    li   reg, imm12			  /* yes, <= 12bit, will be simple li */	            ;\
	    slli reg, reg, edge1			/* add trailing zeros */		                            ;\
    .elseif ((immx==imme) & ((absimm>>31)==0)) /* fits in 32b after shifting?*/	;\
      .if ((absimm>>31)==0)			/* are bits MSB:31 all the same?     */	                ;\
        lui  reg, (((shiftimm>>12)+((shiftimm>>11)&1))&0xFFFFF) /* yes<= 32bit, use lui/addi pair  */	;\
          .if ((shiftimm&0x0FFF)!=0)	/* but skip this if lower bits are zero	     */	              ;\
  	        addi reg, reg, imm12		                                          ;\
          .endif                                                              ;\
	      slli reg, reg, edge1			/* add trailing zeros */		                            ;\
      .endif		     								                                                      ;\
    .else					/* give up and unroll */		                                            ;\
      .option pop									                                                        ;\
      .align UNROLLSZ									                                                    ;\
      .option push									                                                      ;\
      .option norvc									                                                      ;\
      .if(XLEN==32)									                                                      ;\
	      .warning "Should never get her for RV32"					                                ;\
      .endif										                                                          ;\
          li reg,imm									                                                    ;\
      .align UNROLLSZ									                                                    ;\
    .endif										                                                            ;\
  .endif										                                                              ;\
  .option pop


/**** fixed length LA macro ****/
#define LA(reg,val)	;\
	.option push	;\
	.option norvc	;\
	.align UNROLLSZ	;\
	la reg,val	;\
	.align UNROLLSZ	;\
	.option pop	;

/*****************************************************************/
/**** initialize regs, just to make sure you catch any errors ****/
/*****************************************************************/

/* init regs, to ensure you catch any errors */
#define RVTEST_INIT_GPRS			;\
     LI (x1,  (0xFEEDBEADFEEDBEAD & MASK))	;\
     LI (x2,  (0xFF76DF56FF76DF56 & MASK))	;\
     LI (x3,  (0x7FBB6FAB7FBB6FAB & MASK))	;\
     LI (x4,  (0xBFDDB7D5BFDDB7D5 & MASK))	;\
     LA (x5,  rvtest_code_begin)		;\
     LA (x6,  rvtest_data_begin)		;\
     LI (x7,  (0xB7FBB6FAB7FBB6FA & MASK))	;\
     LI (x8,  (0x5BFDDB7D5BFDDB7D & MASK))	;\
     LI (x9,  (0xADFEEDBEADFEEDBE & MASK))	;\
     LI (x10, (0x56FF76DF56FF76DF & MASK))	;\
     LI (x11, (0xAB7FBB6FAB7FBB6F & MASK))	;\
     LI (x12, (0xD5BFDDB7D5BFDDB7 & MASK))	;\
     LI (x13, (0xEADFEEDBEADFEEDB & MASK))	;\
     LI (x14, (0xF56FF76DF56FF76D & MASK))	;\
     LI (x15, (0xFAB7FBB6FAB7FBB6 & MASK))	;\
   #ifndef RVTEST_E				;\
     LI (x16, (0x7D5BFDDB7D5BFDDB & MASK))	;\
     LI (x17, (0xBEADFEEDBEADFEED & MASK))	;\
     LI (x18, (0xDF56FF76DF56FF76 & MASK))	;\
     LI (x19, (0x6FAB7FBB6FAB7FBB & MASK))	;\
     LI (x20, (0xB7D5BFDDB7D5BFDD & MASK))	;\
     LI (x21, (0xDBEADFEEDBEADFEE & MASK))	;\
     LI (x22, (0x6DF56FF76DF56FF7 & MASK))	;\
     LI (x23, (0xB6FAB7FBB6FAB7FB & MASK))	;\
     LI (x24, (0xDB7D5BFDDB7D5BFD & MASK))	;\
     LI (x25, (0xEDBEADFEEDBEADFE & MASK))	;\
     LI (x26, (0x76DF56FF76DF56FF & MASK))	;\
     LI (x27, (0xBB6FAB7FBB6FAB7F & MASK))	;\
     LI (x28, (0xDDB7D5BFDDB7D5BF & MASK))	;\
     LI (x29, (0xEEDBEADFEEDBEADF & MASK))	;\
     LI (x30, (0xF76DF56FF76DF56F & MASK))	;\
     LI (x31, (0xFBB6FAB7FBB6FAB7 & MASK))	;\
   #endif

/******************************************************************************/
/**** this is a helper macro that conditionally instantiates the macros	   ****/
/**** PROLOG/HANDLER/EPILOG/SAVEAREA depending on test type & mode support ****/
/******************************************************************************/
.macro INSTANTIATE_MODE_MACRO MACRO_NAME
 #ifdef rvtest_mtrap_routine
  \MACRO_NAME M		// actual m-mode prolog/epilog/handler code

	#ifdef rvtest_strap_routine
      \MACRO_NAME S	// actual s-mode prolog/epilog/handler code

	#ifdef rvtest_vtrap_routine
	  \MACRO_NAME V // actual v-mode prolog/epilog/handler code
	#endif
    #endif
 #endif
.endm

/******************************************************************************/
/**** this is a helper macro defining int macro if the macro if undefined  ****/
/**** It builds the macro name from arguments prefix,  mode, and type	   ****/
/**** The prefix is only SET or CLEAR here, preceded by RVMODEL.	   ****/
/**** The types are the 3 types of standard interrupts: SW, EXT, & TIMER   ****/
/******************************************************************************/

.macro INIT_MODE_MACRO MACRO_NAME
 #ifndef   MACRO_NAME
 //#warning "MACRO_NAME is not defined by target. Executing this will end test"
 .macro	   \MACRO_NAME
     j cleanup_epilogs
 .endm
 #endif
.endm

/******************************************************************************/
/**** this is a helper macro that conditionally instantiates default	   ****/
/**** interrupt set/clear depending on type of test & mode support	   ****/
/******************************************************************************/

 .macro INSTANTIATE_INT_MACRO INT_MACRO_NAME
    #ifndef INT_MACRO_NAME	// actual m-mode handler code
      // #warning: "INT_MACRO_NAME  is not defined by target. Executing this will end test"
       .macro \INT_MACRO_NAME
	 j cleanup_epilogs
       .endm
    #endif
 .endm

/******************************************************************************/
/**** this is a helper macro that creates CSR aliases so code that	   ****/
/**** accesses CSRs when V=1 in different modes can share the code	   ****/
/******************************************************************************/

 .macro	XCSR_RENAME __MODE__	// enable CSR names to be parameterized.
    .if ((\__MODE__\() == S) | (\__MODE__\() == V))
      .set CSR_XSTATUS,	CSR_SSTATUS
      .set CSR_XEDELEG,	CSR_SEDELEG
      .set CSR_XIE,	CSR_SIE
      .set CSR_XIP,	CSR_SIP
      .set CSR_XCAUSE,	CSR_SCAUSE
      .set CSR_XEPC,	CSR_SEPC
      .set CSR_XSATP,	CSR_SATP
      .set CSR_XSCRATCH,CSR_SSCRATCH
      .set CSR_XTVAL,	CSR_STVAL
      .set CSR_XTVEC,	CSR_STVEC
    .else
      .set CSR_XSTATUS, CSR_MSTATUS
      .set CSR_XEDELEG, CSR_MEDELEG
      .set CSR_XIE,	CSR_MIE
      .set CSR_XIP,	CSR_MIP
      .set CSR_XCAUSE,	CSR_MCAUSE
      .set CSR_XEPC,	CSR_MEPC
      .set CSR_XSATP,	CSR_SATP
      .set CSR_XSCRATCH,CSR_MSCRATCH
      .set CSR_XTVAL,	CSR_MTVAL
      .set CSR_XTVEC,	CSR_MTVEC
    .endif
.endm

/******************************************************************************/
/**** this is a helper macro that creates CSR aliases so code that	   ****/
/**** accesses CSRs when V=1in different modes can share the code	   ****/
/******************************************************************************/
 .macro	XCSR_VRENAME __MODE__		// enable CSR names to be parameterized, with H/V,S aliasing
  .if (\__MODE__\() == S)
      .set CSR_XSTATUS, CSR_SSTATUS
      .set CSR_XEDELEG, CSR_SEDELEG
      .set CSR_XIE,	CSR_SIE
      .set CSR_XIP,	CSR_SIP
      .set CSR_XCAUSE,	CSR_SCAUSE
      .set CSR_XEPC,	CSR_SEPC
      .set CSR_XSATP,	CSR_SATP
      .set CSR_XSCRATCH,CSR_SSCRATCH
      .set CSR_XTVAL,	CSR_STVAL
      .set CSR_XTVEC,	CSR_STVEC
  .elseif (\__MODE__\() == V)
      .set CSR_XSTATUS, CSR_HSTATUS
      .set CSR_XEDELEG, CSR_HEDELEG
      .set CSR_XIE,	CSR_HIE
      .set CSR_XIP,	CSR_HIP
      .set CSR_XCAUSE,	CSR_VSCAUSE
      .set CSR_XEPC,	CSR_VSEPC
      .set CSR_XSATP,	CSR_VSATP
      .set CSR_XSCRATCH,CSR_VSSCRATCH
      .set CSR_XTVAL,	CSR_VSTVAL
      .set CSR_XTVEC,	CSR_VSTVEC
     .else
      .set CSR_XSTATUS, CSR_MSTATUS
      .set CSR_XEDELEG, CSR_MEDELEG
      .set CSR_XIE,	CSR_MIE
      .set CSR_XIP,	CSR_MIP
      .set CSR_XCAUSE,	CSR_MCAUSE
      .set CSR_XEPC,	CSR_MEPC
      .set CSR_XSATP,	CSR_SATP
      .set CSR_XSCRATCH,CSR_MSCRATCH
      .set CSR_XTVAL,	CSR_MTVAL
      .set CSR_XTVEC,	CSR_MTVEC
    .endif
.endm

////////////////////////////////////////////////////////////////////////////////////////
//**** This is a helper macro that saves GPRs. Normally used only inside CODE_END ****//
//**** Note: this needs a temp scratch register, & there isn't anything that will ****//
//**** will work, so we always trash some register, determined by macro param	  ****//
//**** NOTE: Only be use for debug! Xregs containing addresses won't be relocated ****//
////////////////////////////////////////////////////////////////////////////////////////

.macro RVTEST_SAVE_GPRS BASEREG REG_SV_ADDR	// optionally save GPRs
.option push
.option norvc
.set offset,0
  LA(	  \BASEREG, \REG_SV_ADDR)		//this destroys basereg, but saves rest
  SIGUPD( \BASEREG, x1)
  SIGUPD( \BASEREG, x2)
  SIGUPD( \BASEREG, x3)
  SIGUPD( \BASEREG, x4)
  SIGUPD( \BASEREG, x5)
  SIGUPD( \BASEREG, x6)
  SIGUPD( \BASEREG, x7)
  SIGUPD( \BASEREG, x8)
  SIGUPD( \BASEREG, x9)
  SIGUPD( \BASEREG, x10)
  SIGUPD( \BASEREG, x11)
  SIGUPD( \BASEREG, x12)
  SIGUPD( \BASEREG, x13)
  SIGUPD( \BASEREG, x14)
  SIGUPD( \BASEREG, x15)
#if (! __RV32E__)
  SIGUPD( \BASEREG, x16)
  SIGUPD( \BASEREG, x17)
  SIGUPD( \BASEREG, x18)
  SIGUPD( \BASEREG, x19)
  SIGUPD( \BASEREG, x20)
  SIGUPD( \BASEREG, x21)
  SIGUPD( \BASEREG, x22)
  SIGUPD( \BASEREG, x23)
  SIGUPD( \BASEREG, x24)
  SIGUPD( \BASEREG, x25)
  SIGUPD( \BASEREG, x26)
  SIGUPD( \BASEREG, x27)
  SIGUPD( \BASEREG, x28)
  SIGUPD( \BASEREG, x29)
  SIGUPD( \BASEREG, x30)
  SIGUPD( \BASEREG, x31)
#endif
.option pop
.endm

/********************* REQUIRED FOR NEW TESTS *************************/
/**** new macro encapsulating RVMODEL_DATA_BEGIN (signature area)  ****/
/**** defining rvtest_sig_begin: label to enabling direct stores   ****/
/**** into the signature area to be properly relocated		   ****/
/**********************************************************************/
#define	RVTEST_SIG_BEGIN						;\
.global rvtest_sig_begin	/* defines beginning of signature area */ ;\
									;\
    RVMODEL_DATA_BEGIN		/* model specific stuff		       */ ;\

/* DFLT_TRAP_SIG is defined to simplify the tests. 
Tests that don't define mtrap_routine don't need 64 words of trap signature; 
they only need 3 at most, because any unexpected traps will write 4 words 
and overwrite the canary and stop the test after the 4rd word is written */
	
#define DFLT_TRAP_SIG		;\
#ifndef rvtest_mtrap_routine	;\
   tsig_begin_canary:	CANARY;	;\
   mtrap_sigptr:	.fill 3*(XLEN/32),4,0xdeadbeef	;\
   tsig_end_canary:    CANARY;		;\
#endif		
// tests allocate normal signature space here, then define
// the mtrap_sigptr: label to separate normal and trap
// signature space, then allocate trap signature space

/********************* REQUIRED FOR NEW TESTS *************************/
/**** new macro encapsulating RVMODEL_DATA_END (signature area)	   ****/
/**** defining rvtest_sig_end: label to enabling direct stores	   ****/
/**** into the signature area to be properLY relocated		   ****/
/**********************************************************************/
#define	RVTEST_SIG_END							;\
.global rvtest_sig_end		/* defines end of signature area       */ ;\
DFLT_TRAP_SIG							;\
	#ifdef rvtest_gpr_save						;\
		gpr_save:  .fill 32*(XLEN/32),4,0xdeadbeef		;\
	#endif								;\
sig_end_canary: CANARY;	/* add one extra word of guardband     */	;\
rvtest_sig_end:								;\
RVMODEL_DATA_END	/* model specific stuff */

 //#define rvtest_sig_sz (rvtest_sig_end - rvtest_sig_begin) not currently used
/***************************************************************************************/
/**** At end of test, this code is entered. It diverts the Mmode trampoline to code ****/
/**** that follows this, executes an op illegal in any non-Mmode, then restores the ****/
/**** trampoline. This either falls through if already in Mmode, or traps to mmode  ****/
/**** because the prolog cleared all edeleg CSRs. This code makes no assumptions    ****/
/**** about the current operating mode. and assumes that xedeleg[illegal_op]==0 to  ****/
/**** prevent an infinite delegation loop. This will overwrite t1..t6		    ****/
/**** NOTE: that any test that sets xedeleg[illegal_op] must replace  mode_rtn_inst ****/
/**** with an op that causes a different exception cause that isn't delegated.	    ****/
/**** NOTE: this will overwrite non-Mmode trap status CSRs			    ****/
/****FIXME - check that SATP and VSATP point to the identity map page table	    ****/
/***************************************************************************************/
//  #define TESTCODE_ONLY_TVAL_ILLOP	//FIXME comment this out when Sail fixes illop

.macro	RVTEST_GOTO_MMODE
#ifdef	rvtest_mtrap_routine		/**** FIXME this can be empty if no Umode ****/
.option push
.option norvc

#ifdef rvtest_strap_routine		/* assume save are is in order M/S/V  */
    LA(	 t1, Mtrapreg_sv)		/* addr of next higher priv mode save area   */
    lw	 t2,	 mode_rtn_inst(t1)	/* get replacement op: jr 8(t2)		     */

    LREG t3,	xtvec_new_addr(t1)	/* get mtvec value from save area	     */
    lw	 t5,  0(t3)			/* get tramp entry br inst, pted to by mtvec */
    sw	 t2,  0(t3)			/* overwrite it w/ replacement op	     */

  #ifdef rvtest_vtrap_routine
      LREG t4,	xtvec_new_addr+Msv_area_sz(t1)	/* get stvec value from save area    */
      lw   t6,	0(t4)			/* get tramp entry br inst, pted to by stvec */
      sw   t2,	0(t4)			/* overwrite it w/ replacement op	     */
  #endif
#endif
  RVMODEL_FENCEI			/* orig tramp entries now in t5,t6 for S,V   */

/****FIXME**** this must relocate the PC to the equivalent Mmode physical address    */
  auipc	t2, 0				/* set rtnaddr base= GOTO_M_OP-4 (reuse t2)  */

// Note that if illegal op trap is delegated , this may infinite loop
// The solution is either for test to disable delegation, or to
// redefine the GOTO_M_OP to be an op that will trap vertically
  GOTO_M_OP				/* illegal op if < Mmode , else falls through*/
					/* if delegated to S/ V, it rtns here&re-trap*/
#ifdef rvtest_strap_routine		/* fallthru to here when csrr  succeeds	     */
    sw	 t5, 0(t3)			/* restore old	smode trampoline head inst   */
  #ifdef rvtest_vtrap_routine
       sw   t6, 0(t4)			/* restore old vsmode trampoline head inst   */
  #endif
#endif
  RVMODEL_FENCEI			/* ensure tramp ifetches get new code	     */
					/* if in a mode w/ MMU enabled, req VA==PA   */
.option pop
#endif					/* not implemented if no trap		     */
.endm


/**** This is a helper macro that causes harts to transition from    ****/
/**** M-mode to a lower priv mode. Legal params are VS,HS,VU,HU,S,U. ****/
/**** The H,U variations leave V unchanged. This uses t4 only.	     ****/
/**** NOTE: this MUST be executed in M-mode. Precede with GOTO_MMODE ****/
/**** FIXME - SATP & VSATP must point to the identity map page table ****/

#define MPV_LSB	   7	// bit pos of prev vmod mstatush.MPV in either mstatush or upper half of mstatus
#define HSmode	0x9
#define HUmode	0x8
#define	VUmode	0x4
#define	VSmode	0x5
#define Smode	0x1
#define Umode	0x0

.macro RVTEST_GOTO_LOWER_MODE LMODE
.option push
.option norvc

	// first, clear MSTATUS.PP (and .MPV if it will be changed_
	// then set them to the values that represent the lower mode
.if (XLEN==32)
   .if	   ((\LMODE\()==VUmode) | (\LMODE\()==VSmode))
     csrsi CSR_MSTATUS, MSTATUS_MPV	/* set V			*/
   .elseif ((\LMODE\()==HUmode) | (\LMODE\()==HSmode))
     csrci CSR_MSTATUS, MSTATUS_MPV	/* clr V			*/
   .endif				/* lv  V unchged for S or U	*/

  LI(	 t4, MSTATUS_MPP)
  csrc	 CSR_MSTATUS, t4		/* clr PP always		*/

  .if	 ((\LMODE\()==VSmode) | (\LMODE\()==HSmode) | (\LMODE\()==Smode))
    LI(	 t4, MPP_SMODE)			/* val for Smode		*/
    csrs CSR_MSTATUS, t4		/* set in PP			*/
  .endif
	// do the same if XLEN=64
.else				/* XLEN=64, maybe 128? FIXME for 128	*/
  .if ((\LMODE\()==Smode) | (\LMODE\()==Umode)) /* lv V unchanged here	*/
    LI(	 t4,  MSTATUS_MPP)	/* but always clear PP			*/
  .else
    LI(	 t4, (MSTATUS_MPP | MSTATUS_MPV))	/* clr V and P		*/
  .endif
  csrc	 CSR_MSTATUS, t4	/* clr PP to umode & maybe Vmode	*/

  .if (!((\LMODE\()==HUmode) | (\LMODE\()==Umode)))  /* lv pp unchged, v=0 or unchged	*/
    .if	     (\LMODE\()==VSmode)
      LI(  t4, (MPP_SMODE | MSTATUS_MPV)) /* val for pp & v		*/
  .elseif ((\LMODE\()==HSmode) | (\LMODE\()==Smode))
      LI(  t4, (MPP_SMODE))	/* val for pp only			*/
  .else			/* only VU left; set MPV only		*/
      li   t4, 1		/* optimize for single bit		*/
      slli t4, t4, 32+MPV_LSB	/* val for v only			*/
    .endif
    csrs CSR_MSTATUS, t4	/* set correct mode and Vbit		*/
  .endif
.endif
	/**** mstatus MPV and PP now set up to desired mode    ****/
	/**** set MEPC to mret+4; requires an identity mapping ****/
  auipc	t4, 0
  addi	t4, t4, 16
  csrrw	t4, CSR_MEPC, t4	/* set rtn addr to mret+4		*/
  mret				/* transition to desired mode		*/
.option pop
.endm

//==============================================================================
// Helper macro to set defaults for undefined interrupt set/clear
// macros. This is used to populated the interrupt vector table
//==============================================================================

  #ifndef RVMODEL_SET_MSW_INT
       // #warning: "RVMODEL_SET_MSW_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_SET_MSW_INT
	 j cleanup_epilogs
       .endm
  #endif
  #ifndef RVMODEL_CLEAR_MSW_INT
       // #warning: "RVMODEL_CLEAR_MSW_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_CLEAR_MSW_INT
	 j cleanup_epilogs
       .endm
  #endif
  #ifndef RVMODEL_CLEAR_MTIMER_INT
       // #warning: "RVMODEL_CLEAR_MTIMER_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_CLEAR_MTIMER_INT
	 j cleanup_epilogs
       .endm
  #endif
  #ifndef RVMODEL_CLEAR_MEXT_INT
       // #warning: "RVMODEL_CLEAR_MEXT_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_CLEAR_MEXT_INT
	 j cleanup_epilogs
       .endm
  #endif

  #ifndef RVMODEL_SET_SSW_INT
       // #warning: "RVMODEL_SET_SSW_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_SET_SSW_INT
	 j cleanup_epilogs
       .endm
  #endif
  #ifndef RVMODEL_CLEAR_SSW_INT
       // #warning: "RVMODEL_CLEAR_SSW_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_CLEAR_SSW_INT
	 j cleanup_epilogs
       .endm
  #endif
  #ifndef RVMODEL_CLEAR_STIMER_INT
       // #warning: "RVMODEL_CLEAR_STIMER_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_CLEAR_STIMER_INT
	 j cleanup_epilogs
       .endm
  #endif
  #ifndef RVMODEL_CLEAR_SEXT_INT
       // #warning: "RVMODEL_CLEAR_SEXT_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_CLEAR_SEXT_INT
	 j cleanup_epilogs
       .endm
  #endif

    #ifndef RVMODEL_SET_VSW_INT
       // #warning: "RVMODEL_SET_VSW_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_SET_VSW_INT
	 j cleanup_epilogs
       .endm
  #endif
  #ifndef RVMODEL_CLEAR_VSW_INT
       // #warning: "RVMODEL_CLEAR_VSW_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_CLEAR_VSW_INT
	 j cleanup_epilogs
       .endm
  #endif
  #ifndef RVMODEL_CLEAR_VTIMER_INT
       // #warning: "RVMODEL_CLEAR_VTIMER_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_CLEAR_VTIMER_INT
	 j cleanup_epilogs
       .endm
  #endif
  #ifndef RVMODEL_CLEAR_VEXT_INT
       // #warning: "RVMODEL_CLEAR_VEXT_INT  is not defined by target. Executing this will end test"
       .macro RVMODEL_CLEAR_VEXT_INT
	 j cleanup_epilogs
       .endm
  #endif

//==============================================================================
// This section defines macros used by these required macros:
// RVTEST_TRAP_PROLOG, RVTEST_TRAP_HANDLER, RVTEST_TRAP_EPILOG
// These are macros instead of inline because they need to be replicated per mode
// These are passed the privmode as an argument to properly rename labels
// The helper INSTANTIATE_MODE_MACRO actually handles the replication
//==============================================================================

.macro	RVTEST_TRAP_PROLOG __MODE__
.option push
.option norvc
  /******************************************************************************/
  /**** this is a mode-configured version of the prolog, which either saves and */
  /**** replaces xtvec, or saves and replaces the code located at xtvec if it	*/
  /**** it xtvec isn't arbitrarily writable. If not writable, restore & exit	*/
  /******************************************************************************/

  /******************************************************************************/
  /****			Prolog, to be run before any tests		     ****/
  /****	      #include 1 copy of this per mode in rvmodel_boot code?	     ****/
  /**** -------------------------------------------------------------------  ****/
  /**** if xTVEC isn't completely RW, then we need to change the code at its ****/
  /**** target. The entire trap trampoline and mtrap handler replaces the    ****/
  /**** area pointed to by mtvec, after saving its original contents first.  ****/
  /**** If it isn't possible to fully write that area, restore and fail.     ****/
  /******************************************************************************/

  // RVTEST_TRAP_PROLOG trap_handler_prolog; enter with t1..t6 available; define specific handler
  // sp will immediately point to the current mode's save area and must not be touched
  //NOTE: this is run in M-mode, so can't use aliased S,V CSR names

.global \__MODE__\()trampoline
//.global mtrap_sigptr

	XCSR_VRENAME \__MODE__		//retarget XCSR names to this modes CSRs, separate V/S copies

	LA(	t1, \__MODE__\()trapreg_sv)	// get pointer to save area
//----------------------------------------------------------------------
init_\__MODE__\()scratch:
	csrrw	t3, CSR_XSCRATCH, t1	// swap xscratch with save area ptr (will be used by handler)
	SREG	t3, xscr_save_addr(t1)	// save old mscratch in xscratch_save
//----------------------------------------------------------------------
init_\__MODE__\()edeleg:
	li	t2, 0			// save and clear edeleg so we can exit to Mmode
.if	(\__MODE__\() == V)
	csrrw	t2, CSR_VEDELEG, t2	// special case: VS EDELEG available from Vmode
.elseif	(\__MODE__\() == M)
#ifdef rvtest_strap_routine
	csrrw	t2, CSR_XEDELEG, t2	// this handles M  mode save, but only if Smode exists
#endif
.else
//FIXME: if N-extension or anything like it is implemented, uncomment the following
//	csrrw	t2, CSR_XEDELEG, t2	// this handles S mode
.endif
	SREG	t2, xedeleg_sv_addr(t1)	// now do the save
//----------------------------------------------------------------------
init_\__MODE__\()satp:
.if (\__MODE__\() != M)			// if S or VS mode
	LA(	t4, rvtest_\__MODE__\()root_pg_tbl)	// rplc xsatp w/ identity-mapped pg table **FIXME: fixed offset frm trapreg_sv?
	csrrw	t4, CSR_XSATP, t4
	SREG	t4, xsatp_sv_addr(t1)
.endif
//----------------------------------------------------------------------
init_\__MODE__\()tvec:
	LA(	t4, \__MODE__\()trampoline)	//this is a code-relative pointer
	csrr	t3, CSR_XTVEC
	SREG	t3, xtvec_sav_addr(t1)	// save orig mtvec+mode in tvec_save
	andi	t2, t3, 3		// merge .mode & tramp ptr and store to both XTVEC, tvec_new
	or	t2, t4, t2
	SREG	t2, xtvec_new_addr(t1)
	csrw	CSR_XTVEC, t2		// write xtvec with trap_trampoline+mode, so trap will go to the trampoline

	csrr	t5, CSR_XTVEC		// now read new_mtval back & make sure we could write it
#ifndef HANDLER_TESTCODE_ONLY
	beq	t5, t2, rvtest_\__MODE__\()prolog_done // if mtvec==trap_trampoline, mtvec is writable, continue
#endif
	csrw	CSR_XTVEC, t3		// xTVEC not completely writable, restore old value & exit if uninitialized
	beqz	t3, abort\__MODE__\()test
	SREG	t3, xtvec_new_addr(t1)	// else update tvect_new with orig mtvec

  /*****************************************************************/
  /**** fixed mtvec, can't move it so move trampoline instead	****/
  /**** t1=tramp sv, t2=orig tvec, t3=sv end, t4=tramp		****/
  /*****************************************************************/

init_\__MODE__\()tramp:	/**** copy trampoline at mtvec tgt; t4->t2->t1	t3=end of save ****/
	andi	t2, t3, -4			// calc bgn of orig tramp area by rmving mode bits
	addi	t3, t2, tramp_sz		// calc end of orig tramp area (+4)
	addi	t1, t1, tramp_sv_addr		// calc bgn of tramp save area
//----------------------------------------------------------------------
	overwt_tt_\__MODE__\()loop:		// now build new tramp table w/ local offsets
	lw	t6, 0(t2)			//  move original mtvec target to save area
	sw	t6, 0(t1)
	lw	t5, 0(t4)			//  move traphandler trampoline into orig mtvec target
	sw	t5, 0(t2)
	lw	t6, 0(t2)			// rd it back to make sure it was written
	bne	t6, t5, endcopy_\__MODE__\()tramp // table isn't fully writable, restore and give up
#ifdef HANDLER_TESTCODE_ONLY
	csrr	t5, CSR_XSCRATCH		// load trapreg_sv from scratch
	addi	t5, t5,256			// calculate some offset into the save area
	bgt	t5, t1, endcopy_\__MODE__\()tramp // and pretend if couldnt be written
#endif
	addi	t2, t2, 4			// next tvec  inst. index
	addi	t1, t1, 4			// next save  inst. index
	addi	t4, t4, 4			// next tramp inst. index
	bne	t3, t2, overwt_tt_\__MODE__\()loop	// haven't reached end of save area,  loop
//----------------------------------------------------------------------
  endcopy_\__MODE__\()tramp:			// vector table not writeable, restore
	RVMODEL_FENCEI				// make sure ifetches get new code
	csrr	t1, CSR_XSCRATCH		// reload trapreg_sv from scratch
	sw	t2, trampend_addr(t1)		// save copy progress
	beq	t3,t2, rvtest_\__MODE__\()prolog_done //full loop, don't exit
abort\__MODE__\()test:
	LA(	t6, exit_\__MODE__\()cleanup)	// trampoline rplc failure **FIXME:  precalc& put into savearea?
	jalr   x0, t6				// this branch may be too far away, so longjmp

rvtest_\__MODE__\()prolog_done:

.option pop
.endm
/*******************************************************************************/
/***************		 end of prolog macro		    ************/
/*******************************************************************************/

.macro RVTEST_TRAP_HANDLER __MODE__
.option push
.option rvc	// temporarily allow compress to allow c.nop alignment
.align 6	// ensure that a trampoline is on a typical cacheline boundary, just in case
.option pop

  /**********************************************************************/
  /**** This is the entry point for all x-modetraps, vectored or not.****/
  /**** xtvec should either point here, or trampoline code does and  ****/
  /**** trampoline code was copied to whereever xtvec pointed to.    ****/
  /**** At entry, xscratch will contain a pointer to a scratch area. ****/
  /**** This is an array of branches at 4B intevals that spreads out ****/
  /**** to an array of 12B xhandler stubs for specd int causes, and  ****/
  /**** to a return for anything above that (which causes a mismatch)****/
  /**********************************************************************/

  XCSR_RENAME \__MODE__			//retarget XCSR names to this modes CSRs

.global \__MODE__\()trampoline			// define the label and make it available
.global common_\__MODE__\()entry
.option push
.option norvc

\__MODE__\()trampoline:
   .set	 value, 0
  .rept NUM_SPECD_INTCAUSES			// located at each possible int vectors
	j    trap_\__MODE__\()handler+ value	// offset < +/- 1MB
	.set value, value + 12			// length of xhandler trampoline spreader code
  .endr

  .rept XLEN-NUM_SPECD_INTCAUSES		// fill at each impossible entry
	j rvtest_\__MODE__\()endtest		// end test if this happens
  .endr

  /*********************************************************************/
  /**** this is spreader stub array; it saves enough info (sp &	    ****/
  /**** vec-offset) to enable branch to common routine to save rest ****/
  /*********************************************************************/
  /**** !!CSR_xSCRATCH is preloaded w/ xtrapreg_sv in init_xscratch:****/

 trap_\__MODE__\()handler:			// on exit sp swapped w/ save ptr, t6 is vector addr
  .rept NUM_SPECD_INTCAUSES
	csrrw	sp, CSR_XSCRATCH, sp		// save sp, replace w/trapreg_sv regtmp save ptr
	SREG	t6, 6*REGWIDTH(sp)		// save t6 in temp save area offset 6
	jal	t6, common_\__MODE__\()handler	// jmp to common code, saving vector in t6
  .endr

 rvtest_\__MODE__\()endtest:			// target may be too far away, so longjmp
	LA     (t1, rvtest_\__MODE__\()end)	// FIXME: must be identity mapped if its a VA
	jalr	x0, t1
  /*********************************************************************/
  /**** common code for all ints & exceptions, will fork to handle  ****/
  /**** each separately. The common handler first stores trap mode+ ****/
  /**** vector, & mcause signatures. Most traps have 4wd sigs, but  ****/
  /**** sw and timer ints only store 3 of the 4, & some hypervisor  ****/
  /**** traps will set store 6 ops				    ****/
  /**** sig offset Exception	ExtInt	     SWInt	  TimerInt  ****/
  /****		0: <---------------------  Vect+mode  ---------->   ****/
  /****		4: <----------------------  xcause ------------->   ****/
  /****		8: xepc	     <-------------  xip  -------------->   ****/
  /****	       12: tval		IntID	<---- x ---------------->   ****/
  /****	       16: tval2/x * <--------------  x ---------------->   ****/
  /****	       20: tinst/x * <--------------  x ---------------->   ****/
  /****	 *  only loaded for Mmode traps when hypervisor implemented ****/
  /*********************************************************************/
  /*   in general, CSRs loaded in t2, addresses into t3		       */

	//If we can distinguish between HS and S mode, we can share S and V code.
	//except for prolog code which needs to initialize CSRs, and the save area
	//To do this, we need to read one of the CSRs (e.g. xSCRATCH) and compare
	//it to either Strapreg_sv or Vtrapreg_sv to determine which it is.

common_\__MODE__\()handler:			// enter with vector addr in t6 (orig t6 is at offset 6*REGWIDTH)
	SREG	t5, 5*REGWIDTH(sp)		// x30	save remaining regs, starting with t5
	csrrw	t5, CSR_XSCRATCH, sp		// restore ptr to reg sv area, and get old sp
	SREG	t5, 7*REGWIDTH(sp)		// save old sp
	auipc	t5, 0
	addi	t5, t5, 12			// quick calculation of common Xentry: label
	jr	t5				// needed if trampoline gets moved elsewhere, else it's effectively a noop

common_\__MODE__\()entry:
	SREG	t4, 4*REGWIDTH(sp)		//x29
	SREG	t3, 3*REGWIDTH(sp)		//x28
	SREG	t2, 2*REGWIDTH(sp)		//x7
	SREG	t1, 1*REGWIDTH(sp)		//x6  save other temporaries

//------pre-update trap_sig pointer so handlers can themselves trap-----
\__MODE__\()trapsig_ptr_upd:
	li	t2, 4*REGWIDTH			// standard entry length
	csrr	t5, CSR_XCAUSE
	bgez	t5, \__MODE__\()xcpt_sig_sv	// Keep std length if this is an exception for now (MSB==0)
\__MODE__\()int_sig_sv:
	slli	t3, t5, 1			// remove MSB, cause<<1
	addi	t3, t3, -(IRQ_M_TIMER)<<1	// is cause (w/o MSB) an extint or larger? ( (cause<<1) > (8<<1) )?
	bgez	t3, \__MODE__\()trap_sig_sv	// yes, keep std length
	li	t2, 3*REGWIDTH			// no,	its a timer or swint, overrride preinc to 3*regsz
	j	\__MODE__\()trap_sig_sv
\__MODE__\()xcpt_sig_sv:
.if (\__MODE__\() == M)				// exception case, don't adjust if hypervisor mode disabled
	csrr	t1, CSR_MISA
	slli	t1, t1, XLEN-8			// shift H bit into msb
	bgez	t1, \__MODE__\()trap_sig_sv	// no hypervisor mode, keep std width
	li	t2, 6*REGWIDTH			// Hmode implemented &	Mmode trap, override prein to be 6*regsz
.endif

\__MODE__\()trap_sig_sv:
	LA(	t3, rvtest_trap_sig)		// this is where curr trap signature pointer is stored
//------this should be atomic-------------------------------------
	LREG	t1, 0(t3)			// get the trap signature pointer (initialized to mtrap_sigptr)
	add	t4, t1, t2			// pre-inc pointer so nested traps don't corrupt signature queue
	SREG	t4, 0(t3)			// and save new value (old value is still in t1)
//------end atomic------------------------------------------------

	LREG	t3, xtvec_new_addr(sp)		// get pointer to actual tramp table
//----------------------------------------------------------------
sv_\__MODE__\()vect:
	sub	t6, t6, t3			// cvt vec-addr in t6 to offset fm top of tramptable **FIXME: breaks if tramp crosses pg && MMU enabled
	slli	t6, t6, 3			// make room for 3 bits; MSBs aren't useful **FIXME: won't work for SV64!)
	or	t6, t6, t2			// insert entry size into bits 5:2
	addi	t6, t6, \__MODE__\()MODE_SIG	// insert mode# into 1:0
	SREG	t6, 0*REGWIDTH(t1)		// save 1st sig value, (vec-offset, entrysz, trapmode)
//----------------------------------------------------------------
sv_\__MODE__\()cause:
	SREG	t5, 1*REGWIDTH(t1)		// save 2nd sig value, (mcause)
//----------------------------------------------------------------
	bltz	t5, common_\__MODE__\()int_handler // split off if this is an interrupt

  /********************************************************************/
  /**** This is the exceptions specific code, storing relative mepc****/
  /**** & relative tval signatures. tval is relocated by code or   ****/
  /**** data start, or 0 depending on mcause. mepc signature value ****/
  /**** is relocated by code start, and restored bumped by 2..6B   ****/
  /****	 depending on op alignment so trapped op isn't re-executed ****/
  /********************************************************************/
common_\__MODE__\()excpt_handler:
	csrr	t2, CSR_XEPC
sv_\__MODE__\()epc:
	LA(t3, rvtest_code_end)		      // Fetch the address of rvtest_code_end to compare against mepc
  bge t2, t3, epc_data_\__MODE__\()adj	// If mepc > rvtest_code_end, (outside the code memory) 
					// then trap came from data area and epc_data_\__MODE__\()adj will adjust to offset wrt to rvtest_data_begin
  LA(	t3, rvtest_code_begin)		  // Fetch the address of rvtest_code_begin to compare against mepc.
	blt t2, t3, epc_data_\__MODE__\()adj  // Check if mepc < rvtest_code_begin, (trap came from outside the code memory), 
					// then ask epc_data_\__MODE__\()adj to adjust the offset wrt data memory starting address
  sub t4, t2, t3			// if mepc < rvtest_code_end, then offset will be adjusted wrt rvtest_code_begin
	SREG	t4, 2*REGWIDTH(t1)		    // save 3rd sig value, (normalized rel mepc) into trap signature area
  j  adj_\__MODE__\()epc

epc_data_\__MODE__\()adj:
  LA(	t4, rvtest_data_begin)		// Fetch rvtest_data_begin to adjust mepc offset against it
	sub	t4, t2, t4			// Offset adjustment
	SREG	t4, 2*REGWIDTH(t1)		  // save 3rd sig value, (normalized rel mepc) into trap signature area

adj_\__MODE__\()epc:			// adj mepc so there is at least 4B of padding after op
	andi	t6, t2, -0x4		// adjust mepc to prev 4B alignment (if 2B aligned)
	addi	t6, t6,  0x8		// adjust mepc so it skips past op, has padding & 4B aligned
	csrw	CSR_XEPC, t6		// restore adjusted value, w/ 2,4 or 6B of padding

  /****WARNING needs updating when insts>32b are ratified, only 4 or 6B of padding; for 64b insts,  2B or 4B of padding	  ****/

  /******************************************************************************/
  /* Calculate relative mtval if it’s an addr (by code_begin or data_begin amt) */
  /* Special case if its in the signature region as well.			*/
  /* Model-defined mask values (indexed by xcause value) select which to use	*/
  /* Enter with rvtest_code_begin (which is start of actual test) in t3		*/
  /* if illegal op, load opcode from mtval (if !=0) or from istream (if ==0)	*/
  /******************************************************************************/

//	csrr	t4, CSR_XCAUSE
	csrr	t6, CSR_XTVAL

  /******************************************************************************/
  /* null adjustment if exception address is neither code nor data relative; ****/
  /* FUTURE FIXME: this may need to be updated to handle 48 or 64b opcodes   ****/
  /******************************************************************************/

/* now check for addr adjustments - code, data, or sig region (cause is in t5)*/

illop_\__MODE__\()tval:			// if cause is illop, save always
	addi	t2, t5, -CAUSE_ILLEGAL_INSTRUCTION	// spcl case mcause==2 (illegal op) no addr adj
	beqz	t2, sv_\__MODE__\()tval

chk_\__MODE__\()tval:
	LI(	t4, SET_REL_TVAL_MSK)	// now check if code or data (or sig) region adjustment

	srl	t4, t4, t5		// put mcause bit# into LSB
	slli	t4, t4, XLEN-1		// put mcause bit# into MSB
	bge	t4, x0, sv_\__MODE__\()tval	// if MSB=0, no adj, sv to ensure tval was cleared


code_\__MODE__\()adj:
	LA(t4, rvtest_code_end)		  // Fetch rvtest_code_end to compare against mtval.
  bge t6, t4, data_\__MODE__\()adj  // Compare if mtval > rvtest_code_end ; address belong outside code area ==> will be adjusted via data memory
  LA(	t4, rvtest_code_begin)	      // Fetch rvtest_code_end to compare against mtval.
	blt t6, t4, data_\__MODE__\()adj  // Compare if mtval < rvtest_code_begin ; address belong outside code area ==> will be adjusted via data memory
  addi t3, t4, 0		    // If rvtest_code_begin < mtval < rvtest_code_end ==> Adjustment will be made via code region
  j  adj_\__MODE__\()tval

data_\__MODE__\()adj:			// only possibilities left are testdata or sigdata
	LA(	t3, rvtest_data_end)
	bge	t6, t3, sig_\__MODE__\()adj	// after data area, must be sig, adj according to sig label
	LA(	t3, rvtest_data_begin)
	bge	t6, t3, adj_\__MODE__\()tval	// inside data area, lv rvtest_data_begin as adjustment amt

sig_\__MODE__\()adj:
	LA(	t3, mtrap_sigptr)	// adj assuming sig_region access

adj_\__MODE__\()tval:			// For Illegal op handling or tval not loaded - opcode not address
	sub	t6, t6, t3		// perform mtval adjust by either code, data, or sig position in t3

sv_\__MODE__\()tval:
	SREG	t6, 3*REGWIDTH(t1)	      // save 4rd sig value, (intID)

skp_\__MODE__\()tval:

  .if (\__MODE__\() == M)
    .ifdef  __H_EXT__
	csrr	t2, CSR_MTVAL2
	SREG	t2, 4*REGWIDTH(t1)	// store 5th sig value, only if mmode handler and VS mode exists
	csrr	t2, CSR_MTINST
	SREG	t2, 5*REGWIDTH(t1)	// store 6th sig value, only if mmode handler and VS mode exists
    .endif
  .endif

chk_\__MODE__\()trapsig_overrun:	//FIXME: move trap signature check to here
#ifdef rvtest_sig_end			// if not defined, we can't test for overrun
#define trap_sig_sz (rvtest_sig_end-mtrap_sigptr)
	LA(	t4, rvtest_trap_sig)
	LREG	t4, 0(t4)		// get the preincremented trap signature ptr
	LI(	t6, trap_sig_sz)	// length of trap sig; exceeding this is an overrun, test error
	add	t3, t3, t6		// calculate rvtest_sig_end from mtrap_sigptr, avoiding an LA
	bgtu	t4, t3, cleanup_epilogs	// abort test if pre-incremented value overruns
#endif

  /**** vector to execption special handling routines ****/
	li	t2, XLEN<<3		    // offset of exception dispatch table bzse
	j	spcl_\__MODE__\()handler	// jump to shared int/excpt spcl handling dispatcher

 /**** common return code for both interrupts and exceptions ****/
resto_\__MODE__\()rtn:			// restore and return
	LREG	t1, 1*REGWIDTH(sp)
	LREG	t2, 2*REGWIDTH(sp)
	LREG	t3, 3*REGWIDTH(sp)
	LREG	t4, 4*REGWIDTH(sp)
	LREG	t5, 5*REGWIDTH(sp)
	LREG	t6, 6*REGWIDTH(sp)
	LREG	sp, 7*REGWIDTH(sp)	// restore temporaries

	\__MODE__\()RET			// return to test, after padding adjustment (macro to handle case)

 /***************************************************/
 /**** This is the interrupt specific code. It	 ****/
 /**** clears the int and saves int-specific CSRS****/
 /***************************************************/
common_\__MODE__\()int_handler:		// t1 has sig ptr, t5 has mcause
	LI(	t3, 1)
 //**FIXME** - make sure this is kept up-to-date with fast int extension and others
	andi	t2, t5, XLEN-1		// clr INT bit	(**NOTE rmv extra bits if future extns use upper bits)
	sll	t3, t3, t2		// create mask 1<<xcause **NOTE**: that MSB is ignored in shift amt
	csrrc	t4, CSR_XIE, t3		// read, then attempt to clear int enable bit??
	csrrc	t4, CSR_XIP, t3		// read, then attempt to clear int pend bit
sv_\__MODE__\()ip:			// note: clear has no effect on MxIP
	SREG	t4, 2*REGWIDTH(t1)	// save 3rd sig value, (xip)

	li	t2, 0			// index of interrupt dispatch table base

/***************************************************/
/**** shared spcl int/excp dispatcher.		****/
/**** JR to table in t3+t2, indexed by mcause	****/
/***************************************************/
spcl_\__MODE__\()handler:		// case table branch to special handler code, depending on mcause
  .set int_handler_tblsz,	  excpt_Mhndlr_tbl-clrint_Mtbl
	LA(	t3, clrint_\__MODE__\()tbl)
//	LA(	t3, (clrint_\__MODE__\()tbl))	
	// load spcl int/excpt handler dispatch table 
	add	t3, t3, t2		// offset into the correct table
	slli	t2, t5, 3		// index into 8b aligned dispatch entry and jump through it
	add	t3, t3, t2
	LREG	t3, 0(t3)
	jr	t3			// this defaults to resto_\__MODE__\()rtn

\__MODE__\()clr_Msw_int:				 // default to just return if not defined
	RVMODEL_CLEAR_MSW_INT
	j	resto_\__MODE__\()rtn

\__MODE__\()clr_Mtmr_int:				 // default to just return
	RVMODEL_CLEAR_MTIMER_INT
	j	resto_\__MODE__\()rtn

\__MODE__\()clr_Mext_int:				 // default to just return
	RVMODEL_CLEAR_MEXT_INT
	SREG	t3, 3*REGWIDTH(t1)	      // save 4rd sig value, (intID)
	j	resto_\__MODE__\()rtn


\__MODE__\()clr_Ssw_int:				 // default to just return if not defined
	RVMODEL_CLEAR_SSW_INT
	j	resto_\__MODE__\()rtn

\__MODE__\()clr_Stmr_int:				 // default to just return
	RVMODEL_CLEAR_STIMER_INT
	j	resto_\__MODE__\()rtn

\__MODE__\()clr_Sext_int:				 // default to just return
	RVMODEL_CLEAR_SEXT_INT
	SREG	t3, 3*REGWIDTH(t1)	      // save 4rd sig value, (intID)
	j	resto_\__MODE__\()rtn


\__MODE__\()clr_Vsw_int:				// default to just return if not defined
	RVMODEL_CLEAR_VSW_INT
	j	resto_\__MODE__\()rtn

\__MODE__\()clr_Vtmr_intrvtest_code_begin:			       // default to just return
	RVMODEL_CLEAR_VTIMER_INT
	j	resto_\__MODE__\()rtn

\__MODE__\()clr_Vext_int:			       // default to just return
	RVMODEL_CLEAR_VEXT_INT
	SREG	t3, 3*REGWIDTH(t1)	      // save 4rd sig value, (intID)
	j	resto_\__MODE__\()rtn

/**** this is the table of interrupt clearing routine pointers, which could include special handlers ****/
/**** They default to RVMODEL macros above, which are model supplied, then jump to rtn code	     ****/

  .align ALIGNSZ
clrint_\__MODE__\()tbl:			//this code should only touch t2..t6
	.dword	resto_\__MODE__\()rtn	// int cause  0 is reserved, just return
#ifdef rvtest_strap_routine
	.dword	\__MODE__\()clr_Ssw_int		// int cause  1	 Smode SW int
#else
	.dword	cleanup_epilogs		// no Smode, impossible
#endif
#ifdef rvtest_vtrap_routine
	.dword	\__MODE__\()clr_Vsw_int		// int cause  2	 Vmode SW int#else
#else
	.dword	cleanup_epilogs		// no Smode, impossible
#endif
#ifdef rvtest_Mtrap_routine
	.dword	\__MODE__\()clr_Msw_int		// int cause  3	 Mmode SW int#else
#else
	.dword	cleanup_epilogs		// no Smode, impossible
#endif

	.dword	resto_\__MODE__\()rtn	// int cause  4 is reserved, just return
#ifdef rvtest_strap_routine
	.dword	\__MODE__\()clr_Stmr_int		// int cause  5	 Smode Tmr int
#else
	.dword	cleanup_epilogs		// no Smode, impossible
#endif
#ifdef rvtest_vtrap_routine
	.dword	\__MODE__\()clr_Vtmr_int		// int cause  6	 Vmode Tmr int
#else
	.dword	cleanup_epilogs		// no Vmode, impossible
#endif

#ifdef rvtest_mtrap_routine
	.dword	\__MODE__\()clr_Mtmr_int		// int cause  7	 Mmode Tmr int
#else
	.dword	cleanup_epilogs		// no Mmode, impossible
#endif

	.dword	resto_\__MODE__\()rtn	// int cause  8 is reserved, just return
#ifdef rvtest_strap_routine
	.dword	\__MODE__\()clr_Sext_int		// int cause  9	 Smode Ext int
#else
	.dword	cleanup_epilogs		// no Smode, impossible
#endif

#ifdef rvtest_vtrap_routine
	.dword	\__MODE__\()clr_Vext_int		// int cause  A	 Vmode Ext int
#else
	.dword	cleanup_epilogs		// no Vmode, impossible
#endif
#ifdef rvtest_mtrap_routine
	.dword	\__MODE__\()clr_Mext_int		// int cause  B	 Mmode Ext int
#else
	.dword	cleanup_epilogs		// no Mmode, impossible
#endif

 .rept NUM_SPECD_INTCAUSES-0xC
	.dword	resto_\__MODE__\()rtn	// int cause c..NUM_SPECD_INTCAUSES is reserved, just return
 .endr
 .rept XLEN-NUM_SPECD_INTCAUSES
	.dword	cleanup_epilogs		// impossible, quit test by jumping to	epilogs
 .endr

/**** this is the table of exception handling routine pointers, which ****/
/****  could include special handlers. They default to the rtn code   ****/
excpt_\__MODE__\()hndlr_tbl:		// handler code should only touch t2..t6 ****<<--must be speced!****
 .rept NUM_SPECD_EXCPTCAUSES
	.dword	resto_\__MODE__\()rtn	// default, just return
 .endr
 .rept XLEN-NUM_SPECD_INTCAUSES
	.dword	cleanup_epilogs		// impossible, quit test by jumping to epilogs
 .endr
.option pop
.endm

/**** These are invocations of the model supplied interrupt clearing macros ****/
/**** Note there is a copy per mode, though they could all be the same code ****/
/****	!!!! This must be speced as to which registers they may touch !!!!  ****/
// **FIXME** : the spec needs to be updated with the per/mode versions, not just one
// **FIXME**: ove these outside the handler so it can copied per mode using INSTANTIATE_MODE_MACRO
	//Does this work if we recode like thks?:
	//  clr_\__MODE__\()sw_int:				 // default to just return if not defined
	//  RVMODEL_CLEAR_\__MODE__\()SW_IN
	//  j	resto_\__MODE__\()rtn



/*******************************************************************************/
/**************** cleanup code; restore xtvec or where it points to ************/
/********* Assumption: in M-mode, because GOTO_MMODE always ends tests *********/
/********* Assumption: XSCRATCH pnts to save area for appropriate mode *********/
/*******************************************************************************/

.macro RVTEST_TRAP_EPILOG __MODE__
.option push
.option norvc

	XCSR_VRENAME \__MODE__			// retarget XCSR names to this modes CSRs, no V/S aiasing

exit_\__MODE__\()cleanup:
	csrr	t1, CSR_XSCRATCH		// pointer to save area
resto_\__MODE__\()edeleg:
	LREG	t2, xedeleg_sv_addr(t1)		// get saved xedeleg at offset -32

.if	(\__MODE__\() == V)
	csrw	CSR_VEDELEG, t2	//special case: VS EDELEG available from Vmode
.elseif	(\__MODE__\() == M)
#ifdef rvtest_strap_routine
	csrw	CSR_XEDELEG, t2	//this handles M  mode restore, but only if Smode exists
#endif
.else
//FIXME: if N-extension or anything like it is implemented, uncomment the following
//	csrw	CSR_XEDELEG, t2	//this handles S  mode restore
.endif
resto_\__MODE__\()satp:
	LREG	t2, xsatp_sv_addr(t1)		// restore saved xsatp
	csrw	CSR_XSATP,  t2
resto_\__MODE__\()scratch:
	LREG	t5, xscr_save_addr(t1)		// restore saved xscratch
	csrw	CSR_XSCRATCH, t5
resto_\__MODE__\()xtvec:
	LREG	t4, xtvec_sav_addr(t1)		// restore  orig xtvec addr & load current one
	csrrw	t2, CSR_XTVEC, t4
	andi	t4, t4, -4			// remove mode, so both word aligned 
	andi	t2, t2, -4
	bne	t4, t2, 1f			// if saved!=curr mtvec, done, else need to restore tramp

resto_\__MODE__\()tramp:			// t2 now contains where to restore to
	addi	t4, t1, tramp_sv_addr		// t4 now contains where to restore from
	LREG	t3, trampend_addr(t1)		// t3 tracks how much to restore

resto_\__MODE__\()loop:
	lw	t6, 0(t4)			// read saved tramp entry
	sw	t6, 0(t2)			// restore original tramp entry
	addi	t2, t2, 4			// next tgt  index
	addi	t4, t4, 4			// next save index
	blt	t2, t3, resto_\__MODE__\()loop	// didn't get to end, continue
  1:
.global rvtest_\__MODE__\()end
rvtest_\__MODE__\()end:

#ifdef HANDLER_TESTCODE_ONLY
	//**fixme**: add conditional code to compare original trampoline with
	// restored trampoline and store the deltas in the trap signature region
	// must work for each mode
#endif
 .option pop
 .endm
/*******************************************************************************/
/**************** end cleanup code; should fall into RVMODEL_HALT   ************/
/*******************************************************************************/

/*******************************************************************************/
/**** This macro defines per/mode save areas for mmode for each mode	    ****/
/**** note that it is the code area, not the data area, and		    ****/
/**** must be mulitple of 8B, so multiple instantiations stay aligned	    ****/
/**** This is preceded by the current signature pointer, (@Mtrpreg_sv -64?  ****/
/*******************************************************************************/
.macro RVTEST_TRAP_SAVEAREA __MODE__
.align 6
.option push
.option norvc
.global \__MODE__\()trapreg_sv

//****ASSERT: this shoud be a 64B boundary******//
\__MODE__\()tramptbl_sv:	// save area of existing trampoline table
.rept ((tramp_sz>>2)+1) & -2	// align to dblwd
	j	.+0		// prototype jump instruction, offset to be filled in
.endr

\__MODE__\()satp_sv:
	.dword 0		// save area for incoming xsatp
\__MODE__\()mode_rtn:
	jalr   x0, 8(t2)	// This replaces trap trampoline entry for S/Vmode
	.word 0			// dummy to keep alignment
\__MODE__\()trampend_sv:
	.dword	0		// save location of end of trampoline
\__MODE__\()edeleg_sv:
	.dword	0		// save location for edeleg CSR
\__MODE__\()tvec_new:
	.dword	0		// points to the in-use tvec, actual tramp table used
\__MODE__\()tvec_save:
	.dword	0		// save area for incoming mtvec
\__MODE__\()scratch_save:
	.dword	0		// save area for incoming mscratch
\__MODE__\()trapreg_sv:
	.fill	8, REGWIDTH, 0xdeadbeef	    // handler reg save area, 2 extra, keep dbl alignment

\__MODE__\()sv_area_end:	// used to clc size, which is used to avoid CSR read
.set \__MODE__\()sv_area_sz, (\__MODE__\()sv_area_end-\__MODE__\()satp_sv)

.option pop
.endm

//==============================================================================
// This section defines the required test format spec macros:
// RVTEST_[CODE/DATA/SIG]_[BEGIN/END]
//==============================================================================


/**************************** CODE BEGIN w/ TRAP HANDLER START	*********************/
/**** instantiate prologs using RVTEST_TRAP_PROLOG() if rvtests_xtrap_routine is ****/
/**** is defined, then initializes regs & defines rvtest_code_begin global label ****/
/************************************************************************************/
.macro RVTEST_CODE_BEGIN
 .option push
 .option norvc
 .align UNROLLSZ
 .section .text.init
 .globl	 rvtest_init
 .global rvtest_code_begin		//define the label and make it available

rvtest_init:				//instantiate prologs here
  INSTANTIATE_MODE_MACRO RVTEST_TRAP_PROLOG
  RVTEST_INIT_GPRS			// 0xF0E1D2C3B4A59687
rvtest_code_begin:
 .option pop
.endm					//end of RVTEST_CODE_BEGIN
/*********************** end of RVTEST_CODE_BEGIN ***********************************/

/************************************************************************************/
/****	     The above is instantiated at the start of the actual test		 ****/
/****			 So the test is here					 ****/
/****	     the below is instantiated at the end   of the actual test		 ****/
/************************************************************************************/

/**************************************************************************************/
/**** RVTEST_CODE_END macro  defines end of test code: saves regs, transitions to  ****/
/**** Mmode, & instantiates epilog using RVTEST_TRAP_EPILOG() macros. Test code	   ****/
/**** falls through to this else must branch to label rvtest_code_end. This must   ****/
/**** branch to a RVMODEL_HALT macro at the end. The actual trap handlers for each ****/
/**** mode are instantiated immediately following with RVTEST_TRAP_HANDLER() macro ****/
/**************************************************************************************/

.macro RVTEST_CODE_END		// test is ended, but in no particular mode
  .option push
  .option norvc
  .global rvtest_code_end	// define the label and make it available
  .global cleanup_epilogs

rvtest_code_end:		// COMPLIANCE_HALT should get here
  #ifdef rvtest_gpr_save	// gpr_save area is instantiated at end of signature
    RVTEST_SAVE_GPRS  x1	gpr_save
  #endif
    RVTEST_GOTO_MMODE		// if only Mmode used by tests, this has no effect
cleanup_epilogs:		// jump here to quit, will restore state for each mode

	//restore xTVEC, trampoline, regs for each mode in opposite order that they were saved
#ifdef rvtest_mtrap_routine
    #ifdef rvtest_strap_routine
	#ifdef rvtest_vtrap_routine
	  RVTEST_TRAP_EPILOG V	// actual v-mode prolog/epilog/handler code
	#endif
      RVTEST_TRAP_EPILOG S	// actual s-mode prolog/epilog/handler code
    #endif
   RVTEST_TRAP_EPILOG M 	// actual m-mode prolog/epilog/handler code
#endif
//    INSTANTIATE_MODE_MACRO RVTEST_TRAP_EPILOG	//restore xTVEC, trampoline, regs for each mode

/************* test done, epolog has restored everying, jump to halt ****************/
  j	exit_cleanup		//skip around handlers, go to RVMODEL_HALT
/********************** trap handlers inserted here ***********************************/

    INSTANTIATE_MODE_MACRO RVTEST_TRAP_HANDLER

exit_cleanup:			// *** RVMODEL_HALT MUST follow this***, then data

  .option pop
.endm				// end of RVTEST_CODE_END

/************************************************************************************/
/**** RVTEST_CODE_END macros must fall thru or jump to an RVMODEL_HALT macro here ***/
/************************************************************************************/

/*===================================data section starts here========================*/

/************************************************************************************/
/**** RVTEST_DATA_BEGIN macro defines end of input data & rvtest_data_end label	 ****/
/**** this is a data area, so we instantiate trap save areas for each mode here	 ****/
/************************************************************************************/

.macro RVTEST_DATA_BEGIN
.data

.align 4	//ensure dbl alignment
/**************************************************************************************/
/**** this is the pointer to the current trap signature part of the signature area ****/
/**** it is shared by all trap modes, but shouldn't be instantiated unless at least****/
/**** 1 trap mode is defined (which is covered if m-mode trap handlers are defined ****/
/**************************************************************************************/

.global rvtest_trap_sig
rvtest_trap_sig:
	.dword	 mtrap_sigptr

/**** now instantiate separate save areas for each modes state	   ****/
/**** strictly speaking, should only be needed for reentrant traps ****/
.align 3	//redundant?
	INSTANTIATE_MODE_MACRO RVTEST_TRAP_SAVEAREA

.align 4

/************************************************************************************/
/**************** end of RVTEST_DATA_BEGIN; input data should follow ****************/
/************************************************************************************/

.global rvtest_data_begin
rvtest_data_begin:
.endm

/************************************************************************************/
/**************** RVTEST_DATA_END macro; defines global label rvtest_data_end	 ****/
/************************************************************************************/
.macro RVTEST_DATA_END
.global rvtest_data_end
 #ifndef rvtest_mtrap_routine
  mtrap_sigptr:
    .fill 2,4,0xdeadbeef
 #endif
 rvtest_data_end:

/**** create identity mapped page tables here if mmu is present ****/
.align 12

#ifdef rvtest_strap_routine
  rvtest_Sroot_pg_tbl:
    RVTEST_PTE_IDENT_MAP

  #ifdef rvtest_vtrap_routine
    rvtest_Vroot_pg_tbl:
      RVTEST_PTE_IDENT_MAP
  #endif
#endif
.endm
