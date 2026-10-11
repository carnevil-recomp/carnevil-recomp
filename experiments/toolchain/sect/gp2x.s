	.file	1 "IN.I"
	.abicalls

 # GNU C 2.7.2.SN16.3.3 [AL 1.1, MM 40] Williams Phoenix compiled by GNU C

 # Cc1 defaults:
 # -mgas

 # Cc1 arguments (-G value = 0, Cpu = R4600, ISA = 4):
 # -quiet -O3 -mips4 -mhard-float -fomit-frame-pointer -G -fno-builtin
 # -mabicalls -mno-gpopt -o

gcc2_compiled.:
__gnu_compiled_c:
	.text
	.align	2
	.globl	f
	.ent	f
f:
	.frame	$sp,32,$31		# vars= 0, regs= 2/0, args= 16, extra= 8
	.mask	0x90000000,-4
	.fmask	0x00000000,0
	.set	noreorder
	.cpload	$25
	.set	reorder
	subu	$sp,$sp,32
	.cprestore 16
	sw	$31,28($sp)
	sw	$28,24($sp)
	sw	$0,flag
	jal	g
	lw	$31,28($sp)
	addu	$sp,$sp,32
	j	$31
	.end	f
