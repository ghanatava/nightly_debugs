# task_struct Inspection — Process Representation in /proc

**Date:** 2026-03-06
**Kernel Subsystem:** Process Management
**Tools Used:** /proc, ps, cat, watch, custom C program
**Difficulty:** Beginner
**Time:** ~30 minutes

---

## Objective

Understand how the kernel represents a process via task_struct by reading its fields live through the /proc filesystem. Map kernel data structures to observable /proc entries.

---

## Background Concepts

### What is task_struct?

Every process in Linux is represented internally by a `task_struct` — a large C structure defined in `include/linux/sched.h`. It holds everything the kernel knows about a process:

| task_struct field | /proc equivalent | What it means |
|------------------|-----------------|---------------|
| `pid` | `/proc/<pid>/status` → Pid | Process ID |
| `ppid` | `/proc/<pid>/status` → PPid | Parent process ID |
| `state` | `/proc/<pid>/status` → State | Current run state |
| `prio` | `/proc/<pid>/sched` | Scheduling priority |
| `mm` | `/proc/<pid>/maps` | Memory map |
| `files` | `/proc/<pid>/fd/` | Open file descriptors |
| `cred` | `/proc/<pid>/status` → Uid/Gid | Credentials |
| `comm` | `/proc/<pid>/comm` | Process name |

### Process States

| State | Symbol | Meaning |
|-------|--------|---------|
| Running | R | On CPU or runqueue |
| Sleeping (interruptible) | S | Waiting, can be woken by signal |
| Sleeping (uninterruptible) | D | Waiting on I/O, cannot be interrupted |
| Zombie | Z | Dead but not reaped |
| Stopped | T | Paused via SIGSTOP |

---

## Lab

### Step 1 — Spawn a long running process to inspect

```bash
sleep 300 &
echo "PID: $!"
```

### Step 2 — Full status dump

```bash
cat /proc/<pid>/status
```

Key fields to read:
- `Name` — process name (maps to task_struct.comm)
- `State` — current state
- `Pid` / `PPid` / `Tgid`
- `VmRSS` — physical memory in use
- `Threads` — thread count
- `voluntary_ctxt_switches` — how often process yielded CPU willingly
- `nonvoluntary_ctxt_switches` — how often kernel preempted it

### Step 3 — Observe scheduling info

```bash
cat /proc/<pid>/sched
```

Shows:
- `se.sum_exec_runtime` — total CPU time consumed in nanoseconds
- `nr_switches` — total context switches
- `nr_voluntary_switches` / `nr_involuntary_switches`

### Step 4 — Observe memory map

```bash
cat /proc/<pid>/maps
```

Each line is a memory region — text, data, heap, stack, shared libraries. Maps directly to task_struct.mm.

### Step 5 — Observe open file descriptors

```bash
ls -la /proc/<pid>/fd
```

Each symlink is an open file descriptor. Maps to task_struct.files.

### Step 6 — Watch state changes live

```bash
watch -n 0.5 "cat /proc/<pid>/status | grep State"
```

Then in another terminal send SIGSTOP and SIGCONT:
```bash
kill -SIGSTOP <pid>   # state changes to T
kill -SIGCONT <pid>   # state changes back to S
```

---

## Observations

- `/proc` is not files on disk — it is a virtual filesystem the kernel populates in real time
- Every field in `/proc/<pid>/status` maps directly to a task_struct field
- State changes are visible immediately — no polling delay
- `voluntary_ctxt_switches` vs `nonvoluntary_ctxt_switches` tells you if a process is I/O bound (high voluntary) or CPU bound (high nonvoluntary)

---

## Key Lessons

- `/proc/<pid>/status` is your first stop when debugging any unknown process behavior
- D state (uninterruptible sleep) is the dangerous one — process is stuck on I/O and cannot be killed with SIGKILL
- `ls /proc/<pid>/fd | wc -l` gives instant FD count for any process — useful for debugging `too many open files` before the process actually hits the limit
- task_struct is the kernel's single source of truth for everything about a process — /proc is just a window into it

---

## Real World Relevance

When a process is misbehaving in production:
1. `cat /proc/<pid>/status` — state, memory, threads
2. `cat /proc/<pid>/limits` — resource limits
3. `ls /proc/<pid>/fd | wc -l` — FD count
4. `cat /proc/<pid>/sched` — CPU consumption and context switches

This sequence answers 80% of process-related production incidents before you need any other tool.

---

## Next Lab

fork vs clone vs exec — process creation mechanisms