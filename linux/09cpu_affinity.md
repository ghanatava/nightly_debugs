# CPU Affinity — taskset, cpusets, and Kubernetes CPU Limits

**Date:** 2026-03-06
**Kernel Subsystem:** Process Scheduling, cgroups
**Tools Used:** taskset, stress-ng, top, /sys/fs/cgroup/cpuset
**Difficulty:** Intermediate
**Time:** ~30 minutes

---

## Objective

Understand CPU affinity — how processes are pinned to specific cores — and connect it directly to Kubernetes CPU requests/limits and cgroup cpusets which implement container CPU isolation.

---

## Background Concepts

### What is CPU Affinity?

CPU affinity is stored in `task_struct.cpus_allowed` as a bitmask defining which CPUs a process is allowed to run on.

```
2 CPUs available:
Default (no pinning)  → mask: 11 (binary) = 3 (decimal) → both cores allowed
Pin to CPU 0 only     → mask: 01 (binary) = 1 (decimal)
Pin to CPU 1 only     → mask: 10 (binary) = 2 (decimal)
```

### Connection to Kubernetes

When you set `resources.requests.cpu` and `resources.limits.cpu` on a pod, kubelet translates this into a **cpuset cgroup** restriction. The container's processes are allowed to run only on the cores kubelet assigns — implemented via the same cpuset mechanism as taskset, just automated.

```
Pod CPU limit → kubelet → cgroup cpuset → task_struct.cpus_allowed
```

Understanding taskset means understanding what K8s is doing under the hood for every CPU-limited pod.

---

## Lab 1 — Observe Default Affinity

```bash
sleep 300 &
PID=$!

# Affinity mask (hex)
taskset -p $PID

# Human readable CPU list
taskset -cp $PID
```

### Observed Output

```
pid 5565's current affinity mask: 3
pid 5565's current affinity list: 0,1
```

Mask 3 = binary 11 = both CPU 0 and CPU 1 allowed. Default for all processes — scheduler has full freedom to place it anywhere.

---

## Lab 2 — Pin Processes to Separate Cores

```bash
taskset -c 0 stress-ng --cpu 1 --timeout 120s & PID1=$!
taskset -c 1 stress-ng --cpu 1 --timeout 120s & PID2=$!
sleep 2

# Verify affinity
taskset -cp $PID1   # should show: 0
taskset -cp $PID2   # should show: 1

# Get worker child PIDs (actual CPU burners)
CPID1=$(pgrep -P $PID1)
CPID2=$(pgrep -P $PID2)

# Observe CPU usage — each should show ~100% on its own core
top -b -n 2 -p $CPID1,$CPID2
```

### Expected Behavior

Each worker owns its core entirely — no contention, no context switches between them. Compare to the CFS lab where both were on CPU 0 and got ~50% each with 2528 context switches in 5 seconds.

Pinning to separate cores eliminates scheduler overhead entirely for these workloads.

---

## Lab 3 — Change Affinity of Running Process

```bash
taskset -c 0 stress-ng --cpu 1 --timeout 120s & PID=$!
sleep 1

# Confirm on CPU 0
taskset -cp $PID

# Move to CPU 1 without restarting
taskset -cp 1 $PID

# Confirm moved
taskset -cp $PID
```

No restart required. Kernel updates task_struct.cpus_allowed immediately and the scheduler migrates the process on its next scheduling decision.

---

## Lab 4 — cpuset cgroup (what Kubernetes actually does)

```bash
# Check cpuset subsystem
cat /sys/fs/cgroup/cpuset.cpus 2>/dev/null || \
cat /sys/fs/cgroup/cpuset/cpuset.cpus

# Create a cpuset cgroup manually
sudo mkdir /sys/fs/cgroup/cpuset/lab_test
echo 0 | sudo tee /sys/fs/cgroup/cpuset/lab_test/cpuset.cpus
echo 0 | sudo tee /sys/fs/cgroup/cpuset/lab_test/cpuset.mems

# Add a process to it
echo $PID | sudo tee /sys/fs/cgroup/cpuset/lab_test/cgroup.procs

# Verify
taskset -cp $PID
cat /sys/fs/cgroup/cpuset/lab_test/cgroup.procs
```

This is exactly what kubelet does for every CPU-limited pod — automated cpuset assignment based on the pod's CPU request. The only difference is kubelet manages it dynamically as pods are scheduled and evicted.

---

## Key Lessons

- Default affinity mask = all CPUs — scheduler decides placement freely
- taskset -c pins at spawn time, taskset -cp changes affinity of running process
- Worker children inherit affinity from parent — pinning the parent pins all its children
- cpusets are the cgroup implementation of CPU affinity — K8s CPU limits are cpusets under the hood
- Pinning eliminates cross-core context switches for that process — reduces scheduler overhead

---

## Real World Relevance

**When to use CPU pinning in production:**
- Latency-sensitive workloads — eliminate scheduler migration overhead
- NUMA systems — pin processes to cores on the same NUMA node as their memory
- Noisy neighbor isolation — pin critical processes away from noisy workloads

**Kubernetes connection:**
- `resources.limits.cpu` → kubelet → cpuset cgroup → task_struct.cpus_allowed
- CPU throttling in K8s = CFS bandwidth controller, not cpuset — processes still run on all allowed CPUs but get throttled when they exceed quota
- `kubectl top pod` CPU numbers come from cgroup cpu.stat, not taskset

**Debugging CPU affinity issues:**
```bash
# Check affinity of any process
taskset -cp <pid>

# Check K8s container cpuset
cat /sys/fs/cgroup/cpuset/kubepods/<pod-uid>/cpuset.cpus

# Check if process was migrated across NUMA nodes
cat /proc/<pid>/sched | grep numa
```

---
