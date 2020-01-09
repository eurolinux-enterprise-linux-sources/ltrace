#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include "ptrace.h"
#include <asm/unistd.h>

#include "ltrace.h"
#include "options.h"
#include "sysdep.h"

static int fork_exec_syscalls[][5] = {
	{
#ifdef __NR_fork
	 __NR_fork,
#else
	 -1,
#endif
#ifdef __NR_clone
	 __NR_clone,
#else
	 -1,
#endif
#ifdef __NR_clone2
	 __NR_clone2,
#else
	 -1,
#endif
#ifdef __NR_vfork
	 __NR_vfork,
#else
	 -1,
#endif
#ifdef __NR_execve
	 __NR_execve,
#else
	 -1,
#endif
	 }
#ifdef FORK_EXEC_SYSCALLS
	FORK_EXEC_SYSCALLS
#endif
};

/* Returns 1 if the sysnum may make a new child to be created
 * (ie, with fork() or clone())
 * Returns 0 otherwise.
 */
int fork_p(struct process *proc, int sysnum)
{
	unsigned int i;
	if (proc->personality
	    >= sizeof fork_exec_syscalls / sizeof(fork_exec_syscalls[0]))
		return 0;
	for (i = 0; i < sizeof(fork_exec_syscalls[0]) / sizeof(int) - 1; ++i)
		if (sysnum == fork_exec_syscalls[proc->personality][i])
			return 1;
	return 0;
}

/* Returns 1 if the sysnum may make the process exec other program
 */
int exec_p(struct process *proc, int sysnum)
{
	int i;
	if (proc->personality
	    >= sizeof fork_exec_syscalls / sizeof(fork_exec_syscalls[0]))
		return 0;
	i = sizeof(fork_exec_syscalls[0]) / sizeof(int) - 1;
	if (sysnum == fork_exec_syscalls[proc->personality][i])
		return 1;
	return 0;
}

void trace_me(void)
{
	if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0) {
		perror("PTRACE_TRACEME");
		exit(1);
	}
}

int trace_pid(pid_t pid)
{
	if (ptrace(PTRACE_ATTACH, pid, 1, 0) < 0) {
		return -1;
	}
	return 0;
}

void trace_set_options(struct process *proc, pid_t pid)
{
#ifndef PTRACE_SETOPTIONS
#define PTRACE_SETOPTIONS 0x4200
#endif
#ifndef PTRACE_OLDSETOPTIONS
#define PTRACE_OLDSETOPTIONS 21
#endif
#ifndef PTRACE_O_TRACESYSGOOD
#define PTRACE_O_TRACESYSGOOD 0x00000001
#endif
	if (proc->tracesysgood & 0x80)
		return;
	if (ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD) < 0 &&
	    ptrace(PTRACE_OLDSETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD) < 0) {
		perror("PTRACE_SETOPTIONS");
		return;
	}
	proc->tracesysgood |= 0x80;
}

void untrace_pid(pid_t pid)
{
	ptrace(PTRACE_DETACH, pid, 1, 0);
}

void continue_after_signal(pid_t pid, int signum)
{
	/* We should always trace syscalls to be able to control fork(), clone(), execve()... */
	ptrace(PTRACE_SYSCALL, pid, 0, signum);
}

void continue_process(pid_t pid)
{
	continue_after_signal(pid, 0);
}

void continue_enabling_breakpoint(pid_t pid, struct breakpoint *sbp)
{
	enable_breakpoint(pid, sbp);
	continue_process(pid);
}

void continue_after_breakpoint(struct process *proc, struct breakpoint *sbp)
{
	if (sbp->enabled)
		disable_breakpoint(proc->pid, sbp);
	set_instruction_pointer(proc, sbp->addr);
	if (sbp->enabled == 0) {
		continue_process(proc->pid);
	} else {
		proc->breakpoint_being_enabled = sbp;
#if defined __sparc__  || defined __ia64___
		/* we don't want to singlestep here */
		continue_process(proc->pid);
#else
		ptrace(PTRACE_SINGLESTEP, proc->pid, 0, 0);
#endif
	}
}

int umovestr(struct process *proc, void *addr, int len, void *laddr)
{
	union {
		long a;
		char c[sizeof(long)];
	} a;
	int i;
	int offset = 0;

	while (offset < len) {
		a.a = ptrace(PTRACE_PEEKTEXT, proc->pid, addr + offset, 0);
		for (i = 0; i < sizeof(long); i++) {
			if (a.c[i] && offset + (signed)i < len) {
				*(char *)(laddr + offset + i) = a.c[i];
			} else {
				*(char *)(laddr + offset + i) = '\0';
				return 0;
			}
		}
		offset += sizeof(long);
	}
	*(char *)(laddr + offset) = '\0';
	return 0;
}
