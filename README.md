# Pintos

An implementation of [Pintos](http://pintos-os.org), a teaching operating
system for the x86 architecture, built for the undergraduate Operating
Systems course (CS 600.318) at Johns Hopkins University.

Pintos is small enough to understand fully, yet realistic enough to run
real x86 machine code inside a simulator (QEMU, Bochs, or VMWare Player).
Students build out a working kernel across four projects, starting from a
minimal skeleton.

## What's implemented

| Project | Description | Status |
|---|---|---|
| **1 — Threads** | Alarm clock (no busy-waiting), priority scheduling with priority donation, and the 4.4BSD advanced scheduler (MLFQS) | ✅ Complete |
| **2 — User Programs** | Argument passing, system calls (process control + file I/O), user-memory validation, process wait/exit semantics, denying writes to running executables | ✅ Complete |
| **3 — Virtual Memory** | Page tables, page fault handling, swapping, memory-mapped files | Not started |
| **4 — File Systems** | Extensible files, subdirectories, buffer cache | Not started |

## Getting started

You'll need a Linux environment (native, VM, or Docker) with the Pintos
cross-compiler toolchain and either Bochs or QEMU as the simulator.

### Option 1: Docker

```bash
git clone https://github.com/Koshi2004/PintOS.git
cd PintOS
docker build -t pintos .
docker run -it --rm --mount type=bind,source="$(realpath ./pintos/src)",target=/pintos/src pintos
```

### Option 2: Linux / VM (e.g. Ubuntu on VirtualBox or WSL2)

```bash
git clone https://github.com/Koshi2004/PintOS.git
cd PintOS/pintos/src/misc

# Build the cross-compiler toolchain and Bochs
./toolchain-build.sh --prefix ~/pintos-toolchain ~/pintos-toolchain-src
./bochs-2.6.2-build.sh ~/pintos-toolchain

# Add the toolchain to your PATH (add this to ~/.bashrc to make it permanent)
export PATH=~/pintos-toolchain/bin:$PATH
```

### Building and testing a project

Once the environment is set up:

```bash
cd pintos/src/threads     # or userprog, vm, filesys
make
make check
```

`make check` builds and runs every test for that project inside the
simulator and reports pass/fail for each one.

**Note on simulators:** `Make.vars` sets `SIMULATOR = --bochs`. If your
host machine supports hardware virtualization (VT-x/AMD-V) and you're
running directly on it (not inside a nested VM), `--qemu` will generally
be faster. Inside a VM without nested virtualization, QEMU falls back to
slow software emulation and tests can time out — Bochs avoids this
because it's always software-emulated at a consistent speed.

## Repository layout

```
pintos/src/
├── threads/     Thread management, scheduling, synchronization
├── userprog/    Process loading, system calls
├── vm/          Virtual memory (Project 3)
├── filesys/     File system (Project 4)
├── devices/     Device drivers (timer, disk, keyboard, etc.)
├── lib/         Shared C library code (kernel and user)
└── tests/       Test programs and expected-output checkers
```

## Acknowledgements

Pintos was created by Ben Pfaff and others at Stanford University. This
course's variant comes from Ryan Huang's CS318 at Johns Hopkins
(originally from [ryanphuang/PintosM](https://github.com/ryanphuang/PintosM)).
The Docker setup is adapted from
[kavienanj/pintos](https://github.com/kavienanj/pintos).

See [`pintos/src/LICENSE`](pintos/src/LICENSE) for licensing details.