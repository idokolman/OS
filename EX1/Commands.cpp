#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h> // doesn't work on windows
#include <iomanip>
#include "Commands.h"
#include <algorithm> // std::swap
#include <fcntl.h>

using namespace std;

using std::string;

const string WHITESPACE = " \n\r\t\f\v";
#if 0
#define FUNC_ENTRY() \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT() \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

void deleteCommand(Command *cmd)
{
  if (cmd != nullptr)
  {
    for (int i = 0; i < cmd->num_of_args; i++)
    {
      free(cmd->args[i]);
    }
    free(cmd->cmd_line);
    cmd->pid = -1;
    delete (cmd);
  }
}

bool isNumber(const char *str)
{
  if ((str == NULL) || (str[0] == '\0') || strcmp(str, " ") == 0)
  {
    return false;
  }
  for (int i = 0; str[i] != '\0'; i++)
  {
    if (std::isdigit(str[i]) == 0 && str[i] != ' ')
    {
      if (i == 0 && str[i] == '-' && strlen(str) > 1)
      {
        continue;
      }
      return false;
    }
  }
  return true;
}

string _ltrim(const std::string &s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args)
{
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for (std::string s; iss >> s;)
  {
    args[i] = (char *)malloc(s.length() + 1);
    memset(args[i], 0, s.length() + 1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line)
{
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

// static void removeBGsignString(std::string *line){
//   unsigned int idx = line->find_last_not_of(WHITESPACE);
//    if (idx == string::npos)
//   {
//     return;
//   }
//   // if the command line does not end with & then return
//   if (line[idx] != "&")
//   {
//     return;
//   }
//   line[idx] = ' ';
//   // truncate the command line string up to the last non-space character
//   line[line->find_last_not_of(WHITESPACE, idx) + 1] = "";
// }

void _removeBackgroundSign(char *cmd_line)
{
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos)
  {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&')
  {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

//**************************************
// command constructor
//**************************************

Command::Command(const char *cmd_line)
{
  for(int i = 0 ; i<COMMAND_MAX_ARGS + 1; i++){
    args[i] =NULL;
  }
  this->cmd_line = (char *)malloc(strlen(cmd_line) + 1);
  strcpy(this->cmd_line, cmd_line);
  this->num_of_args = _parseCommandLine(cmd_line, this->args);
}

//**************************************
// builtin commands part
//**************************************

BuiltInCommand::BuiltInCommand(const char *cmd_line, JobsList *jobs) : Command(cmd_line), jobs(jobs)
{
  for (int i = 0; i <= COMMAND_MAX_ARGS; i++)
  {
    if ((args[i] != NULL) && (args[i][0] != '\0'))
    {
      _removeBackgroundSign(args[i]);

      if ((strcmp(args[i], " ") == 0) && (i != COMMAND_MAX_ARGS))
      {
        for (int j = i; j < COMMAND_MAX_ARGS; j++)
        {
          args[j] = args[j + 1];
        }
        args[COMMAND_MAX_ARGS] = (char *)"";
        num_of_args--;
        break;
      }
    }
  }
  is_builtIn = true;
  cmd_status = FG;
  deleteCommand(this->jobs->FGcmd);
  this->jobs->job_id_of_FG = -1;
  this->jobs->FGcmd = this;
}

void BuiltInCommand::executePipe()
{
  this->execute();
}

//**************************************
// RedirectionCommand commands part
//**************************************

static void redirectionTrim(string& cmd, string *cmd1, string *file, bool *override)
{
  cmd.erase(remove(cmd.begin(), cmd.end(), '&'), cmd.end());
  int red_location;
  if (cmd.find(">>") != string::npos)
  {
    *override = false;
    red_location = cmd.find(">>");
  }
  else if (cmd.find(">") != string::npos)
  {
    *override = true;
    red_location = cmd.find(">");
  }

  *cmd1 = _trim(cmd.substr(0, red_location));            //trim command
  int size_of_red = *override ? 1 : 2;                   //check which kind of pipe, add chars accordingly
  *file = _trim(cmd.substr(red_location + size_of_red)); //trim file
  //removeBGsignString(file);
}

RedirectionCommand::RedirectionCommand(const char *cmd_line, JobsList *jobs) : Command(cmd_line), jobs(jobs)
{
  string cmd=string(cmd_line);
  redirectionTrim(cmd, &cmd1_line, &file_name, &override);
}

void RedirectionCommand::execute()
{
  int O_flag = override ? O_TRUNC : O_APPEND;

  int stdoutFd = dup(1);                                                         // stdout
  CALL_SYS(close(1), "close");                                                   // stdout
  int file = open((char *)file_name.c_str(), O_WRONLY | O_CREAT | O_flag, 0777); //does append or overwrites
  if (file != -1)
  {
    SmallShell::getInstance().executeCommand(cmd1_line.c_str());
    CALL_SYS(close(file), "close");
    CALL_SYS(dup2(stdoutFd, 1), "dup2"); // stdout
    CALL_SYS(close(stdoutFd), "close");
    return;
  }
  else
  {
    perror("smash error: open failed");
  }

  CALL_SYS(dup2(stdoutFd, 1), "dup2"); // stdout
  CALL_SYS(close(stdoutFd), "close");
  return;
}

//**************************************
// pipe commands part
//**************************************

static void pipeTrim(const char *cmd_line, string *cmd1, string *cmd2, bool *regular_pipe)
{
  string cmd = string(cmd_line);
  int pipe_location;
  if (cmd.find("|&") != string::npos)
  {
    *regular_pipe = false;
    pipe_location = cmd.find("|&");
  }
  else if (cmd.find("|") != string::npos)
  {
    *regular_pipe = true;
    pipe_location = cmd.find("|");
  }

  *cmd1 = _trim(cmd.substr(0, pipe_location));             //trim first command
  int size_of_pipe = *regular_pipe ? 1 : 2;                //check which kind of pipe, add chars accordingly
  *cmd2 = _trim(cmd.substr(pipe_location + size_of_pipe)); //trim second command
}

PipeCommand::PipeCommand(const char *cmd_line, JobsList *jobs) : Command(cmd_line), jobs(jobs)
{
  pipeTrim(cmd_line, &cmd1_line, &cmd2_line, &regular_pipe);
}

void PipeCommand::execute()
{
  int out = regular_pipe ? 1 : 2;
  int in = 0;
  int fd[2];
  CALL_SYS(pipe(fd), "pipe");
  pid_t cmd1_pid;
  pid_t cmd2_pid;
  cmd1_pid = fork();
  if (cmd1_pid == -1)
  {
    perror("smash error: fork failed");
    return;
  }
  if (cmd1_pid == 0)
  {
    // first child
    SmallShell &smash = SmallShell::getInstance();
    smash.isParent = false;
    CALL_SYS(setpgrp(), "setpgrp");
    CALL_SYS(dup2(fd[1], out), "dup2"); //dup2(fd[1], out);
    CALL_SYS(close(fd[0]), "close");    //close(fd[0]);
    CALL_SYS(close(fd[1]), "close");    //close(fd[1]);
    Command *cmd1 = smash.CreateCommand(cmd1_line.c_str());
    cmd1->executePipe();
    exit(0);
  }
  else //shell
  {
    cmd2_pid = fork();
    if (cmd2_pid == -1)
    {
      perror("smash error: fork failed");
      return;
    }
    if (cmd2_pid == 0)
    {
      // second child
      SmallShell &smash = SmallShell::getInstance();
      smash.isParent = false;
      CALL_SYS(setpgrp(), "setpgrp");
      CALL_SYS(dup2(fd[0], in), "dup2"); //dup2(fd[0], in);
      CALL_SYS(close(fd[0]), "close");   //close(fd[0]);
      CALL_SYS(close(fd[1]), "close");   //close(fd[1]);
      Command *cmd2 = smash.CreateCommand(cmd2_line.c_str());
      cmd2->executePipe();
      exit(0);
    }
  }
  // back to shell

  CALL_SYS(close(fd[0]), "close"); //close(fd[0]);
  CALL_SYS(close(fd[1]), "close"); //close(fd[1]);
  waitpid(cmd1_pid, NULL, WUNTRACED);
  waitpid(cmd2_pid, NULL, WUNTRACED);
}

//**************************************
// change prompt
//**************************************

ChangePromptCommand::ChangePromptCommand(const char *cmd_line, string *prompt, JobsList *jobs) : BuiltInCommand(cmd_line, jobs), prompt(prompt)
{
}

void ChangePromptCommand::execute()
{
  if (this->num_of_args == 1)
  {
    *this->prompt = "smash> ";
  }
  else
  {
    std::string new_prompt(this->args[1]);
    *this->prompt = new_prompt + "> ";
  }
}

//**************************************
// show pid
//**************************************

void ShowPidCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  cout << "smash pid is " << smash.smash_pid << endl;
}

//**************************************
// pwd
//**************************************

void GetCurrDirCommand::execute()
{
  char *dir_name = get_current_dir_name();
  if ((dir_name == NULL) || (dir_name[0] == '\0'))
  {
    perror("smash error: get_current_dir_name failed");
    return;
  }
  cout << dir_name << endl;
  free(dir_name);
}

//**************************************
// cd
//**************************************

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, string *old_dir, string *curr_dir, JobsList *jobs) : BuiltInCommand(cmd_line, jobs), old_dir(old_dir), curr_dir(curr_dir)
{
}

void ChangeDirCommand::execute()
{
  if (this->num_of_args == 1)
  {
    return;
  }
  else if (this->num_of_args > 2)
  {
    cerr << "smash error: cd: too many arguments" << endl;
    return;
  }
  else if (_trim(string(this->args[1])) == "-")
  {
    if (*old_dir == "")
    {
      cerr << "smash error: cd: OLDPWD not set" << endl;
      return;
    }
    CALL_SYS(chdir((*old_dir).c_str()), "chdir");
    std::swap(*curr_dir, *old_dir);
  }
  else
  {                                    // new path
    CALL_SYS(chdir(args[1]), "chdir"); //if (chdir(args[1]) != 0)
    *old_dir = *curr_dir;
    if (strcmp(args[1], "..") == 0)
    {
      if (*old_dir != "/")
      {
        int backslash_loc = (*old_dir).find_last_of("/");
        *curr_dir = (*old_dir).substr(0, backslash_loc);
        if (*curr_dir == "")
        {
          *curr_dir = "/";
        }
      }
    }
    else if (strcmp(args[1], ".") != 0)
    {
      *curr_dir = get_current_dir_name();
      if (curr_dir == nullptr)
      {
        perror("smash error: get_current_dir_name failed");
        return;
      }
    }
  }
}

//**************************************
// jobs+jobslist
//**************************************

JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line, jobs)
{
}

void JobsCommand::execute()
{
  (*this->jobs).printJobsList();
}

JobsList::~JobsList()
{
  deleteCommand(this->FGcmd);
  for (auto it : this->job_list)
  {
    delete it.second.cmd;
  }
}

void JobsList::printJobsList()
{
  this->removeFinishedJobs();
  for (auto &it : this->job_list)
  {
    if (it.second.status == FG && it.second.isStopped == false)
      continue;
    time_t curr_time;
    CALL_SYS(time(&curr_time), "time");
    cout << "[" << it.first << "] " << it.second.cmd->cmd_line << " : " << it.second.cmd->pid << " " << int(difftime(curr_time, it.second.create_time)) << " secs";
    string stop = (it.second.isStopped) ? " (stopped)" : "";
    cout << stop << endl;
  }
}

pid_t JobsList::getPidByJob(int job_id)
{
  return (this->getJobById(job_id))->cmd->pid;
}

void JobsList::changeStopStatus(int job_id, bool stop)
{
  this->getJobById(job_id)->isStopped = stop; // true means stop , false means BG
}

void JobsList::changeStatus(int job_id, CmdStatus status)
{
  JobsList::JobEntry *job = this->getJobById(job_id);
  job->status = status;
}

// static void removeJobFromTimeout(int job_id){
//   SmallShell &smash = SmallShell::getInstance();
// }

void JobsList::removeFinishedJobs()
{
  SmallShell &smash = SmallShell::getInstance();
  if (smash.isParent == false)
  { // forked childs will not waitpid
    return;
  }
  for (auto it = job_list.begin(); it != job_list.end();)
  {
    pid_t pid = it->second.cmd->pid;
    if (waitpid(pid, NULL, WNOHANG) > 0) //&& (WIFEXITED(status) || WIFSIGNALED(status))
    {
      for(auto& it2 : smash.timeoutList){
        if(it2.cmd == it->second.cmd){
          it2.job_id= -1;
        }
      }
      deleteCommand(it->second.cmd); //child has finished-> can remove job
      it = job_list.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

static int getMaxJobId(JobsList *jobs)
{
  int max_job_id;
  int temp_helper = 0;
  int *temp = (&temp_helper);
  JobsList::JobEntry *j = jobs->getLastJob(temp);
  max_job_id = (!j) ? 1 : *(temp) + 1;
  return max_job_id;
}

int JobsList::addJob(Command *cmd, CmdStatus status, bool isStopped, int old_job_id)
{
  removeFinishedJobs();
  int max_job_id = getMaxJobId(this);
  if (old_job_id != -1)
  {
    max_job_id = old_job_id;
  }
  JobsList::JobEntry new_job(max_job_id, cmd, isStopped, status);
  this->job_list.insert(pair<int, JobEntry>(max_job_id, new_job));

  return max_job_id;
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId)
{
  removeFinishedJobs();
  if (this->job_list.empty())
  {
    return nullptr;
  }
  *lastJobId = job_list.rbegin()->first;

  return &(this->job_list.rbegin()->second);
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId)
{
  removeFinishedJobs();
  for (auto it = this->job_list.rbegin(); it != this->job_list.rend(); ++it)
  {
    if (it->second.isStopped == true)
    {
      *jobId = it->first;
      return &it->second;
    }
  }
  return nullptr;
}

JobsList::JobEntry *JobsList::getJobById(int jobId)
{
  try
  {
    return &this->job_list.at(jobId);
  }
  catch (...)
  {
    return nullptr;
  }
  // for (auto it = this->job_list.rbegin(); it != this->job_list.rend(); ++it)
  // {
  //   if (it->first == jobId)
  //   {
  //     return &it->second;
  //   }
  // }
  // return nullptr;
}

void JobsList::removeJobById(int jobId)
{
  this->job_list.erase(jobId);
}

void JobsList::killAllJobs()
{
  cout << "smash: sending SIGKILL signal to " << this->job_list.size() << " jobs:" << endl;
  for (auto it : this->job_list)
  {
    cout << it.second.cmd->pid << ": " << it.second.cmd->cmd_line << endl;
    CALL_SYS(kill(it.second.cmd->pid, SIGKILL), "kill"); //if (kill(it.second.cmd->pid, SIGKILL) != 0)
  }
  for(auto& it : this->job_list){
    deleteCommand(it.second.cmd);
  }
  this->job_list.clear();
}

void JobsList::freeAllJobs()
{
  for(auto& it : this->job_list){
    deleteCommand(it.second.cmd);
  }
  this->job_list.clear();
}

//**************************************
// kill
//**************************************

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line, jobs)
{
  if ((args[1] == NULL) || (args[1][0] == '\0'))
  {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  char *signum_string = this->args[1] + 1;
  if (isNumber(signum_string))
  {
    signum = stoi(signum_string);
  }
}
void KillCommand::execute()
{
  int job_id;
  if (isNumber(this->args[2]))
  {
    job_id = stoi(this->args[2]);
  }
  else
  {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  if (this->num_of_args != 3 || this->args[1][0] != '-' || this->signum == -1)
  {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  JobsList::JobEntry *job_to_kill = this->jobs->getJobById(job_id);
  if (job_to_kill == nullptr)
  {
    cerr << "smash error: kill: job-id " << job_id << " does not exist" << endl;
    return;
  }
  pid_t pid_to_kill = this->jobs->getPidByJob(job_id);
  CALL_SYS(kill(pid_to_kill, this->signum), "kill"); //  if (kill(pid_to_fg, SIGCONT) != 0)
  cout << "signal number " << this->signum << " was sent to pid " << pid_to_kill << endl;
  if (this->signum == SIGCONT)
  {
    if (job_to_kill->status == FG)
    {
      jobs->FGcmd = job_to_kill->cmd;
      jobs->job_id_of_FG = job_id;
      jobs->job_list.erase(job_id);
      waitpid(pid_to_kill, NULL, WUNTRACED);
    }
    else
    {
      this->jobs->changeStopStatus(job_id, false);
    }
  }
  if (signum == SIGSTOP)
  {
    this->jobs->changeStopStatus(job_id, true);
  }
  if (this->signum == SIGKILL)
  {
    deleteCommand(job_to_kill->cmd);
    jobs->job_list.erase(job_id);
  }
  this->jobs->removeFinishedJobs();
}

//**************************************
// fg
//**************************************

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line, jobs)
{
}

void ForegroundCommand::execute()
{
  this->jobs->removeFinishedJobs();
  int job_id_to_fg;
  JobsList::JobEntry *j;
  if (this->num_of_args > 2)
  {
    cerr << "smash error: fg: invalid arguments" << endl;
    return;
  }
  else if (this->num_of_args == 1)
  { // fg the maximal job
    if (this->jobs->job_list.empty())
    { // empty job list
      cerr << "smash error: fg: jobs list is empty" << endl;
      return;
    }
    j = this->jobs->getLastJob(&job_id_to_fg);
  }
  else if (this->num_of_args == 2)
  { // fg to specific job id
    if (isNumber(args[1]))
    {
      job_id_to_fg = stoi(this->args[1]);
    }
    else
    {
      cerr << "smash error: fg: invalid arguments" << endl;
      return;
    }
    j = this->jobs->getJobById(job_id_to_fg);
  }
  if (!j)
  {
    cerr << "smash error: fg: job-id " << job_id_to_fg << " does not exist" << endl;
    return;
  }
  pid_t pid_to_fg = j->cmd->pid;
  const char *cmd_line = j->cmd->cmd_line;
  cout << cmd_line << " : " << pid_to_fg << endl; // prints the job with its pid to fg
  this->jobs->job_id_of_FG = j->job_id;
  deleteCommand(this->jobs->FGcmd);
  this->jobs->FGcmd = j->cmd;
  // delete the command from joblist
  if (j->isStopped)
  {
    CALL_SYS(kill(pid_to_fg, SIGCONT), "kill"); //  if (kill(pid_to_fg, SIGCONT) != 0)
  }
  this->jobs->removeJobById(this->jobs->job_id_of_FG);
  CALL_SYS(waitpid(pid_to_fg, NULL, WUNTRACED), "waitpid"); //  if (waitpid(pid_to_fg, NULL, WUNTRACED) == -1)
  return;
}

//**************************************
// bg
//**************************************

BackgroundCommand::BackgroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line, jobs)
{
}

void BackgroundCommand::execute()
{
  this->jobs->removeFinishedJobs();
  int job_id_to_bg;
  JobsList::JobEntry *j;
  if (this->num_of_args > 2)
  {
    cerr << "smash error: bg: invalid arguments" << endl;
    return;
  }
  else if (this->num_of_args == 1)
  { // fg the maximal stoped job
    j = this->jobs->getLastStoppedJob(&job_id_to_bg);
    if (!j)
    {
      cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
      return;
    }
  }
  else if (this->num_of_args == 2)
  { // fg to specific job id
    if (isNumber(args[1]))
    {
      job_id_to_bg = stoi(this->args[1]);
    }
    else
    {
      cerr << "smash error: bg: invalid arguments" << endl;
      return;
    }
    j = this->jobs->getJobById(job_id_to_bg);
    if (!j)
    {
      cerr << "smash error: bg: job-id " << job_id_to_bg << " does not exist" << endl;
      return;
    }
    if (j->isStopped == false)
    {
      cerr << "smash error: bg: job-id " << job_id_to_bg << " is already running in the background" << endl;
      return;
    }
  }

  pid_t pid_to_bg = j->cmd->pid;
  const char *cmd_line = j->cmd->cmd_line;
  cout << cmd_line << " : " << pid_to_bg << endl; // prints the job with its pid to fg
  j->status = BG;
  CALL_SYS(kill(pid_to_bg, SIGCONT), "kill");
  j->isStopped = false; // now it is not stopped
  return;
}

//**************************************
// Quit
//**************************************

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line, jobs)
{
}

void QuitCommand::execute()
{
  if (this->args[1] != NULL && strcmp(this->args[1], "kill") == 0)
  {
    this->jobs->killAllJobs();
  }
  else{
    this->jobs->freeAllJobs();
  }
  exit(0);
}

//**************************************
// cat command
//**************************************

CatCommand::CatCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line, jobs)
{
}

void CatCommand::execute()
{
  if (this->num_of_args < 2)
  {
    cerr << "smash error: cat: not enough arguments" << endl;
    return;
  }
  int fd, i, ch, rd;
  for (i = 1; i < this->num_of_args; i++)
  {
    rd = 0;
    fd = open(args[i], O_RDONLY);
    if (fd == -1)
    {
      perror("smash error: open failed");
      return;
    }
    rd = read(fd, &ch, 1);
    if (rd == -1)
    {
      perror("smash error: read failed");
      close(fd);
      return;
    }
    while (rd)
    {
      if (write(STDOUT_FILENO, &ch, 1) == -1)
      {
        perror("smash error: write failed");
        close(fd);
        return;
      }
      rd = read(fd, &ch, 1);
      if (rd == -1)
      {
        perror("smash error: read failed");
        close(fd);
        return;
      }
    }
    close(fd);
  }
}

//**************************************
// external commands part
//**************************************

ExternalCommand::ExternalCommand(const char *cmd_line, JobsList *job) : Command(cmd_line), jobs(job)
{
  is_builtIn = false;
  char *last_arg = this->args[this->num_of_args - 1];
  this->is_bg = false;
  if (last_arg[strlen(last_arg) - 1] == '&')
  { // bg
    this->is_bg = true;
  }
  clean_cmd = string(this->cmd_line);
}

void ExternalCommand::executePipe()
{
  _removeBackgroundSign((char *)clean_cmd.c_str());
  char *args[] = {(char *)"/bin/bash",
                  (char *)"-c",
                  (char *)clean_cmd.c_str(),
                  NULL};
  CALL_SYS(execv(args[0], args), "execv"); //execv(args[0], args);
}

void ExternalCommand::execute()
{
  this->pid = fork();
  if (pid < 0)
  {
    perror("smash error: fork failed");
    return;
  }
  _removeBackgroundSign((char *)clean_cmd.c_str());
  if (pid == 0)
  { // child proccess
    SmallShell &smash = SmallShell::getInstance();
    smash.isParent = false;
    CALL_SYS(setpgrp(), "setpgrp"); //if (setpgrp() == -1)
    char *args[] = {(char *)"/bin/bash",
                    (char *)"-c",
                    (char *)clean_cmd.c_str(),
                    NULL};
    CALL_SYS(execv(args[0], args), "execv"); //execv(args[0], args);
  }
  else
  { //shell
    if (this->is_bg == false)
    { // fg
      deleteCommand(this->jobs->FGcmd);
      this->jobs->job_id_of_FG = -1;
      this->jobs->FGcmd = this;

      waitpid(pid, NULL, WUNTRACED);
    }
    // bg
    else
    {
      deleteCommand(this->jobs->FGcmd);
      this->jobs->job_id_of_FG = -1;
      this->jobs->FGcmd = nullptr;
      this->jobs->addJob(this, BG);
    }
  }
}

//**************************************
// Timeout part
//**************************************

TimeoutCommand::TimeoutCommand(const char *cmd_line) : Command(cmd_line) {}
void TimeoutCommand::execute()
{
  int sleep = stoi(args[1]);
  SmallShell &smash = SmallShell::getInstance();
  string new_cmd_str = "";
  for (int i = 2; i < num_of_args; i++)
  {
    new_cmd_str += args[i];
    new_cmd_str += " ";
  }
  time_t curr_time;
  CALL_SYS(time(&curr_time), "time");
  cmd = smash.CreateCommand(new_cmd_str.c_str());
  free(cmd->cmd_line);
  cmd->cmd_line = this->cmd_line;
  int job_id = getMaxJobId(&smash.jobs);

  if (_isBackgroundComamnd(new_cmd_str.c_str()) == true && cmd->is_builtIn == false)
  { // if its external + BG so it will get into the joblist
    SmallShell::TimeoutInfo cmd_info = SmallShell::TimeoutInfo(cmd, string(cmd_line), curr_time + sleep, job_id);
    smash.timeoutList.push_back(cmd_info);
  }
  else // fg command
  {
    SmallShell::TimeoutInfo cmd_info = SmallShell::TimeoutInfo(cmd, string(cmd_line), curr_time + sleep, -1);
    smash.timeoutList.push_back(cmd_info);
  }

  int min_time = 0;
  for (auto it = smash.timeoutList.begin(); it != smash.timeoutList.end();)
  {
    if (it->finish_time < min_time || min_time == 0)
    {
      min_time = it->finish_time;
    }
    ++it;
  }

  alarm(min_time - curr_time);
  cmd->execute();
}

//**************************************
// smash part
//**************************************

SmallShell::SmallShell()
{
  char *str = get_current_dir_name();
  if ((str == NULL) || (str[0] == '\0'))
  {
    perror("smash error: get_current_dir_name failed");
    throw(-1);
  }
  curr_dir = str;
  last_dir = "";
  free(str);
  smash_pid = getpid();
  if (smash_pid < 0)
  {
    perror("smash error: getpid failed");
  }
}

SmallShell::~SmallShell()
{
  // TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line)
{
  // For example:

  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  if (cmd_s.find("|") != string::npos)
  {
    return new PipeCommand(cmd_line, &this->jobs);
  }
  if (cmd_s.find(">") != string::npos)
  {
    return new RedirectionCommand(cmd_line, &this->jobs);
  }
  if (firstWord.compare("timeout") == 0 || firstWord.compare("timeout&") == 0)
  {
    return new TimeoutCommand(cmd_line);
  }
  if (firstWord.compare("cat") == 0 || firstWord.compare("cat&") == 0)
  {
    return new CatCommand(cmd_line, &this->jobs);
  }
  if (firstWord.compare("chprompt") == 0 || firstWord.compare("chprompt&") == 0)
  {
    return new ChangePromptCommand(cmd_line, &this->prompt, &this->jobs);
  }
  else if (firstWord.compare("showpid") == 0 || firstWord.compare("showpid&") == 0)
  {
    return new ShowPidCommand(cmd_line, &this->jobs);
  }
  else if (firstWord.compare("pwd") == 0 || firstWord.compare("pwd&") == 0)
  {
    return new GetCurrDirCommand(cmd_line, &this->jobs);
  }
  else if (firstWord.compare("cd") == 0 || firstWord.compare("cd&") == 0)
  {
    return new ChangeDirCommand(cmd_line, &this->last_dir, &this->curr_dir, &this->jobs);
  }
  else if (firstWord.compare("jobs") == 0 || firstWord.compare("jobs&") == 0)
  {
    return new JobsCommand(cmd_line, &this->jobs);
  }
  else if (firstWord.compare("kill") == 0 || firstWord.compare("kill&") == 0)
  {
    return new KillCommand(cmd_line, &this->jobs);
  }
  else if (firstWord.compare("fg") == 0 || firstWord.compare("fg&") == 0)
  {
    return new ForegroundCommand(cmd_line, &this->jobs);
  }
  else if (firstWord.compare("bg") == 0 || firstWord.compare("bg&") == 0)
  {
    return new BackgroundCommand(cmd_line, &this->jobs);
  }
  else if (firstWord.compare("quit") == 0 || firstWord.compare("quit&") == 0)
  {
    return new QuitCommand(cmd_line, &this->jobs);
  }
  else
  {
    return new ExternalCommand(cmd_line, &this->jobs);
  }

  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line)
{
  string cmd_s = _trim(string(cmd_line));
  if (cmd_s == "")
    return;
  Command *cmd = CreateCommand(cmd_line);
  cmd->execute();
  if (strcmp(cmd->args[0], "timeout") == 0)
  {
    for (int i = 0; i < cmd->num_of_args; i++)
    {
      free(cmd->args[i]);
    }
    delete (cmd);
  }
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}