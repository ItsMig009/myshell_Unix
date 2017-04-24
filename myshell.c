/*  COURSE      : COP 4338
  * Section     : U05
  * Semester    : Spring 2017
  * Instructor  : Caryl Rahn
  * Author      : (Miguel Jardines)
  * Lab #       : 6
  * Due date    : March 12, 2017
  *
  * Description : Extend the myshell.c program that allows pipelines and I/O redirections.
  *
  *
  *
  *
  *  I certify that the work is my own and did not consult with
  *  anyone.
  *
  *
  *                                       (Miguel Jardines)
  */

//Libraries
/*==================*/
#include <fcntl.h> /*
                    fcntl - file control is int fcntl( int fildes, int cmd, ...);
                    - It performs operations on open files.
                        = The fildes argument is a file descriptor.
                        = The avialble values for command are at <fcntl.h>
                          */

#include <stdio.h> // Standard input and output
#include <stdlib.h> /*
                      The Standard Library defines four variable types:
                        = size_t - unsigned integral type and the result of the sizeof keyword.
                        = wchar_t - an integer type of the size of a wide character constant.
                        = div_t is the structure returned by the div fun.

*/
#include <string.h> // String Manipulation
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 20
#define BUFFSIZE 1024

// Global variables to hold arguments from cmdline
char* args[MAX_ARGS]; // character pointers poiting to strings

// Temporary variables
char*  argsTemp[MAX_ARGS]; // For string manipulation storage
char* argsAfter[MAX_ARGS];

// Flags for Input and Output Redirection
int    inFlag = 0; // Reads from input if 1
int   outFlag = 0; // Outputs if 1

char *inFile = NULL; // Input file
char *outFile = NULL; // Output file (character pointer to a string)

int OutAppendFlag = 0; // For adding a line cmdline option. (Appends line to a file if 1)

// Notice of method signatures to compiler (Sorted alphabetically)
int  argCopy     (char **args, char **argsTemp, char* symb, int mode); // (1)
void execute     (char* cmdline);                                      // (2)
int  fileIOnames (char **args);                                        // (3)
int  get_args    (char* cmdline, char* args[]);                        // (4)
int  getPipeNumber  ();                                                   // (5)
int  process     (int in, int out, char **inData, char **outData);     // (6)
void resetVar    ();                                                   // (7)
void swap        (char ** args2, char **argsTemp2);                    // (8)




// (1) Separates args into two parts and copies everything after symbol to argsTemp.
int argCopy (char **args, char ** argsTemp, char *symbol, int mode){
    int i = 0;
    int j = 0;

    while(args[i] != NULL){

        // Mode 0: Compares a single symbol
        if (mode == 0) {
            if(!strcmp( args[i], symbol) ) {
              args[i] = NULL;
              i++;
              while(args[i] != NULL){
                  argsTemp[j] = args[i];
                  args[i] = NULL;
                  i++;
                  j++;
              }
              i--;

            }
            i++;
        }

		    // Mode 1: Search for one of the three symbols
        if (mode == 1) {
            if(!strcmp(args[i], ">") || !strcmp(args[i], ">>") || !strcmp(args[i], "<")) {
				        args[i] = NULL;
                i++;
				        while(args[i] != NULL){
					          argsTemp[j] = args[i];
					          args[i] = NULL;
					          i++;
					      j++;
				        }
				    i--;
          }
          i++;
        }
    }

    // Extra arguments are filled with NULL
    while (j < MAX_ARGS) {
        argsTemp[j] = NULL;
        j++;
    }

    return 0;
}

// (2) Method that handles the forking and piping
void execute(char* cmdline) {
    int nargs = get_args(cmdline, args);
    if(nargs <= 0) return;

    if(!strcmp(args[0], "quit") || !strcmp(args[0], "exit")) {
        exit(0);
    }

	  // Get number of pipes
    int pipeNum = getPipeNumber();

    int inSave;
    int outSave;
    inSave = dup(1); // Saves stdin
    outSave = dup(0); // Saves stdout

    // No pipes
    if(pipeNum == 0) {
        int pid1;
        pid1 = process(inSave, outSave, args, argsAfter);
        waitpid(pid1, NULL,0);
        resetVar();
    }

    //More than 1 pipe
    if(pipeNum > 0) {
        int x;
        int pipefd[2];
        int nextIn = 0;
        int pid2;

        for(x = 0; x < pipeNum; x++) {
            argCopy (args, argsTemp, "|", 0); // Separates args by the "|" symbol
            pipe(pipefd);

            pid2 = process(nextIn, pipefd[1], args, argsAfter);

            waitpid(pid2, NULL, 0);
            close(pipefd[1]); //C lose the Write Part

            swap(args, argsTemp);
            nextIn = pipefd[0];
        }

        if(nextIn != 0)
            dup2(nextIn, 0);

        // Inputs from previous iteration, output to stdout
        pid2 = process(nextIn, outSave, args, argsAfter);//Last iteration with stdout
        waitpid(pid2, NULL, 0);
    }

    resetVar(); // Resets variables
    dup2(inSave, 0); //Resets inputs to stdin
    dup2(outSave, 1); //Resets output to stdout
}


