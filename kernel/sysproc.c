#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0) return -1;
  exit(n);
  return 0;  // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  int flags;
  // int flags = argraw(0);
  // if (argint(0, &flags) < 0) return -1;

  if (argaddr(0, &p) < 0) return -1;
  if (argint(1, &flags) < 0) return -1;  // 获取 flags 参数
  // if (argaddr(1, &p) < 0) return -1;     // 获取 p 参数
  return wait(p, flags);

}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0) return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if (argint(0, &n) < 0) return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0) return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_rename(void) {
  char name[16];
  int len = argstr(0, name, MAXPATH);
  if (len < 0) {
    return -1;
  }
  struct proc *p = myproc();
  memmove(p->name, name, len);
  p->name[len] = '\0';
  return 0;
}

uint64 sys_yield(void) {
    // 获取当前的 CPU
    struct cpu *c_now = mycpu();
    // 获取当前的进程的 proc 结构体指针
    struct proc *p_now = myproc();

    // 打印出该进程对应的上下文
    printf("Save the context of the process to the memory region from address %p to %p\n", 
           &(c_now->context), (char*)(&(c_now->context)) + sizeof(c_now->context));

    // 打印当前进程的 pid 和此进程在用户态的 pc 值
    printf("Current running process pid is %d and user pc is %p\n", 
           p_now->pid,(p_now->trapframe->epc));


    for (;;) {
        // 避免死锁，确保设备可以打断
        intr_on();
        int found = 0;
        // 第一部分：从当前进程往后遍历到进程表的末尾
        for (struct proc *p = myproc(); p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if (p->state == RUNNABLE) {
                printf("Next runnable process pid is %d and user pc is %p\n", 
                       p->pid, (void*)(p->trapframe->epc));
                found = 1;
            }
            release(&p->lock);
            if (found) break; // 找到可运行进程，退出循环
        }

        // 如果没有找到可运行的进程，第二部分：从进程表头开始遍历到当前进程前
         if (!found) {
            for (struct proc *p = proc; p < p_now; p++) {
                acquire(&p->lock);
                if (p->state == RUNNABLE) {
                    printf("Next runnable process pid is %d and user pc is %p\n", p->pid, (void*)(p->trapframe->epc));
                    found = 1;
                }
                release(&p->lock);
                if (found) break; // 找到可运行进程，退出循环
            }
        }
        // 如果找到了 RUNNABLE 进程，退出循环
        if (found) {
          yield();
          break;
        }
    }
    return 0; // 返回
}

