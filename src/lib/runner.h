#ifndef RUNNER__H
#define RUNNER__H

int process_start(char **command);
int process_wait(pid_t pid);
int process_kill(int pid);

#endif
