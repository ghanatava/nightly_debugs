# SCHED_FIFO — Real-Time Scheduling and CPU Starvation

**Date:** 2026-03-06
**Kernel Subsystem:** Process Scheduling, Real-Time
**Tools Used:** gcc, chrt, taskset, top, /proc/sys/kernel/sched_rt_runtime_us
**Difficulty:** Intermediate-Advanced
**Time:** ~30 minutes

---

## Objective

Observe how a SCHED_FIFO real-time process monopolizes a CPU core, completely starving normal CFS processes, and recover the system without rebooting.

---

## Background Concepts

### Scheduling Classes

| Policy | Behavior | Priority range |
|--------|---------|---------------|
| SCHED_NORMAL | CFS — fair sharing, preemptible | nice -20 to 19 |
| SCHED_FIFO | Real-time — runs until it yields or blocks, never preempted by normal processes | 1-99 |
| SCHED_RR | Real-time — like FIFO but with time slices between equal RT priority processes | 1-99 |

Kernel scheduling order is absolute:
```
RT processes (SCHED_FIFO/SCHED_RR) → always scheduled first
CFS processes (SCHED_NORMAL) → only run when RT queue is empty
```

### The Safety Valve

Linux limits RT process CPU consumption globally:
```
/proc/sys/kernel/sched_rt_period_us   = 1000000  (1 second period)
/proc/sys/kernel/sched_rt_runtime_us  = 950000   (950ms max for RT)
```

RT processes get max 95% of CPU. The remaining 5% is reserved for normal processes to run recovery commands. Without this, a runaway RT process would make the system completely unrecoverable without a reboot.

---

## Lab

### Code

```c
// fifo_hog.c
#define _GNU_SOURCE
#include <stdio.h>
#include <sched.h>
#include <unistd.h>

int main() {
    struct sched_param param;
    param.sched_priority = 99;  // highest RT priority

    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("sched_setscheduler failed");
        return 1;
    }

    printf("PID %d running as SCHED_FIFO priority 99\n", getpid());
    printf("Hogging CPU — normal processes on this core are starved\n");

    while (1) {}  // never yields, never blocks

    return 0;
}
```

```bash
gcc -o fifo_hog fifo_hog.c
```

### Run the starvation

```bash
# Take snapshot first
# On host: sudo virsh snapshot-create-as linux-lab --name "pre-fifo-chaos"

# Spawn victim on CPU 0
taskset -c 0 stress-ng --cpu 1 --timeout 120s &
VICTIM_PID=$!

# Spawn hog on CPU 0 — immediately starves victim
sudo taskset -c 0 ./fifo_hog &
HOG_PID=$!

# Observe
top -b -n 3 -p $HOG_PID,$VICTIM_PID
```

### Observed Output

```
PID    NI  %CPU  COMMAND
HOG    0   100   fifo_hog       ← owns CPU 0 entirely
VICTIM 0   0.1   stress-ng      ← completely starved
```

The 0.1% the victim gets is from kernel interrupts briefly preempting even RT processes — hardware interrupts have higher priority than SCHED_FIFO.

### Why system stayed responsive

2 vCPUs — hog was pinned to CPU 0 only. CPU 1 remained completely free for shell, SSH, and all other processes. On a single-core system or unpinned RT process this would be catastrophic.

### Recovery

```bash
sudo chrt -o -p 0 $HOG_PID
```

Breaking it down:
- `chrt` — change real-time scheduling attributes of a running process
- `-o` — set policy to SCHED_OTHER (SCHED_NORMAL)
- `-p 0` — set priority to 0 (CFS processes have no RT priority)

Kernel moves the hog from RT run queue back into CFS red-black tree with fresh vruntime. Victim gets its CPU share back immediately.

---

## Check the Safety Valve

```bash
cat /proc/sys/kernel/sched_rt_runtime_us
cat /proc/sys/kernel/sched_rt_period_us
```

Default: 950000 / 1000000 = 95% max for RT processes. The remaining 5% is your recovery window.

---

## Key Lessons

- SCHED_FIFO never gets preempted by SCHED_NORMAL — it runs until it yields or blocks
- A SCHED_FIFO process in an infinite loop will starve everything else on that core
- Hardware interrupts still preempt RT processes — that 0.1% CPU the victim got was interrupt handling
- `chrt -o -p 0 <pid>` is your recovery command — demotes RT process to normal CFS instantly
- sched_rt_runtime_us is the kernel safety valve — always verify it is not set to -1 (unlimited) in production

---

## Real World Relevance

Legitimate uses of SCHED_FIFO:
- Audio drivers — need guaranteed low latency
- Industrial control systems — deterministic response time
- Low latency networking — kernel bypass paths

Danger scenarios:
- Developer accidentally sets RT priority on application process
- Application hits infinite loop or deadlock
- Everything on that core starves — health checks, metrics, alerts all fail
- If sched_rt_runtime_us = -1, system becomes completely unrecoverable without reboot

Production rule: never set SCHED_FIFO on application processes. Use nice values for priority adjustment instead.

---

## Next Lab

CPU Affinity — taskset, cpusets, and the connection to Kubernetes CPU limits