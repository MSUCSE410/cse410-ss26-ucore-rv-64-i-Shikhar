#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"

uint64 sys_write(int fd, char *str, uint len)
{
	debugf("sys_write fd = %d str = %x, len = %d", fd, str, len);
	if (fd != STDOUT)
		return -1;
	for (int i = 0; i < len; ++i) {
		console_putchar(str[i]);
	}
	return len;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

uint64 sys_gettimeofday(TimeVal *val, int _tz)
{
	uint64 cycle = get_cycle();
	val->sec = cycle / CPU_FREQ;
	val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	return 0;
}

uint64 sys_task_info(TaskInfo *ti)
{
	if (ti == 0)
		return -1;

	struct proc *p = curr_proc();
	TaskInfo kti;

	switch (p->state) {
	case UNUSED:
		kti.status = UnInit;
		break;
	case RUNNABLE:
		kti.status = Ready;
		break;
	case RUNNING:
		kti.status = Running;
		break;
	default:
		kti.status = Exited;
		break;
	}

	for (int i = 0; i < MAX_SYSCALL_NUM; i++) {
		kti.syscall_times[i] = p->syscall_times[i];
	}

	uint64 now_ms = get_cycle() * 1000 / CPU_FREQ;
	if (p->first_scheduled_ms == 0) {
		kti.time = 0;
	} else {
		kti.time = (int)(now_ms - p->first_scheduled_ms);
	}

	memmove((void *)ti, &kti, sizeof(TaskInfo));
	return 0;
}

uint64 sys_getpid(void)
{
	return curr_proc()->pid;
}

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;

	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			  trapframe->a3, trapframe->a4, trapframe->a5 };

	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);

	if (id >= 0 && id < MAX_SYSCALL_NUM) {
		curr_proc()->syscall_times[id]++;
	}

	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], (char *)args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
		break;
	case SYS_task_info:
		ret = sys_task_info((TaskInfo *)args[0]);
		break;
	case SYS_getpid:
		ret = sys_getpid();
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}

	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}