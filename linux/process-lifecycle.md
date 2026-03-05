# Process Lifecycle — Zombie and Orphan Processes

**Date:** 2026-03-06
**Kernel Subsystem:** Process Management
**Tools Used:** gcc, ps, /proc, kill, virsh, custom C programs
**Difficulty:** Beginner-Intermediate
**Time:** ~2 hours

---

## Objective

Understand zombie and orphan process lifecycle by creating them manually, observing their behavior in the process table and /proc filesystem, and then inducing a real process table exhaustion scenario via zombie flood chaos.

---

## Background Concepts

### fork() return values

`fork()` returns twice — once in each process, with different values:

| Process | fork() return value |
|---------|-------------------|
| Parent | Child's actual PID (positive integer) |
| Child | 0 |

`pid == 0` does not mean the child's PID is 0. It is just the kernel's signal that "you are the newly created process." The child calls `getpid()` to get its actual PID.

### What is a Zombie Process?

A process that has finished executing but whose parent never called `wait()` to collect its exit status. It stays in the process table as a corpse holding one PID slot. Consumes zero CPU and zero memory — only occupies a process table entry.

### What is an Orphan Process?

A process whose parent exited before the child finished. The kernel automatically reparents it to PID 1 (systemd). Orphans are harmless — systemd reaps them cleanly.

---

## Lab 1 — Zombie Process

### Code

```c
// ~/process-management/lifecycle/zombie/zombie.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    pid_t pid = fork();

    if (pid > 0) {
        printf("Parent PID: %d\n", getpid());
        printf("Child PID: %d\n", pid);
        printf("Parent sleeping — child is now a zombie\n");
        sleep(60);  // Parent alive but never calls wait()
    } else if (pid == 0) {
        printf("Child exiting now\n");
        exit(0);
    }

    return 0;
}
```

### Steps

```bash
gcc -o zombie zombie.c
./zombie &
ps aux | head -1 && ps aux | grep zombie
```

### Observation

```
USER         PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND
ghanatava   7480  0.0  0.0   2680  1672 pts/0    S    19:55   0:00 ./zombie
ghanatava   7481  0.0  0.0      0     0 pts/0    Z    19:55   0:00 [zombie] <defunct>
```

Key observations:
- Child shows `STAT=Z` — zombie state
- Child shows `VSZ=0 RSS=0` — zero memory consumption
- `<defunct>` tag confirms process is dead but not reaped

### Verify via /proc

```bash
cat /proc/<zombie-pid>/status | grep -E 'State|VmRSS|VmSize'
```

Output:
```
State: Z (zombie)
```

### How to Kill a Zombie

You cannot `kill -9` a zombie — it is already dead. Kill the parent instead:

```bash
kill -9 <parent-pid>
ps aux | grep Z   # zombie disappears
```

**Key insight:** You never kill a zombie. You kill its parent.

Alternatively send SIGCHLD to signal the parent to reap — only works if parent has a SIGCHLD handler:
```bash
kill -SIGCHLD <parent-pid>
```

### The Proper Fix — wait()

```c
// zombie_fixed.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid > 0) {
        printf("Parent PID: %d waiting for child\n", getpid());
        wait(NULL);  // properly reaps the child
        printf("Child reaped — no zombie\n");
        sleep(10);
    } else if (pid == 0) {
        printf("Child PID: %d exiting\n", getpid());
        exit(0);
    }

    return 0;
}
```

With `wait()` — no zombie ever created.

---

## Lab 2 — Orphan Process

### Code

```c
// ~/process-management/lifecycle/orphan/orphan.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    pid_t pid = fork();

    if (pid > 0) {
        printf("Parent PID: %d — exiting now\n", getpid());
        exit(0);  // Parent exits immediately
    } else if (pid == 0) {
        sleep(1);  // Wait for parent to die before checking PPID
        printf("Child PID: %d — still running\n", getpid());
        printf("My parent is now: %d\n", getppid());  // Should print 1
        sleep(30);
    }

    return 0;
}
```

### Steps

```bash
gcc -o orphan orphan.c
./orphan & ps -ef | head -1 && ps -ef | grep orphan
```

### Observation

```
UID        PID  PPID  C STIME TTY          TIME CMD
ghanatava  7451     1  0 19:45 ?        00:00:00 ./orphan
```

