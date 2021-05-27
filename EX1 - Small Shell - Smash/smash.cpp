#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"
#include <system_error>

int main(int argc, char *argv[])
{
    if (signal(SIGTSTP, ctrlZHandler) == SIG_ERR)
    {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR)
    {
        perror("smash error: failed to set ctrl-C handler");
    }
    struct sigaction sa={0};
    sa.sa_flags=SA_RESTART;
    sa.sa_handler=&alarmHandler;
    if(sigaction(SIGALRM , &sa, NULL)==-1) {
        perror("smash error: failed to set alarm handler");
    }


    //TODO: setup sig alarm handler
    try
    {
        SmallShell &smash = SmallShell::getInstance();
        while (true)
        {
            std::cout << smash.prompt;
            //change jobs for new command
            smash.jobs.removeFinishedJobs();
            deleteCommand(smash.jobs.FGcmd);
            smash.jobs.FGcmd = nullptr;
            // get new command
            std::string cmd_line;
            std::getline(std::cin, cmd_line);
            smash.executeCommand(cmd_line.c_str());
        }
    }
    catch (int e)
    {
        return e;
    }

    // JobsList list ;
    // Command* cmd = new Command("pwd");
    // list.addJob(cmd);
    // Command* cmd1 = new Command("cd dfsdf");
    // list.addJob(cmd1,true);
    // Command* cmd2 = new Command("ido");
    // list.addJob(cmd2,true);
    // Command* cmd3 = new Command("mor");
    // list.addJob(cmd3);
    // list.printJobsList();
    // list.removeJobById(1);
    // list.printJobsList();
    // int jobid1;
    // int jobid2;
    // JobsList::JobEntry* j = list.getLastJob(&jobid1);
    // JobsList::JobEntry* j2 = list.getLastStoppedJob(&jobid2);

    return 0;
}