/* The BIOS exec() (EXEC.C): load another program from the disk over the running one and jump to
   it, never to return. On the cabinet the test switch execs DIAG.EXE (hwinfo.c, diags_proc) and
   the diagnostics exec GAME.EXE to leave. In the port each program is its own executable next to
   this one (carnevil, carnevil-diag), so exec replaces the process with the other program, given
   the same command line. What the two share lives in the state directory (cmos.bin is written
   through on every CMOS write), as it lives in the CMOS on the cabinet. The four argument words
   the BIOS copies for the new program travel in CARNEVIL_EXEC_ARGS. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define EXE_SUFFIX ".exe"
#else
#define EXE_SUFFIX ""
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include "hal.h"

int	hal_argc;
char	**hal_argv;

static int self_dir(char *buf, size_t n)
{
	char	*s;
#if defined(_WIN32)
	DWORD	k = GetModuleFileNameA(NULL, buf, (DWORD)n);
	if(!k || k >= n) return 0;
#elif defined(__APPLE__)
	uint32_t k = (uint32_t)n;
	if(_NSGetExecutablePath(buf, &k)) return 0;
#else
	ssize_t	k = readlink("/proc/self/exe", buf, n - 1);
	if(k <= 0) return 0;
	buf[k] = 0;
#endif
	s = strrchr(buf, '/');
#ifdef _WIN32
	if(strrchr(buf, '\\') > s) s = strrchr(buf, '\\');
#endif
	if(!s) return 0;
	s[1] = 0;
	return 1;
}

int exec(char *file, unsigned int entry_address, int *arg_ptr)
{
	static const struct { const char *exe, *prog; } progs[] = {
		{ "GAME.EXE", "carnevil" }, { "DIAG.EXE", "carnevil-diag" },
	};
	char	name[64], path[1024], env[96];
	const char *prog = 0;
	char	*tmp;
	int	i;
	(void)entry_address;	/* the BIOS too jumps to the address in the file's header */
	snprintf(name, sizeof name, "%s", file);
	for(tmp = name; *tmp; tmp++) *tmp = toupper((unsigned char)*tmp);	/* "Make sure the filename is uppercase" */
	for(i = 0; i < (int)(sizeof progs / sizeof progs[0]); i++)
		if(!strcmp(name, progs[i].exe)) prog = progs[i].prog;
	if(!prog || !hal_argv || !self_dir(path, sizeof path))
	{
		fprintf(stderr, "[exec] %s: no program for it\n", name);
		return 1;
	}
	strncat(path, prog, sizeof path - strlen(path) - 1);
	strncat(path, EXE_SUFFIX, sizeof path - strlen(path) - 1);
	if(access(path, X_OK) != 0)
	{
		fprintf(stderr, "[exec] %s: %s is not there (the diagnostics are being ported on the diag branch)\n", name, path);
		return 1;
	}
	snprintf(env, sizeof env, "CARNEVIL_EXEC_ARGS=%d,%d,%d,%d", arg_ptr ? arg_ptr[0] : 0, arg_ptr ? arg_ptr[1] : 0,
		 arg_ptr ? arg_ptr[2] : 0, arg_ptr ? arg_ptr[3] : 0);
	putenv(strdup(env));
	fprintf(stderr, "[exec] %s -> %s\n", name, path);
	fflush(NULL);
	hal_argv[0] = path;
#ifdef _WIN32
	{
		/* the Windows runtime splits a command line again on spaces: quote every argument */
		char	**q = calloc(hal_argc + 1, sizeof *q);
		for(i = 0; i < hal_argc; i++)
		{
			q[i] = malloc(strlen(hal_argv[i]) + 3);
			sprintf(q[i], "\"%s\"", hal_argv[i]);
		}
		if(_spawnv(_P_NOWAIT, path, (const char * const *)q) != -1) _exit(0);
	}
#else
	execv(path, hal_argv);
#endif
	fprintf(stderr, "[exec] %s: cannot start %s\n", name, path);
	return 2;
}