// (3) Set files and set flags for the symbols and files
int fileIOnames(char ** args){
    int i = 0;
    while (args[i] != NULL) {
        if(!strcmp(args[i], ">")){
            outFile = args[i+1];
            outFlag = 1;
        }

        if(!strcmp(args[i], ">>")) {
            outFile = args[i+1];
            OutAppendFlag = 1;
        }

        if(!strcmp(args[i], "<")) {
            inFile = args[i+1];
            inFlag = 1;
        }
        i++;
    }
    return 0;
}

// (4) Store cmdline into a pointer array of strings
int get_args(char* cmdline, char* args[]) {
    int i = 0;
    // if no args
    if((args[0] = strtok(cmdline, "\n\t ")) == NULL)
        return 0;

    while((args[++i] = strtok(NULL, "\n\t ")) != NULL) {
        if(i >= MAX_ARGS) {
            printf("Too many arguments!\n");
            exit(1);
        }
    }
    // Last one is always returned NULL (index i)
    return i;
}

// (5) Counts # of pipes
int getPipeNumber(){
    int i = 0;
    int pipeNum = 0;

    while(args[i] != NULL)
    {
        if(!strcmp(args[i], "|"))
        pipeNum++;
        i++;
    }

    return pipeNum;
}

// (6) Input and Output Redirections
int process (int inPipe, int outPipe, char **inData, char **outData){
    int pid;

    if ((pid = fork()) == 0)
    {
        //If input isnt stdin then set to the read part of pipe
        if (inPipe != 0)
        {
			dup2 (inPipe, 0);
			close (inPipe);
        }

        //If output isnt stdout then set to the write part of pipe
        if (outPipe != 1)
        {
			dup2 (outPipe, 1);
			close (outPipe);
        }

        int err = 0;
        int out;
        int inputRedirection;

        err = fileIOnames(inData);
        if (err != 0)
            exit(1);

        // Separates args
        if (inFlag == 1 && (outFlag == 1 || OutAppendFlag == 1))
            argCopy (inData, outData , "", 1); //In - Out
        else if (inFlag == 1 && outFlag == 0 && OutAppendFlag == 0 )
            argCopy (inData, outData,"<", 0); //Redirect standard input
        else if ((outFlag == 1 || OutAppendFlag == 1) && inFlag == 0)
        {   //Out Redirection
            if(outFlag == 1)
                argCopy(inData, outData, ">", 0); //Redirect standard output
            if(OutAppendFlag == 1)
				argCopy(inData, outData,">>", 0); //Append standard output
        }

        //Set file for output redirection
        if (outFlag == 1)
        {
            out = open(outFile, O_CREAT | O_RDWR | O_TRUNC, 0644);
            if (out < 0)
            {
                perror("Opening File");
                exit(1);
            }
        }
		//Append output
        else if (OutAppendFlag == 1)
        {
            out = open(outFile, O_CREAT | O_RDWR | O_APPEND, 0644);
            if (out < 0)
            {
                perror("Opening File");
                exit(1);
            }
        }

        //Set file for input redirection
        if (inFlag == 1)
        {
            inputRedirection = open(inFile, O_RDONLY, 0644);
            if (inputRedirection < 0)
            {
            perror("Opening File");
            exit(1);
            }
        }

        //Redirect Output
        if (outFlag == 1 || OutAppendFlag == 1)
        {
            if (dup2(out, fileno(stdout)) < 0)
            {
                perror("dup2");
                exit(1);
            }
            close (out);
        }

        //Redirect Input
        if (inFlag == 1)
        {
            if (dup2(inputRedirection, fileno(stdin)) < 0)
            {
                perror("In dup2");
                exit(1);
            }
            close (inputRedirection);
        }

        execvp(inData[0], inData);
        perror("exec failed");
        exit(-1);

    }

    if (pid < 0)
        perror("can't fork");

    close(inPipe);
    close(outPipe);
    return pid;
}

// (7) Global variables are reseted to 0/NULL
void resetVar(){
    *args = NULL;
    *argsTemp = NULL;
    *argsAfter =  NULL;
    outFlag = 0;
    OutAppendFlag = 0;
    inFlag = 0;
    inFile = NULL;
    outFile = NULL;

}

// (8) Swaps two pointer arrays
void swap(char **args2, char **argsTemp2){
    char *copyTemp[MAX_ARGS];
    int i = 0;

    while (i < MAX_ARGS)
    {
        copyTemp[i] = args2[i];
        i++;
    }

    i = 0;
    while (i < MAX_ARGS)
    {
        args2[i] = argsTemp2[i];
        i++;
    }

    i = 0;
    while (i < MAX_ARGS)
    {
        argsTemp2[i] = copyTemp[i];
        i++;
    }
}


// Main (0)
int main(int argc, char* argv []){
  char cmdline[BUFFSIZE];

  for(;;) {
    printf("COP4338$ ");
    if (fgets(cmdline, BUFFSIZE, stdin) == NULL)
    {
		perror("fgets failed");
		exit (1);
    }
    execute(cmdline);
  }
  return 0;
}