PPID is **1** — systemd adopted the orphan.

### Race Condition Observed

First run without `sleep(1)` in child showed PPID as the original parent PID, not 1. Child called `getppid()` before parent had fully exited and kernel had reparented it.

**Lesson:** Kernel does not guarantee ordering between parent and child after fork(). This is your first taste of race conditions in process lifecycle.

### ps aux vs ps -ef

| | `ps aux` | `ps -ef` |
|--|---------|---------|
| Style | BSD syntax | UNIX syntax |
| Shows PPID | No | Yes |
| Shows %CPU/%MEM | Yes | No |
| Use when | Checking resource usage | Checking process relationships |

---

## Lab 3 — Zombie Flood Chaos

### Objective

Artificially exhaust the process table using zombie processes. Observe system behavior at pid_max limit. Practice recovery via hypervisor.

### Setup

```bash
# Take snapshot before chaos
# On host:
sudo virsh snapshot-create-as linux-lab \
  --name "pre-zombie-chaos" \
  --description "before pid_max chaos"

# Lower pid_max inside VM
echo 400 | sudo tee /proc/sys/kernel/pid_max
cat /proc/sys/kernel/pid_max  # verify
ps aux | wc -l                # check current process count
```

### Zombie Bomb Script

```bash
#!/bin/bash
# zombie-chaos.sh

echo "Current PID count: $(ps aux | wc -l)"
echo "pid_max: $(cat /proc/sys/kernel/pid_max)"
echo "Spawning zombies..."

for i in $(seq 1 100); do
    ~/process-management/lifecycle/zombie/zombie &
    echo "Spawned zombie batch $i — PID: $!"
done

echo "Done spawning"
echo "New PID count: $(ps aux | wc -l)"
echo "Zombie count: $(ps aux | grep -c Z)"
```

### Symptoms Observed

```
Child exiting now
Child exiting now
Child pid 339
./zombie-chaos.sh: fork: retry: Resource temporarily unavailable
./zombie-chaos.sh: fork: retry: Resource temporarily unavailable
```

- Last PID assigned was 339 — pid_max of 400 exhausted
- `fork: retry: Resource temporarily unavailable` — kernel cannot create new processes
- All recovery commands failed — could not spawn new processes to run kill, xargs, or anything else
- System completely unresponsive to new process creation

### Failed Recovery Attempts

Both approaches failed because running them requires forking new processes:

```bash
# Could not run — no PIDs available
ps -eo ppid,stat | grep Z | awk '{print $1}' | sort -u | xargs kill -9

# Could not run either
echo 4194304 | sudo tee /proc/sys/kernel/pid_max
```

### Actual Fix — External Recovery via Hypervisor

```bash
# On host machine — hard reset
sudo virsh destroy linux-lab
sudo virsh start linux-lab

# Or revert to pre-chaos snapshot
sudo virsh snapshot-revert linux-lab "pre-zombie-chaos"
```

After recovery, restore pid_max immediately:
```bash
echo 32768 | sudo tee /proc/sys/kernel/pid_max
```

---

## Key Lessons

**On zombies:**
- A zombie holds zero memory and zero CPU — only one process table slot
- You cannot kill a zombie directly — kill its parent
- `wait()` in the parent is the correct prevention
- SIGCHLD handler is the production-grade prevention for daemon processes

**On orphans:**
- Orphans are harmless — systemd adopts and reaps them
- Race conditions exist between parent exit and kernel reparenting
- Always add delay or use proper synchronization when checking PPID after fork

**On process table exhaustion:**
- When pid_max is exhausted no new process can be created — including recovery commands
- External access via hypervisor or physical console is the only escape
- In production this scenario is caused by applications forking heavily without wait()
- Classic culprits: old PHP-FPM configs, Java apps, poorly written shell scripts

**On /proc:**
- `/proc/<pid>/status` shows process state, memory, and metadata
- Zombie state shown as `Z` in ps STAT column and `State: Z` in /proc status
- VSZ and RSS both 0 for zombies confirms zero memory consumption

---

## Real World Relevance

This is a real production scenario. When process table fills up:
- No new SSH sessions can be established
- No new processes can spawn — cron jobs, alerts, health checks all fail
- The only recovery path is console access or hypervisor intervention
- Monitoring `ps aux | wc -l` vs `/proc/sys/kernel/pid_max` can give early warning

---