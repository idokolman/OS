
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <map>
#include <unistd.h>
#include <time.h>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
using std::string;
using std::vector;

#define CALL_SYS(syscall, syscall_name)                \
  do                                                   \
  {                                                    \
    if ((syscall) == -1)                               \
    {                                                  \
      string str_for_perror = string("smash error: "); \
      str_for_perror += syscall_name;                  \
      str_for_perror += " failed";                     \
      perror((char *)str_for_perror.c_str());          \
      return;                                          \
    }                                                  \
  } while (0)

enum CmdStatus
{
  BG,
  FG
};

class Command
{
  // TODO: Add your data members
public:
  bool is_builtIn;
  CmdStatus cmd_status;
  char *args[COMMAND_MAX_ARGS + 1];
  int num_of_args;
  char *cmd_line;
  pid_t pid = -1;
  Command(const char *cmd_line);
  virtual ~Command(){};
  virtual void execute() = 0;
  virtual void executePipe() = 0;
  //virtual void prepare();
  //virtual void cleanup();
  // TODO: Add your extra methods if needed
};
void deleteCommand(Command *cmd);

class JobsList;

class BuiltInCommand : public Command //done
{
public:
  JobsList *jobs;
  BuiltInCommand(const char *cmd_line, JobsList *jobs);
  virtual ~BuiltInCommand() {}
  void execute(){};
  void executePipe() override;
};

class ExternalCommand : public Command //done witout &
{
public:
  bool is_bg;
  JobsList *jobs;
  string clean_cmd;
  ExternalCommand(const char *cmd_line, JobsList *jobs);
  virtual ~ExternalCommand() {}
  void execute() override;
  void executePipe() override;
};

class PipeCommand : public Command
{
  string cmd1_line;
  string cmd2_line;
  JobsList *jobs;
  bool regular_pipe; // | means regular (true), |& false
  // TODO: Add your data members
public:
  PipeCommand(const char *cmd_line, JobsList *jobs);
  virtual ~PipeCommand() {}
  void execute() override;
  void executePipe(){}; // will never be applyed because there will be 1 |
};

class RedirectionCommand : public Command
{
  JobsList *jobs;
  bool override; // < means regular (true), << false (append)
  string cmd1_line;
  string file_name;

public:
  explicit RedirectionCommand(const char *cmd_line, JobsList *jobs);
  virtual ~RedirectionCommand() {}
  void execute() override;
  virtual void executePipe(){};
  //void prepare() override;
  //void cleanup() override;
};

class ChangePromptCommand : public BuiltInCommand //done
{
public:
  std::string *prompt;
  ChangePromptCommand(const char *cmd_line, std::string *prompt, JobsList *jobs);
  virtual ~ChangePromptCommand() {}
  void execute() override;
};

class ChangeDirCommand : public BuiltInCommand //done
{
public:
  string *old_dir;
  string *curr_dir;
  ChangeDirCommand(const char *cmd_line, string *old_dir, string *curr_dir, JobsList *jobs);
  virtual ~ChangeDirCommand() {}
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand //done
{
public:
  GetCurrDirCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line, jobs){};
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand //done
{
public:
  ShowPidCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line, jobs){};
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand //done
{
public:
  QuitCommand(const char *cmd_line, JobsList *jobs);
  virtual ~QuitCommand() {}
  void execute() override;
};

class JobsList
{
public:
  class JobEntry
  {
  public:
    int job_id;
    Command *cmd;
    time_t create_time;
    bool isStopped;
    CmdStatus status;
    JobEntry(int job_id, Command *cmd, bool isStopped, CmdStatus status) : job_id(job_id), cmd(cmd), isStopped(isStopped), status(status)
    {
      time(&create_time);
    };
  };
  std::map<int, JobEntry> job_list;
  Command *FGcmd;
  int job_id_of_FG = -1;
  // TODO: Add your data members
public:
  JobsList()
  {
    FGcmd = NULL;
  }; //done
  ~JobsList();
  int addJob(Command *cmd, CmdStatus status, bool isStopped = false, int old_job_id = -1); //done
  void printJobsList();                                                                     //done
  void killAllJobs();    
  void freeAllJobs();                                                                    //done
  void removeFinishedJobs();                                                                //done
  JobEntry *getJobById(int jobId);                                                          //done
  pid_t getPidByJob(int job_id);                                                            //done
  void changeStopStatus(int job_id, bool stop);                                             //done
  void changeStatus(int job_id, CmdStatus status);                                          //done
  void removeJobById(int jobId);                                                            //done
  JobEntry *getLastJob(int *lastJobId);                                                     //done
  JobEntry *getLastStoppedJob(int *jobId);                                                  //done
  // TODO: Add extra methods or modify exisitng ones as needed
};

class TimeoutCommand : public Command
{
public:
  Command *cmd;
  TimeoutCommand(const char *cmd_line);
  virtual ~TimeoutCommand() {}
  void execute() override;
  virtual void executePipe(){};
};

class JobsCommand : public BuiltInCommand
{ // done
  // TODO: Add your data members
public:
  JobsCommand(const char *cmd_line, JobsList *jobs);
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand //done
{
public:
  int signum = -1;
  KillCommand(const char *cmd_line, JobsList *jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand // done
{
public:
  ForegroundCommand(const char *cmd_line, JobsList *jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand //done
{
public:
  BackgroundCommand(const char *cmd_line, JobsList *jobs);
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class CatCommand : public BuiltInCommand
{
public:
  CatCommand(const char *cmd_line, JobsList *jobs);
  virtual ~CatCommand() {}
  void execute() override;
};

class SmallShell
{
private:
  SmallShell();
  string curr_dir;
  string last_dir;

public:
  class TimeoutInfo
  {
  public:
    Command* cmd;
    string cmd_line;
    time_t finish_time;
    int job_id;
    TimeoutInfo(Command* cmd, string cmd_line, time_t finish_time, int job_id) : cmd(cmd), cmd_line(cmd_line), finish_time(finish_time), job_id(job_id){};
  };
  vector<TimeoutInfo> timeoutList;
  JobsList jobs;
  string prompt = "smash> ";
  bool isParent = true;
  pid_t smash_pid;
  Command *CreateCommand(const char *cmd_line);
  SmallShell(SmallShell const &) = delete;     // disable copy ctor
  void operator=(SmallShell const &) = delete; // disable = operator
  static SmallShell &getInstance()             // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char *cmd_line);
  // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_