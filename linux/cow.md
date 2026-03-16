# Copy-on-Write — Memory Behavior After fork()

**Date:** 2026-03-06
**Kernel Subsystem:** Memory Management, Process Management
**Tools Used:** gcc, /proc/smaps, custom C program
**Difficulty:** Intermediate
**Time:** ~45 minutes

---

## Objective

Prove Copy-on-Write behavior by observing shared vs private memory pages before and after fork(), and before and after a write operation. Connect CoW to containers, qcow2 disks, and production memory spikes.

---

## Background Concepts

### What is Copy-on-Write?

When fork() is called, the kernel does not copy the parent's memory immediately. Instead:

1. Child gets a copy of the parent's page table
2. Both point to the same physical memory pages
3. Both pages are marked read-only
4. When either process writes to a page — kernel gets a page fault, copies only that page, write proceeds

Memory is only copied at the moment of writing, and only the page that was written. Everything else stays shared.

### Why it matters

- fork() of a 2GB process is near-instant — no 2GB copy happens
- Production memory spikes after fork are CoW materializing — not a memory leak
- Container image layers use CoW — writes go to a new layer, base image stays shared
- qcow2 VM disks use CoW — only written blocks consume actual disk space

### /proc/smaps fields

| Field | Meaning |
|-------|---------|
| `Shared_Clean` | Pages shared with other processes, unmodified |
| `Shared_Dirty` | Pages shared with other processes, modified |
| `Private_Clean` | Pages private to this process, unmodified |
| `Private_Dirty` | Pages private to this process, modified |

After fork() — child shows high Shared, low Private.
After write — writing process shows high Private, low Shared.

---

## Lab

### Code

```c
// cow.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define MB 1024 * 1024
#define ALLOC_SIZE 100  // 100MB

void print_memory(const char *label) {
    char path[64];
    char line[256];
    snprintf(path, sizeof(path), "/proc/%d/status", getpid());
    FILE *f = fopen(path, "r");
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS", 5) == 0 ||
            strncmp(line, "VmSize", 6) == 0) {
            printf("[%s] PID %d: %s", label, getpid(), line);
        }
    }
    fclose(f);
}

void print_smaps_summary(const char *label) {
    char path[64];
    char line[256];
    long shared = 0, private = 0;
    snprintf(path, sizeof(path), "/proc/%d/smaps", getpid());
    FILE *f = fopen(path, "r");
    while (fgets(line, sizeof(line), f)) {
        long val;
        if (sscanf(line, "Shared_Clean: %ld", &val) == 1) shared += val;
        if (sscanf(line, "Shared_Dirty: %ld", &val) == 1) shared += val;
        if (sscanf(line, "Private_Clean: %ld", &val) == 1) private += val;
        if (sscanf(line, "Private_Dirty: %ld", &val) == 1) private += val;
    }
    fclose(f);
    printf("[%s] PID %d: Shared=%ldkB Private=%ldkB\n",
           label, getpid(), shared, private);
}

int main() {
    char *data = malloc(ALLOC_SIZE * MB);
    memset(data, 'A', ALLOC_SIZE * MB);

    printf("=== Before fork ===\n");
    print_memory("parent-pre-fork");
    print_smaps_summary("parent-pre-fork");

    pid_t pid = fork();

    if (pid > 0) {
        sleep(1);
        printf("\n=== Parent after fork — before write ===\n");
        print_memory("parent-post-fork");
        print_smaps_summary("parent-post-fork");

        memset(data, 'B', ALLOC_SIZE * MB);  // triggers CoW

        printf("\n=== Parent after write — CoW triggered ===\n");
        print_memory("parent-post-write");
        print_smaps_summary("parent-post-write");

        wait(NULL);

    } else if (pid == 0) {
        printf("\n=== Child after fork — before write ===\n");
        print_memory("child-post-fork");
        print_smaps_summary("child-post-fork");

        char dummy = data[0];  // read only — no CoW
        (void)dummy;
        printf("\n=== Child after read — no CoW ===\n");
        print_smaps_summary("child-read-only");

        exit(0);
    }

    free(data);
    return 0;
}
```

```bash
gcc -o cow cow.c
./cow
```

---

## Observed Output

```
=== Before fork ===
[parent-pre-fork] PID 1109: VmSize:   105084 kB
[parent-pre-fork] PID 1109: VmRSS:    103952 kB
[parent-pre-fork] PID 1109: Shared=1564kB Private=102516kB

=== Child after fork — before write ===
[child-post-fork] PID 1110: VmSize:   105084 kB
[child-post-fork] PID 1110: VmRSS:    103344 kB
[child-post-fork] PID 1110: Shared=103432kB Private=40kB

=== Child after read — no CoW ===
[child-read-only] PID 1110: Shared=103432kB Private=40kB

=== Parent after fork — before write ===
[parent-post-fork] PID 1109: VmSize:   105084 kB
[parent-post-fork] PID 1109: VmRSS:    104208 kB
[parent-post-fork] PID 1109: Shared=1692kB Private=102516kB

=== Parent after write — CoW triggered ===
[parent-post-write] PID 1109: VmSize:   105084 kB
[parent-post-write] PID 1109: VmRSS:    104208 kB
[parent-post-write] PID 1109: Shared=1692kB Private=102516kB
```

---

## Analysis

### Before fork
Parent has ~100MB all private — expected, nothing shared yet.

### Child immediately after fork
```
Shared=103432kB   Private=40kB
```
Child absorbed 100MB as shared pages instantly. Kernel copied no memory — child just got a new page table pointing to parent's physical pages. Private is only 40kB — just child's own stack.

### Child after read
```
Shared=103432kB   Private=40kB   (unchanged)
```
Read-only access triggered no page fault, no copy. CoW working correctly — reads are free.

### Parent after write
Parent private stays at ~102MB — parent was already the owner of these pages before fork. CoW copies pages to the child on child's first write, not to the parent. Parent keeps its original pages.

### The definitive proof
The child went from 0 shared pages to 103MB shared pages in one fork() call with zero memory copied. That is CoW.

---

## To See CoW Trigger on Child Side

Replace child read with a write:
```c
memset(data, 'C', ALLOC_SIZE * MB);  // child writes
print_smaps_summary("child-post-write");
```

Child's Private will jump to ~100MB and Shared will drop — the page copy happening in real time.

---

## Key Lessons

- After fork(), child shows high Shared — they share physical pages
- Read access never triggers CoW — reads are always free
- CoW copies happen per page, not per allocation — only the written page is copied
- Parent private pages don't change on write — parent was already the owner
- Production memory spikes after fork are CoW materializing, not leaks — check smaps before panicking

---

## Real World Relevance

- Unexpected memory spike after a fork-heavy operation — check `/proc/<pid>/smaps` for Private_Dirty growth
- Container layer writes — each write to a file in a container triggers CoW at the filesystem layer, same principle
- Redis fork() for RDB snapshots — Redis forks to snapshot, parent keeps serving writes, CoW means only modified pages are copied — this is why Redis memory spikes during saves

---

## Next Lab

rlimits — process resource limits and exhaustion