/* Fill in your Name and GNumber in the following two comment fields
 * Name: Isaac Smith
 * GNumber: G01054048
 */

#include "logging.h"
#include "shell.h"

/* Constants */
static const char *shell_path[] = {"./", "/usr/bin/", NULL};
static const char *built_ins[] = {"quit", "help", "kill", "jobs", "fg", "bg", NULL};

/* Feel free to define additional functions and update existing lines */
typedef struct _job { // Linked list structure to hold BG jobs
  int job_id; // Inidcates no job 
  int pid;
  int state;
  char *cmd;
  struct _job * next;
}Job;

/* STATES
 * 0 - created
 * 1 - wait
 * -1 terminated
 */

Job bg_jobs;
Job fg_job;

/* Function Decs */
void parse(char *cmd_line, char *argv[], Cmd_aux *aux);
void check_built_ins(char * cmd, char *args[], Cmd_aux *aux);
void run_program(char * cmd, char * args[], Cmd_aux *aux);
int check_file(char * filename);
void io_redirect(char * filename);
int switch_job(int pid, int mode);
void remove_bg_job(int pid);
void print_jobs();
void file_io(char * filename, int mode);
char* get_cmd(int pid);
void quit();
void help();


/* SIG HANDLER FUNCS */
void sig_handler(int signo) {
  
  int pid = getpid();
  int job_type; // FG == 1, BG == 0;
 
  if (fg_job.pid == pid) job_type = 1; 
  else job_type = 0;

  // CTRL - C
  if (signo == SIGINT) {
    kill(SIGINT, pid);
    log_ctrl_c();
    return;
  }

  // CTRL - Z
  else if (signo == SIGTSTP) {
    if (fg_job.state == 0) { 
      fg_job.state = 1; // paused 
      switch_job(pid, 1); // switches to BG job
      log_ctrl_z();
      log_job_fg_stopped(pid, get_cmd(pid));
      kill(pid, SIGTSTP);  
      return;
    }
    // Runs if background job
    else {
      kill(SIGCONT, pid);
      return;
    }
  }
  else if (signo == SIGCONT) {
    kill(SIGCONT, pid);
    if (job_type == 1) log_job_fg_cont(pid, fg_job.cmd);
    else log_job_bg_cont(pid, get_cmd(pid));
    return;
  }
}


/* main */
/* The entry of your shell program */
int main() 
{

    char cmdline[MAXLINE]; /* Command line */
    bg_jobs.job_id = -1;
    fg_job.job_id = -1; 
  
    /* Intial Prompt and Welcome */
    log_prompt();
    log_help();

    /* Shell looping here to accept user command and execute */
    while (1) {
 
        char *argv[MAXARGS]; /* Argument list */
        Cmd_aux aux; /* Auxilliary cmd info: check shell.h */
   
	/* Print prompt */
  	log_prompt();

        /* SIGNAL HANDLING
        // SIGINT - Terminate - CTRL C
        // SIGTSTP - Suspend Process - CTRL Z
        */
    
        struct sigaction sig_act;
        sig_act.sa_handler = sig_handler;

        // Setup blocked signal set
        sigset_t mask;
        sigaddset(&mask, SIGCHLD);
        sig_act.sa_mask = mask;

        sigaction(SIGTSTP, &sig_act, NULL);
        sigaction(SIGINT, &sig_act, NULL);

	/* Read a line */
	// note: fgets will keep the ending '\n'
	if (fgets(cmdline, MAXLINE, stdin)==NULL)
	{
	   	if (errno == EINTR)
			continue;
	    	exit(-1); 
	}

	if (feof(stdin)) {
	    	exit(0);
	}

	/* Parse command line */
    	cmdline[strlen(cmdline)-1] = '\0';  /* remove trailing '\n' */
	parse(cmdline, argv, &aux);		

	/* Evaluate command */
	/* add your implementation here */
    } 
}

/* end main */

/* required function as your staring point; 
 * check shell.h for details
 */

