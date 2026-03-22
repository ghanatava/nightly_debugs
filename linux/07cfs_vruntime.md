# CFS Scheduling — vruntime and Context Switches

**Date:** 2026-03-06
**Kernel Subsystem:** Process Scheduling
**Tools Used:** stress-ng, taskset, perf, /proc/sched, top
**Difficulty:** Intermediate
**Time:** ~45 minutes

---

## Objective

Observe the Completely Fair Scheduler (CFS) in action by measuring context switches between two CPU-bound processes competing for the same core, and understand how vruntime drives scheduling decisions.

---

## Background Concepts

### How CFS Works

CFS tracks a virtual runtime (vruntime) for every process — the total CPU time consumed, weighted by priority. The scheduler always picks the process with the lowest vruntime from a red-black tree.

```
Process A runs → vruntime increases
vruntime exceeds Process B's vruntime
Scheduler preempts A, runs B
B's vruntime catches up
Scheduler preempts B, runs A
```

This leapfrogging ensures fairness — no process gets significantly more CPU time than another at equal priority.

### Key /proc/sched Fields

Reading /proc/1/sched for systemd:

```
se.vruntime              12.127918       # virtual runtime in nanoseconds
se.sum_exec_runtime    1091.707690       # total CPU time since boot (ms)
nr_voluntary_switches     1145           # process yielded CPU willingly
nr_involuntary_switches   3867           # kernel preempted the process
prio                       120           # default priority (nice 0 = prio 120)
se.slice               1400000           # time slice in nanoseconds (1.4ms)
policy                       0           # 0 = SCHED_NORMAL (CFS)
```

### Priority to Nice Value Mapping

| Nice value | Kernel priority | CPU weight |
|-----------|----------------|-----------|
| -20 (highest) | 100 | ~88761 |
| 0 (default) | 120 | 1024 |
| 19 (lowest) | 139 | 15 |

### voluntary vs involuntary switches

- High voluntary = I/O bound process (yields willingly waiting for data)
- High involuntary = CPU bound process (kernel forcibly preempts it)

---

## Lab 1 — Observe /proc/sched Baseline

```bash
cat /proc/1/sched
grep -E 'vruntime|nr_switches|prio|policy|slice' /proc/1/sched
```

---

## Lab 2 — Measure CFS Context Switches with perf

### Setup

```bash
taskset -c 0 stress-ng --cpu 1 --timeout 30s & PID1=$!
taskset -c 0 stress-ng --cpu 1 --timeout 30s & PID2=$!
echo "PID1=$PID1 PID2=$PID2"
```

### Measure

```bash
sudo perf stat -e context-switches -p $PID1,$PID2 sleep 5
```

### Observed Output

```
Performance counter stats for process id '5431,5432':
    2528      context-switches
    5.000345555 seconds time elapsed
```

### Analysis

2528 context switches in 5 seconds = ~505 switches per second. Each process got ~1-2ms before preemption — matches se.slice=1400000ns (1.4ms). CFS fairness working correctly.

---

## Why /proc/sched vruntime Appeared Static

Shell script sampling at 0.1s intervals showed frozen vruntime values. Expected — /proc/sched only updates when the process is actually scheduled on the reading CPU. Observer interference and sampling rate both too coarse.

Lesson: Use perf for scheduler observations. /proc polling is insufficient for microsecond-level events.

---

## Key Lessons

- CFS uses vruntime in a red-black tree — lowest vruntime always runs next
- Default time slice is ~1.4ms — processes swap ~500 times per second under contention
- nr_involuntary_switches high = CPU bound
- nr_voluntary_switches high = I/O bound
- perf hooks directly into kernel tracepoints — always prefer it over /proc polling for scheduler work

---

## Real World Relevance

- High involuntary switches + high CPU = CPU bound, consider nice adjustment or CPU pinning
- High voluntary switches + low CPU = I/O bound, bottleneck is not the CPU
- Unexpected latency spikes: `perf stat -e context-switches -p <pid> sleep 5`
- K8s CPU throttling manifests as involuntary switches on cgroup-limited containers

---

## Next Lab

SCHED_FIFO — real-time scheduling and CPU starvation