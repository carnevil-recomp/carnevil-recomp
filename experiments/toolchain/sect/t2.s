	.file	1 "IN.I"

 # GNU C 2.7.2.SN16.3.3 [AL 1.1, MM 40] Williams Phoenix compiled by GNU C

 # Cc1 defaults:
 # -mgas

 # Cc1 arguments (-G value = 0, Cpu = default, ISA = 4):
 # -quiet -O3 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G
 # -fno-builtin -mloadgp -o

gcc2_compiled.:
__gnu_compiled_c:
	.text
	.align	2
	.globl	f
	.ent	f
f:
	.frame	$sp,24,$31		# vars= 0, regs= 1/0, args= 16, extra= 0
	.mask	0x80000000,-8
	.fmask	0x00000000,0
	subu	$sp,$sp,24
	li	$2,0x00000001		# 1
	sw	$31,16($sp)
	sw	$0,flag
	.set	noreorder
	.set	nomacro
	jal	g
	sw	$2,0($28)
	.set	macro
	.set	reorder

	lw	$31,16($sp)
	addu	$sp,$sp,24
	j	$31
	.end	f
