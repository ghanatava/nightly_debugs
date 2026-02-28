# KVM Lab Setup — Complete Reference

> Personal reference for setting up and managing KVM/libvirt VMs on Ubuntu for DevOps lab work.
> Includes every command, error hit, and fix applied during initial setup.

---

## Environment

- **Host OS:** Ubuntu (work laptop)
- **RAM:** 16GB
- **Hypervisor:** KVM + libvirt + QEMU
- **Guest OS:** Ubuntu 24.04 LTS Server
- **Purpose:** Linux internals and networking lab reps (months 1-3 of DevOps roadmap)

---

## 1. Installation

### Install KVM and libvirt

```bash
sudo apt install qemu-kvm libvirt-daemon virt-manager bridge-utils
sudo usermod -aG libvirt $USER
```

### Set system URI as default (avoid session vs system conflicts)

```bash
export LIBVIRT_DEFAULT_URI="qemu:///system"
echo 'export LIBVIRT_DEFAULT_URI="qemu:///system"' >> ~/.bashrc
```

### Verify KVM acceleration is working

```bash
virt-host-validate
lsmod | grep kvm   # should show kvm_intel
```

---

## 2. Pre-flight: Fix the Default Network

### Problem
libvirt's default NAT network was not active, causing virt-install to fail with:
```
ERROR    Network not found: no network with matching name 'default'
```

### Fix — Define and start the default network

```bash
virsh net-define /usr/share/libvirt/networks/default.xml
sudo virsh net-start default
sudo virsh net-autostart default
sudo virsh net-list --all
```

Expected output:
```
 Name      State    Autostart   Persistent
 default   active   yes         yes
```

---

## 3. Errors Hit During Network Setup

### Error 1 — virbr0 already in use

```
error: Failed to start network default
error: internal error: Network is already in use by interface virbr0
```

**Diagnosis:**
```bash
ip link show virbr0
bridge link show virbr0
brctl show virbr0
```

Output showed `NO-CARRIER, state DOWN` — ghost interface, nothing connected, safe to remove.

**Fix:**
```bash
sudo ip link set virbr0 down
sudo brctl delbr virbr0
sudo virsh net-start default
```

---

### Error 2 — dnsmasq address already in use

```
error: internal error: Child process (dnsmasq) unexpected exit status 2:
dnsmasq: failed to create listening socket for 192.168.122.1: Address already in use
```

**What is dnsmasq?**
A lightweight DNS + DHCP server that libvirt uses to give VMs IP addresses and handle DNS inside the VM network. Not your system DNS. Safe to kill and let libvirt restart it fresh.

**Fix:**
```bash
sudo pkill dnsmasq
sudo virsh net-start default
```

---

### Error 3 — Operation not permitted creating virbr0

```
error: error creating bridge interface virbr0: Operation not permitted
```

**Cause:** Running virsh as user session instead of system instance.

**Fix:**
```bash
sudo virsh net-start default
# or set permanently:
export LIBVIRT_DEFAULT_URI="qemu:///system"
```

---

## 4. Create the VM Disk Image

```bash
mkdir -p ~/lab-vms
qemu-img create -f qcow2 ~/lab-vms/linux-lab.qcow2 40G
```

`qcow2` format — only consumes actual used space, not the full 40GB immediately.

---

## 5. Move Files to libvirt Standard Location

libvirt-qemu user cannot access `/home` by default. Move files to the standard libvirt images directory to avoid permission errors.

```bash
sudo mv ~/lab-vms/linux-lab.qcow2 /var/lib/libvirt/images/
sudo mv ~/Downloads/ubuntu-24.04.4-live-server-amd64.iso /var/lib/libvirt/images/
```

---

## 6. Install the VM

```bash
sudo virt-install \
  --name linux-lab \
  --ram 4096 \
  --vcpus 2 \
  --disk path=/var/lib/libvirt/images/linux-lab.qcow2,format=qcow2 \
  --os-variant ubuntu22.04 \
  --network network=default \
  --graphics spice \
  --cdrom /var/lib/libvirt/images/ubuntu-24.04.4-live-server-amd64.iso \
  --connect qemu:///system
```

