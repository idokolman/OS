#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num)
{
  SmallShell &smash = SmallShell::getInstance();
  cout << "smash: got ctrl-Z" << endl;
  if (smash.jobs.FGcmd == nullptr)
  {
    return;
  }
  cout << "smash: process " << smash.jobs.FGcmd->pid << " was stopped" << endl;
  CALL_SYS(kill(smash.jobs.FGcmd->pid, SIGSTOP), "kill");

  int new_job_id = smash.jobs.addJob(smash.jobs.FGcmd, FG, true, smash.jobs.job_id_of_FG);

  for (auto it = smash.timeoutList.begin(); it != smash.timeoutList.end();)
  {
    if (it->cmd == smash.jobs.FGcmd)
    {
      it->job_id = new_job_id;
    }
    it++;
  }
  smash.jobs.FGcmd = nullptr;
}

void ctrlCHandler(int sig_num)
{
  SmallShell &smash = SmallShell::getInstance();
  cout << "smash: got ctrl-C" << endl;
  if (smash.jobs.FGcmd == nullptr)
  {
    return;
  }
  cout << "smash: process " << smash.jobs.FGcmd->pid << " was killed" << endl;
  CALL_SYS(kill(smash.jobs.FGcmd->pid, SIGKILL), "kill");
  deleteCommand(smash.jobs.FGcmd);
  smash.jobs.FGcmd = nullptr;
}

void alarmHandler(int sig_num)
{
  SmallShell &smash = SmallShell::getInstance();
  smash.jobs.removeFinishedJobs();

  time_t curr;
  CALL_SYS(time(&curr), "time");

  int min_time = 0;
  for (auto it = smash.timeoutList.begin(); it != smash.timeoutList.end();)
  {
    if (it->finish_time <= curr)
    {
      cout << "smash: got an alarm" << endl;
      // todo - what if its FG
      if (it->job_id != -1 && it->cmd->pid != -1)
      { // its in job list and needs to be killed
        cout << "smash: " << it->cmd_line << " timed out!" << endl;
        CALL_SYS(kill(it->cmd->pid, SIGKILL), "kill"); //  if (kill(pid_to_fg, SIGCONT) != 0)
      }
      else if (it->cmd == smash.jobs.FGcmd)
      {
        cout << "smash: " << it->cmd_line << " timed out!" << endl;
        CALL_SYS(kill(it->cmd->pid, SIGKILL), "kill"); //  if (kill(pid_to_fg, SIGCONT) != 0)
      }
      it = smash.timeoutList.erase(it);
    }
    else
    {
      if (it->finish_time < min_time || min_time == 0)
      {
        min_time = it->finish_time;
      }
      ++it;
    }
  }
  if (!smash.timeoutList.empty())
  {
    alarm(min_time - curr);
  }

  smash.jobs.removeFinishedJobs();
}