void parse(char *cmd_line, char *argv[], Cmd_aux *aux) {

  int i = 0; 
  char *inputs;
  char *args[MAXARGS - 1]; 
  char *cmd;

  /* Parsing tokens*/
  inputs = strtok(cmd_line, " ");
  cmd = inputs;
  inputs = strtok(NULL, " ");

  while (inputs != NULL) {   
    
    // Piping in
    if (strcmp(inputs, "<") == 0) {
      inputs = strtok(NULL, " "); // Gets next token
      if (inputs != NULL) aux -> in_file = inputs;

      // Open File, redirect IO
      check_file(aux -> in_file);
      file_io(aux -> in_file, 1);

      args[i] = inputs;
      inputs = strtok(NULL, " ");
      i++;
      continue;
    }
    
    // Pipping out
    if (strcmp(inputs, ">>") == 0) {
      inputs = strtok(NULL, " "); // Gets next token
      if (inputs != NULL) aux -> out_file = inputs;
      
      // Check file validity 
      check_file(aux -> out_file); // Checks file validity
      file_io(aux -> out_file, 2);     
 
      // Update args
      args[i] = inputs;
      inputs = strtok(NULL, " ");
      i++;
      continue;
    }

    // Background &
    if (strcmp(inputs, "&") == 0) {
      aux -> is_bg = 1;
      inputs = strtok(NULL, " ");
      i++;
      continue;
    }
    
    args[i] = inputs;
    inputs = strtok(NULL, " ");
    i++;
  }

  // Runs built in functions as needed 
  check_built_ins(cmd, args, aux);
}


/* Run built in commands */
void check_built_ins(char * cmd, char * args[], Cmd_aux *aux) {
  
  // QUIT
  if (strcmp(cmd, built_ins[0]) == 0) {
    log_quit();
    exit(0);
  }
 
  // HELP
  if (strcmp(cmd, built_ins[1]) == 0) {
    log_help();
    return;
  }
  
  if (strcmp(cmd, built_ins[2]) == 0) {
    int x = atoi(args[0]); // SIGNAL
    int y = atoi(args[1]); // PID
    kill(x, y);
    log_kill(x, y);
    return;
  } 
 
  // PRINT JOBS
  if (strcmp(cmd, built_ins[3]) == 0) {
    printf("PRINT JOBS\n");
    printf("Background Jobs: \n");
    print_jobs(bg_jobs);
    printf("Foreground Job: \n");
    print_jobs(fg_job);
    return; 
  }

  // FG
  if (strcmp(cmd, built_ins[4]) == 0) {
    int x = atoi(args[1]);
    switch_job(x, 1);
    return;
  }
  
  // BG
  if (strcmp(cmd, built_ins[5]) == 0) {
    int x = atoi(args[1]);
    switch_job(x, 0);
    return;
  }

  // Run program
  else {
    run_program(cmd, args, aux);
    return;
  }
}


/* Create new process */
void run_program(char * cmd, char * args[], Cmd_aux *aux) {

  int pid;
  Job *new;
  Job *temp;
  new = malloc(sizeof(new));
  char * argv[MAXARGS];
  char filepath[100];
  int path_v;

  // Check file
  path_v = check_file(cmd);
 
  // Error checking
  if (path_v == -1) {
    log_file_open_error(cmd);
    return;
  }
  
  // Create argv 
  strcpy(filepath, shell_path[path_v]);
  strcat(filepath, cmd);
  argv[0] = filepath;


  int i = 0;
  while (args[i] != NULL) {
    argv[i + 1] = args[i];
    i++;
  } 

  /* Child Executed Code */
  if ((pid = fork()) == 0) { 
    pid = getpid();
    
    
    new -> pid = pid;
    new -> state = 0; // Created
    new -> cmd = cmd;
    
    /* BACKGROUND */
    if (aux -> is_bg == 1) {
      
      temp = &bg_jobs;
      
      // Adds to bg_jobs list
      if (temp -> job_id == -1) {
        new -> job_id = 1;
        temp = new;
      }
      else {
        while (temp -> next != NULL) {temp = temp -> next;}
        new -> job_id = ((temp -> job_id) + 1);
        temp -> next = new;
      }
      log_start_bg(pid, cmd);   
    }

    /* FOREGROUND */
    else {
      temp = &fg_job;
      temp -> job_id = 0;
      temp = new; // Updates fg_job
    
      /* Run program with args */
      log_start_fg(pid, cmd);
    } 

    /* Run program with args */
    if (execv(argv[0], argv) == -1) {
      log_command_error(cmd);
      new -> state = -1;
      exit(1);
    }
    else {
      new -> state = -1;
      execv(argv[0], argv);
      exit(0);
    }
  }
 
  /* Parent */
  else {
    int status;
    wait(&status);
    new -> state = -1; 
    if (aux -> is_bg == 1) {
      log_job_bg_term(pid, cmd);
      remove_bg_job(pid);
    }
    else {log_job_fg_term(pid, cmd);}
  }
  return;
}



