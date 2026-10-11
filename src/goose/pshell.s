# PSHELL.S ($Revision: 2 $) - the process shell every GOOSE process starts in.
# GNU as transcription of original/sdk/lib/goose/PSHELL.S (asmmips syntax: section/xdef/xref/db,
# r-numbered registers). Byte-exact with CarnEvil: text 0x8014a930, version string 0x801a3128.
	.set	noreorder
	.data
	.globl	goose_pshell_s_version
goose_pshell_s_version:
	.asciz	"$Revision: 2 $"

	.text
	# $a0 = argument ptr, $a1 = function ptr (unused: both come from the process node)
	.globl	_proc_shell
_proc_shell:
	lui	$t0, %hi(cur_proc)
	addiu	$t0, $t0, %lo(cur_proc)
	lw	$t0, 0($t0)		# current process pointer
	nop
	# NOTE - if the process structure changes these offsets also need to change
	lw	$t1, 36($t0)		# entry point address
	lw	$a0, 16($t0)		# argument pointer
	jalr	$t1
	nop
	j	die			# if it returns - die
	addiu	$a0, $zero, 1
