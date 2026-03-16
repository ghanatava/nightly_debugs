# rlimits — Process Resource Limits and Exhaustion

**Date:** 2026-03-06
**Kernel Subsystem:** Process Management, Resource Management
**Tools Used:** ulimit, /proc/limits, prlimit, gcc, custom C program
**Difficulty:** Intermediate
**Time:** ~1 hour

---

## Objective

Understand Linux resource limits (rlimits) by observing default limits, exhausting file descriptors, observing cascading failures, and fixing limits at the session, persistent, and per-process level.

---

## Background Concepts

### What are rlimits?

Every process has resource limits inherited from its parent and enforced by the kernel. Two levels exist for each limit:

| Level | Who can change it | Meaning |
|-------|------------------|---------|
| Soft limit | The process itself | Currently enforced ceiling |
| Hard limit | Root only | Absolute maximum the soft limit can be raised to |

### Limits that matter in production

| Limit | ulimit flag | Controls | Production symptom |
|-------|------------|---------|-------------------|
| `nofile` | `-n` | Open file descriptors | `Too many open files` |
| `nproc` | `-u` | Max processes/threads | `Resource temporarily unavailable` on fork |
| `stack` | `-s` | Stack size per process | Segfault on deep recursion |
| `memlock` | `-l` | Locked memory | eBPF programs fail to load |
| `core` | `-c` | Core dump size | No crash dumps generated |

### Baseline limits observed

```
open files       1024       (very low — common cause of production incidents)
max user processes  15339
stack size       8192 kB
core file size   0          (no core dumps — dangerous in production)
max locked memory  501232 kB
```

---

## Lab 1 — File Descriptor Exhaustion

### Code

```c
// fd_exhaust.c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main() {
    int fd_count = 0;
    int fd;

    printf("Starting FD exhaustion...\n");
    printf("Current PID: %d\n", getpid());

    while ((fd = open("/dev/null", O_RDONLY)) != -1) {
        fd_count++;
        if (fd_count % 100 == 0) {
            printf("Opened %d file descriptors, last fd=%d\n", fd_count, fd);
        }
    }

    printf("\n--- LIMIT HIT ---\n");
    printf("Total FDs opened: %d\n", fd_count);
    printf("Error: %s\n", strerror(errno));
    printf("errno value: %d\n", errno);

    char path[64];
    char line[256];
    snprintf(path, sizeof(path), "/proc/%d/limits", getpid());
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        printf("Cannot open /proc/limits — out of file descriptors!\n");
        return 1;
    }
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "open files") || strstr(line, "Max open")) {
            printf("%s", line);
        }
    }
    fclose(f);

    return 0;
}
```

```bash
gcc -o fd_exhaust fd_exhaust.c
./fd_exhaust
```

### Observed Output

```
Opened 100 file descriptors, last fd=102
...
Opened 1000 file descriptors, last fd=1002
--- LIMIT HIT ---
Total FDs opened: 1021
Error: Too many open files
errno value: 24
Cannot open /proc/limits — out of file descriptors!
Segmentation fault (core dumped)
```

### Analysis

- Hit limit at exactly **1021 FDs** — 1024 minus 3 (stdin/stdout/stderr already open)
- errno 24 = `EMFILE` — Too many open files
- **Cascading failure observed:** fopen() for /proc/limits also failed because no FDs were left to open the file. NULL pointer dereference caused segfault.
- This is the real danger of FD exhaustion — it does not just break the primary operation. It breaks every subsequent operation that needs to open anything.

---

## Lab 2 — Fix FD Limit Three Ways

### Fix 1 — Current shell session only

```bash
ulimit -n 4096
./fd_exhaust   # now opens ~4093 before hitting limit
```

Survives only for the current shell session. Resets on logout.

### Fix 2 — Persistent, survives reboot

```bash
sudo nano /etc/security/limits.conf
```

Add:
```
ghanatava soft nofile 65536
ghanatava hard nofile 65536
```

Log out and back in, then verify:
```bash
ulimit -n   # should show 65536
```

### Fix 3 — Per process without restart (production trick)

Raise limits on an already running process via prlimit:
```bash
sudo prlimit --nofile=4096:4096 --pid <pid>
cat /proc/<pid>/limits | grep "open files"
```

This is the production fix when you cannot restart the affected process.

---

## Lab 3 — Verify /proc/limits

For any running process:
```bash
cat /proc/<pid>/limits
```

Shows every limit — soft, hard, and units. First command to run when debugging `too many open files` in production.

---

## Key Lessons

- `nofile=1024` is dangerously low for any modern application — databases, web servers, and service meshes easily exceed this
- FD exhaustion causes cascading failures — not just the primary operation but every subsequent fopen, socket, or file access fails
- Fix order: prlimit for running processes, ulimit for current session, limits.conf for persistent
- `cat /proc/<pid>/limits` is your first command in any resource limit incident
- Core dump size of 0 means no crash analysis is possible — always set this to unlimited in non-production environments

---

## Real World Relevance

`Too many open files` is one of the most common production errors. Common culprits:

- **nginx/apache** — each connection costs one FD. High traffic + low nofile = connection failures
- **Kubernetes** — each pod connection through kube-proxy costs FDs on the node
- **Databases** — PostgreSQL, MySQL open FDs per connection, per WAL file, per table
- **Java apps** — JVM opens FDs for class files, jars, network connections simultaneously

Standard production baseline: `nofile=65536` soft, `nofile=1048576` hard.

---

## Next Lab

Process credentials — real vs effective UID, privilege escalation mechanics