/* Checks file for validity, throws errors */
int check_file(char * filename) {

  // O_RDONLY - read only
  // O_WRONLY - write only
  // O_RDWR - read/write
  
  char path1[100];
  char path2[100];
  
  strcpy(path1, shell_path[0]); 
  strcat(path1, filename);
  strcpy(path2, shell_path[1]);
  strcat(path2, filename);
  
  // Returns 0 if selecting path w ./
  if (open(path1, O_RDONLY) != -1) return 0;
  else if (open(path1, O_WRONLY) != -1) return 0;
  else if (open(path1, O_RDWR) != -1) return 0;
  
  // Returns 1 if selecting path w /usr/bin/
  if (open(path2, O_RDONLY) != -1) return 1;
  else if (open(path2, O_WRONLY) != -1) return 1;
  else if (open(path2, O_RDWR) != -1) return 1;
  
  // Path not found
  return -1;
}
      

/* Switches Job Type */
int switch_job(int pid, int mode) {

  // 1 - indicates FG -> BG switch
  // 0 - indicates BG -> FG switch

  Job *temp, *travs;

  // Mode 0 BG -> FG
  if (mode == 0) {
    temp = &bg_jobs;
    while (temp -> next != NULL) {
      if (temp -> pid == pid) {
        travs = &fg_job;
        travs = temp;
        return 1;
      }
    }
  }

  // Mode 1 FG -> BG
  else {
    temp = &fg_job;
    travs = &bg_jobs;
    
    while (travs -> next != NULL) {travs = travs -> next;}
    travs -> next = temp;
    return 1;
  }
    
  return -1;
}


/* Print running list of jobs */
void print_jobs() {
  
  Job *temp;
  temp = &bg_jobs;
  if (temp -> job_id == -1) printf("No active jobs\n");
  else {
    if (temp -> next == NULL) log_job_details(temp -> job_id, (int) temp -> pid, temp -> state, temp -> cmd);
    while (temp -> next != NULL) {
      log_job_details(temp -> job_id, (int) temp -> pid, temp -> state, temp -> cmd);
      temp = temp -> next;
    }
  }
  return;
}


/* Removes a bg job */
void remove_bg_job(int pid) {

  Job *prev, *temp;
  temp = &bg_jobs;
  
  while (temp -> next != NULL) {
    if (temp -> pid == pid) prev -> next = temp -> next;
    prev = temp;
    temp = temp -> next;
  }
  return;
}


void file_io(char * filename, int mode) {

  int fd;
  char filepath[100];

  // Mode 1 - redirect input
  // Mode 2 - redirect output

  // Input file
  if (check_file(filename) == 1) {
    strcpy(filepath, shell_path[0]);
    strcat(filepath, filename);
  }
  // Output file
  if (check_file(filename) == 0) {
    strcpy(filepath, shell_path[1]);
    strcat(filepath, filename);
  }
  // Error handeling
  else if (check_file(filename) == -1) {
    log_file_open_error(filename);
    return;
  }

  printf("FILEPATH FROM INSIDE FILE_IO: %s\n", filepath);

  // Redirect input
  if (mode == 1) {
    fd = open(filepath, O_RDONLY); 
    dup2(fd, 0);
    close(fd);
  }
  else if (mode == 2) {
    fd = open(filepath, O_WRONLY);
    dup2(fd, 1);
  }
}

char* get_cmd(int pid) {

  Job *temp;
  temp = &bg_jobs;
 
  while (temp -> next != NULL) {
    if (temp -> pid == pid) return temp -> cmd;
    temp = temp -> next;
  }
  return NULL;
}


/* Quit */
void quit() {
  quit();
  return;
}

/* Help */
void help() {
  help();
  return;
}




