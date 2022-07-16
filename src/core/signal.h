/*
    This file is based on houz' signal handlers in darktable,
    Copyright (C) 2016-2020 darktable developers.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include "gui/gui.h"
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <limits.h>

static void
dt_sigsegv_handler(int param)
{
  char filename[PATH_MAX];
  snprintf(filename, sizeof(filename), "/tmp/%s-bt-%d.txt", ap_name, (int)getpid());
  FILE *f = fopen(filename, "wb");
  if(!f) return; // none of the code below works otherwise :(
  fprintf(f, "this is apdt reporting a crash:\n\n");
  fclose(f);

  char pid_arg[100], com_arg[PATH_MAX+100], log_arg[PATH_MAX+100];
  snprintf(pid_arg, sizeof(pid_arg), "%d", (int)getpid());
  snprintf(com_arg, sizeof(com_arg), "%s/gdb_commands", d.basedir);
  snprintf(log_arg, sizeof(log_arg), "set logging file %s", filename);

  int delete_file = 0;
  pid_t pid;
  if((pid = fork()) != -1)
  {
    if(pid)
    { // allow the child to ptrace us
      prctl(PR_SET_PTRACER, pid, 0, 0, 0);
      waitpid(pid, NULL, 0);
      fprintf(stderr, "backtrace written to %s\n", filename);
    }
    else
    {
      printf("%d - %s/gdb_commands - set logging file %s \n", (int)getpid(), d.basedir, filename);
      if(execlp("gdb", "gdb", ap_name, pid_arg, "-batch", "-ex", log_arg, "-x", com_arg, NULL))
      {
        delete_file = 1;
        fprintf(stderr, "an error occurred while trying to execute gdb."
            "please check if gdb is installed on your system.\n");
      }
    }
  }
  else
  {
    delete_file = 1;
    fprintf(stderr, "an error occurred while trying to execute gdb.\n");
  }
  if(delete_file) unlink(filename);
  exit(1);
}

static inline void
dt_set_signal_handlers()
{
  if(SIG_ERR == signal(SIGSEGV, &dt_sigsegv_handler))
    perror("[dt_set_signal_handler]");
  if(SIG_ERR == signal(SIGABRT, &dt_sigsegv_handler))
    perror("[dt_set_signal_handler]");
}
