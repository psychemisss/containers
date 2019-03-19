#include <iostream>
#include <sched.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

char* stack_memory(){
     const int stack_size = 65536;
     auto *stack = new (std::nothrow) char[stack_size];

     if(stack == nullptr){
             printf("Can't allocate memeory");
             exit(EXIT_FAILURE);
     }
     return stack+stack_size;
}

#define CGROUP_FOLDER "/sys/fs/cgroup/pids/container/"
#define concat(a,b) (a"" b)

void write_rule(const char* path, const char* value){
        int fp = open(path, O_WRONLY | O_APPEND);
        write(fp, value, strlen(value));
        close(fp);
}

void limit_process_creation(){
        mkdir(CGROUP_FOLDER, S_IRUSR | S_IWUSR);
        const char* pid = std::to_string(getpid()).c_str();
        write_rule(concat(CGROUP_FOLDER, "cgroup.procs"), pid);
        write_rule(concat(CGROUP_FOLDER, "notify_on_release"), "1");
        write_rule(concat(CGROUP_FOLDER, "pids.max"), "5");
}

template <typename... P>
int run(P... params){
     char *args[] = {(char *)params..., (char *)0};
     execvp(args[0], args);
}

//for setting up default enviroment I created a root folder and downloaded a basic var-system
//curl -Ol http://nl.alpinelinux.org/alpine/v3.7/releases/x86_64/alpine-minirootfs-3.7.0-x86_64.tar.gz
//and I can't unpack it by this comand   tar -xvf alpine-minirootfs-3.7.0-x86_64.tar.gz
void setup_variables(){
     clearenv();
     setenv("TERM", "xterm-256color", 0);
     setenv("PATH", "/bin/:/sbin/:usr/bin:/usr/sbin", 0);
}

void setup_root(const char* folder){
      chroot(folder);
      chdir("/");
}

template <typename Function>
void clone_process(Function&& function, int flags){
        auto pid = clone(function, stack_memory(), flags, 0);
        
        wait(nullptr);
}


int jail(void *args){
    printf("DEBUG: Current process [%s] PID = %d\n", "jail", getpid());
    limit_process_creation();
    sethostname("container", 10);
    setup_variables();
    setup_root("./root");

    mount("proc", "/proc", "proc", 0, 0);
    auto runThis = [](void *args) -> int {run("/bin/sh"); };
    clone_process(runThis, SIGCHLD);
    
    umount("/proc");
    return EXIT_SUCCESS;
}

int main(int argc, char **argv){
    printf("DEBUG: Current process [%s] PID = %d\n", "main", getpid());
    limit_process_creation();
    clone_process(jail, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD);

    return EXIT_SUCCESS;
}