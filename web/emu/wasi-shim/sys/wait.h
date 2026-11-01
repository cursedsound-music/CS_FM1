/* SPDX-License-Identifier: GPL-3.0-only */
/* wasm shim: the emulator never forks (hostsim's test runners only) */
#pragma once
#include <sys/types.h>
static inline pid_t fork(void) { return -1; }
static inline pid_t waitpid(pid_t p, int *s, int o) { (void)p; (void)s; (void)o; return -1; }
static inline int pipe(int f[2]) { (void)f; return -1; }
#define WIFEXITED(s) 0
#define WEXITSTATUS(s) 0
#define WIFSIGNALED(s) 0
#define WTERMSIG(s) 0
