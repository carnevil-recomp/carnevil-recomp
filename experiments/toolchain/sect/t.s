	.file	1 "IN.I"

 # GNU C 2.7.2.SN16.3.3 [AL 1.1, MM 40] Williams Phoenix compiled by GNU C

 # Cc1 defaults:
 # -mgas

 # Cc1 arguments (-G value = 0, Cpu = default, ISA = 4):
 # -quiet -O3 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G
 # -fno-builtin -o

gcc2_compiled.:
__gnu_compiled_c:
	.globl	wr_init
	.data
	.align	2
wr_init:
	.word	5
	.globl	ro_init
	.rdata
	.align	2
ro_init:
	.word	6
	.data
	.align	2
s_init:
	.word	7
	.globl	ptab
	.align	2
ptab:
	.word	$LC0
	.word	$LC1
	.rdata
	.align	2
$LC1:
	.ascii	"beta\000"
	.align	2
$LC0:
	.ascii	"alpha\000"
	.globl	wptab
	.data
	.align	2
wptab:
	.word	$LC2
	.word	$LC3
	.rdata
	.align	2
$LC3:
	.ascii	"delta\000"
	.align	2
$LC2:
	.ascii	"gamma\000"
	.data
	.align	2
sptab:
	.word	$LC4
	.rdata
	.align	2
$LC4:
	.ascii	"eps\000"
	.globl	msg
	.align	2
msg:
	.ascii	"const array\000"
	.globl	wmsg
	.data
	.align	2
wmsg:
	.ascii	"writable array\000"
	.globl	fv
	.align	2
fv:
	.word	0x3fc00000		# 1.5 (float)
	.text
	.align	2
	.globl	f
	.ent	f
f:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, extra= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	lw	$2,wr_init
	lw	$3,ro_init
	lw	$5,s_uninit
	addu	$2,$2,$3
	lw	$3,uninit
	lw	$6,s_init
	l.s	$f0,fv
	addu	$2,$2,$3
	addu	$2,$2,$5
	addu	$2,$2,$6
	addu	$2,$2,$4
	sll	$4,$4,2
	lw	$3,ptab($4)
	lw	$4,wptab($4)
	lw	$5,sptab
	lbu	$3,0($3)
	lbu	$4,0($4)
	lbu	$5,0($5)
	addu	$2,$2,$3
	addu	$2,$2,$4
	lbu	$3,wmsg
	addu	$2,$2,$5
	addu	$2,$2,99
	addu	$2,$2,$3
	trunc.w.s $f2,$f0,$4
	mfc1	$3,$f2
	.set	noreorder
	.set	nomacro
	j	$31
	addu	$2,$2,$3
	.set	macro
	.set	reorder

	.end	f

	.comm	uninit,4

	.lcomm	s_uninit,4
