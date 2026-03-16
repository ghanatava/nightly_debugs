# fork vs clone vs exec — Process Creation Mechanisms

**Date:** 2026-03-06
**Kernel Subsystem:** Process Management
**Tools Used:** gcc, strace, /proc, ps, custom C programs
**Difficulty:** Beginner-Intermediate
**Time:** ~45 minutes

---

## Objective

Understand the three process creation syscalls — fork(), clone(), and exec() — by writing programs that use each, observing what they share vs copy, and connecting them to real world container internals.

---

## Background Concepts

### The Three Syscalls

| Syscall | What it does | Used by |
|---------|-------------|---------|
| `fork()` | Creates an identical copy of the parent process | Shell commands, daemons |
| `clone()` | Creates a new process with fine-grained control over what is shared | Threads, containers |
| `exec()` | Replaces the current process image with a new program | Shell after fork |

All three ultimately call `do_fork()` in the kernel. The difference is in the flags passed.

### What fork() copies vs shares

| Resource | fork() behavior |
|----------|----------------|
| Memory | Shared via CoW until written |
| File descriptors | Copied — child gets same open FDs |
| Signal handlers | Copied |
| PID | New PID assigned to child |
| Threads | Only calling thread is copied |

### clone() flags that matter

| Flag | Effect |
|------|--------|
| `CLONE_VM` | Share memory space (threads) |
| `CLONE_FS` | Share filesystem (cwd, root) |
| `CLONE_FILES` | Share file descriptor table |
| `CLONE_NEWPID` | New PID namespace (containers) |
| `CLONE_NEWNET` | New network namespace (containers) |
| `CLONE_NEWNS` | New mount namespace (containers) |

`docker run` is essentially `clone()` with CLONE_NEWPID + CLONE_NEWNET + CLONE_NEWNS + CLONE_NEWUTS.

---

## Lab 1 — fork()

```c
// fork_demo.c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int shared_var = 100;
    pid_t pid = fork();

    if (pid > 0) {
        shared_var = 200;  // parent modifies — CoW triggers
        printf("Parent PID=%d PPID=%d shared_var=%d\n",
               getpid(), getppid(), shared_var);
        wait(NULL);
    } else if (pid == 0) {
        printf("Child  PID=%d PPID=%d shared_var=%d\n",
               getpid(), getppid(), shared_var);
    }
    return 0;
}
```

```bash
gcc -o fork_demo fork_demo.c && ./fork_demo
```

### Observation
- Child sees `shared_var=100` — got parent's value at fork time
- Parent sees `shared_var=200` — its own copy after CoW
- Child PPID matches parent PID — confirms parent/child relationship

### Verify with strace
```bash
strace -e trace=clone ./fork_demo 2>&1 | grep clone
```
Shows the actual `clone()` syscall fork() uses under the hood.

---

## Lab 2 — clone() with shared memory (thread behavior)

```c
// clone_demo.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>

#define STACK_SIZE 1024 * 1024

int shared_var = 100;

int child_fn(void *arg) {
    shared_var = 999;  // modifies shared memory directly
    printf("Child (clone) PID=%d shared_var=%d\n", getpid(), shared_var);
    return 0;
}

int main() {
    char *stack = malloc(STACK_SIZE);
    char *stack_top = stack + STACK_SIZE;

    // CLONE_VM means child shares parent's memory — no CoW
    pid_t pid = clone(child_fn, stack_top, CLONE_VM | SIGCHLD, NULL);

    waitpid(pid, NULL, 0);

    printf("Parent PID=%d shared_var=%d\n", getpid(), shared_var);
    free(stack);
    return 0;
}
```

```bash
gcc -o clone_demo clone_demo.c && ./clone_demo
```

### Observation
- Both parent and child see `shared_var=999` after child writes
- No CoW — CLONE_VM means they share the exact same memory pages
- This is how threads work — same address space, different execution contexts

---

## Lab 3 — fork() + exec() (how shell runs commands)

```c
// forkexec_demo.c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    printf("Parent PID=%d about to fork+exec ls\n", getpid());

    pid_t pid = fork();

    if (pid == 0) {
        printf("Child PID=%d before exec\n", getpid());
        execl("/bin/ls", "ls", "-la", "/proc/self", NULL);
        perror("exec failed");
    } else {
        wait(NULL);
        printf("Parent PID=%d child finished\n", getpid());
    }
    return 0;
}
```

```bash
gcc -o forkexec_demo forkexec_demo.c && ./forkexec_demo
```

### Observation
- Child PID stays the same before and after exec
- exec() replaces the process image but keeps the PID
- This is exactly what your shell does for every command you type

---

## Key Differences Summary

| | fork() | clone(CLONE_VM) | exec() |
|--|--------|----------------|--------|
| New PID | Yes | Yes | No — same PID |
| Memory | CoW copy | Shared directly | Replaced entirely |
| FDs | Copied | Depends on flags | Kept (unless O_CLOEXEC) |
| Use case | New process | Threads, containers | Replace process image |

---

## Connection to Containers

```bash
# Every docker run is essentially:
clone(child_fn,
      stack_top,
      CLONE_NEWPID |    # new PID namespace
      CLONE_NEWNET |    # new network namespace
      CLONE_NEWNS  |    # new mount namespace
      CLONE_NEWUTS |    # new hostname namespace
      SIGCHLD,
      NULL);
```

Understanding clone() flags is understanding container isolation. When a container can reach the host network unexpectedly, it means CLONE_NEWNET was not set or was bypassed.

---

## Key Lessons

- fork() and clone() both call do_fork() internally — fork() is just clone() with specific default flags
- CLONE_VM is the difference between a process and a thread at the kernel level
- exec() does not create a new process — it replaces the current one, PID stays the same
- Every shell command is fork() + exec() — the shell forks itself, child calls exec() with your command
- Container isolation is entirely implemented via clone() namespace flags — no magic involved

---

## Real World Relevance

- Unexpected memory sharing between processes — check if CLONE_VM was used
- Container escapes often involve namespace flag misconfigurations — understanding clone() flags tells you exactly what is and is not isolated
- `strace -e trace=clone` on any process shows exactly how it creates children and what it shares

---

## Next Lab

Copy-on-Write — memory behavior after fork()