#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/reg.h>

#include "ltrace.h"

#if (!defined(PTRACE_PEEKUSER) && defined(PTRACE_PEEKUSR))
# define PTRACE_PEEKUSER PTRACE_PEEKUSR
#endif

#if (!defined(PTRACE_POKEUSER) && defined(PTRACE_POKEUSR))
# define PTRACE_POKEUSER PTRACE_POKEUSR
#endif

void *get_instruction_pointer(struct process *proc)
{
	long int ret = ptrace(PTRACE_PEEKUSER, proc->pid, 8 * RIP, 0);
	if (proc->mask_32bit)
		ret &= 0xffffffff;
	return (void *)ret;
}

void set_instruction_pointer(struct process *proc, void *addr)
{
	if (proc->mask_32bit)
		addr = (void *)((long int)addr & 0xffffffff);
	ptrace(PTRACE_POKEUSER, proc->pid, 8 * RIP, addr);
}

void *get_stack_pointer(struct process *proc)
{
	long int ret = ptrace(PTRACE_PEEKUSER, proc->pid, 8 * RSP, 0);
	if (proc->mask_32bit)
		ret &= 0xffffffff;
	return (void *)ret;
}

void *get_return_addr(struct process *proc, void *stack_pointer)
{
	unsigned long int ret;
	ret = ptrace(PTRACE_PEEKTEXT, proc->pid, stack_pointer, 0);
	if (proc->mask_32bit)
		ret &= 0xffffffff;
	return (void *)ret;
}