**Flags explained:**
| Flag | Purpose |
|------|---------|
| `--ram 4096` | 4GB RAM allocated to VM |
| `--vcpus 2` | 2 virtual CPUs |
| `--os-variant ubuntu22.04` | OS metadata hint for libvirt optimizations |
| `--graphics spice` | Opens GUI console window during install |
| `--connect qemu:///system` | Forces system URI, avoids session/system mismatch |

---

## 7. Errors Hit During virt-install

### Error 1 — Kernel not found

```
ERROR    Couldn't find kernel for install tree.
```

**Cause:** Used `--location` with `--extra-args` which doesn't work with live server ISOs.

**Fix:** Switch to `--cdrom` flag instead of `--location`.

---

### Error 2 — Permission denied on disk/ISO

```
ERROR    Cannot access storage file '/home/ghanatava/lab-vms/linux-lab.qcow2'
(as uid:64055, gid:993): Permission denied
```

**Cause:** libvirt-qemu user cannot access files in `/home`.

**Fix:** Move files to `/var/lib/libvirt/images/` (see section 5).

---

### Error 3 — Network not active (session vs system mismatch)

```
ERROR    Requested operation is not valid: network 'default' is not active
```

**Cause:** Network was started with `sudo virsh` (system) but virt-install ran as user session — two different contexts.

**Fix:** Always use `--connect qemu:///system` with virt-install when running sudo, or set `LIBVIRT_DEFAULT_URI` permanently.

---

## 8. Post-Install — Take a Snapshot Immediately

Always snapshot right after a clean install. This is your reset button before every lab rep.

```bash
sudo virsh snapshot-create-as linux-lab \
  --name "fresh-install" \
  --description "clean base before any reps"

# Verify
sudo virsh snapshot-list linux-lab
```

---

## 9. VM Power Operations

### Start
```bash
sudo virsh start linux-lab
```

### Graceful shutdown
```bash
sudo virsh shutdown linux-lab
```

### Force kill (hard power off — disk and snapshots are safe)
```bash
sudo virsh destroy linux-lab
```

### Reboot
```bash
sudo virsh reboot linux-lab
```

### Check VM status
```bash
sudo virsh list --all
```

---

## 10. Connecting to the VM

### Open GUI console window
```bash
sudo virt-viewer --connect qemu:///system linux-lab

# With --wait flag (keeps trying while VM boots)
sudo virt-viewer --connect qemu:///system --wait linux-lab
```

### SSH into VM
```bash
# Get VM IP
sudo virsh domifaddr linux-lab

# SSH in
ssh <your-username>@<vm-ip>
```

### Open virt-manager (full GUI — VirtualBox-like experience)
```bash
sudo apt install virt-manager
virt-manager
```

---

## 11. Snapshot Operations

### Create snapshot
```bash
sudo virsh snapshot-create-as linux-lab \
  --name "snapshot-name" \
  --description "description"
```

### List snapshots
```bash
sudo virsh snapshot-list linux-lab
```

### Revert to snapshot (your reset button before/after lab reps)
```bash
sudo virsh snapshot-revert linux-lab fresh-install
```

### Delete snapshot
```bash
sudo virsh snapshot-delete linux-lab snapshot-name
```

---

## 12. Verify VM Health (run inside VM after login)

```bash
uname -r                        # kernel version
lscpu | grep Virtualization     # confirm KVM acceleration
free -h                         # confirm RAM
df -h                           # confirm disk
```

---

## Lab Workflow for Every Rep

```
1. sudo virsh snapshot-create-as linux-lab --name "pre-<rep-name>"
2. Do the rep — break things, fix things
3. Document findings
4. sudo virsh snapshot-revert linux-lab fresh-install   # if needed
```

---

*Last updated: February 2026*