/* Fooling around with fork and pipes
 * author : Marcin Gregorczyk (mg359198) */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define BUFFER_SIZE 4096

/* input, stack and result are variables used by conversion algorithm */
char input[BUFFER_SIZE];
char stack[BUFFER_SIZE];
char result[BUFFER_SIZE];

/* buffer = input\0stack\0result\0
   used for process communication */
char buffer[BUFFER_SIZE];

/* Prints error to stderr and terminates process */
void syserr(const char* msg) {
  fprintf(stderr, "error : %s\n", msg);
  exit(1);
}

/* Function writes a message to an opened pipe.
   Process is terminated if error appears */
void safeWrite(int pipeNumber, const char* msg, size_t len) { 
  if(write(pipeNumber, msg, len) == -1) syserr("can't write to pipe");
}

/* Function reads a message from an opened pipe
   Process is terminated if error appears
   Returns amount of bytes read */
int safeRead(int pipeNumber, char* msg, size_t len) {
  int res = read(pipeNumber, msg, len);
  if(res == -1) syserr("can't read from pipe");
  return res;
}

/* Function closes a pipe.
   Process is terminated if error appears */
void safeClose(int pipeNumber) {
  if(close(pipeNumber) == -1) syserr("close");
}

/* Adds a character at the end of buffer
   Assumes, that buffer will not overflow */
void pushBack(char* buf, char c) {
  char* end = buf + strlen(buf);
  *end = c;
  *(end+1) = '\0';
}

/* Gets priority of an arithmetic operator */
int getPriority(char c) {
  if(c == '+' || c == '-') return 0;
  if(c == '*' || c == '/') return 1;
  return 2;
}

/* Splits buffer into 3 strings : input, stack and result */
void splitBuffer() {
  char* beg = buffer;
  int len = strlen(beg);
  strcpy(input, beg);
  beg += len+1;
  len = strlen(beg);
  strcpy(stack, beg);
  beg += len+1;
  strcpy(result, beg);
}

/* Writes input, stack and result into buffer
   Returns amount of bytes written */
int buildBuffer() {
  char* beg = buffer;
  int len = strlen(input);
  int res = len + 3;
  strcpy(beg, input);
  beg += len + 1;
  len = strlen(stack);
  res += len;
  strcpy(beg, stack);
  beg += len + 1;
  strcpy(beg, result);
  res += strlen(result);
  return res;
}

/* Performs a single conversion step
   Modifies global variables: input, stack and result
   Returns -1 if input is empty */
int conversionStep() {
  char* beg = input;
  int whiteChars = 0;

  while(*beg != '\0' && *beg == ' ') {
    whiteChars++;
    beg++;
  }
  if(*beg == '\0') return -1;
  char* end = beg;
  int len = 0;
  while(*end != '\0' && *end != ' ') {
    len++;
    end++;
  }
  
  if(len > 1 || (*beg >= 'a' && *beg <= 'z') || (*beg >= '0' && *beg <= '9')) {
    //Variable or constant is directly added to the result
    int resLen = strlen(result);
    if(resLen != 0) {
      *(result + resLen) = ' ';
      resLen++;
    }
    strncpy(result + resLen, beg, len);
  }
  else if(*beg == '(') pushBack(stack, *beg);
  else if(*beg == ')') {
    //Operators are added to the result until '(' is removed from stack
    char* top = stack + strlen(stack) -1;
    while(*top != '(') {
      pushBack(result, ' ');
      pushBack(result, *top);
      *top = '\0';
      top--;
    }
    *top = '\0';
    top--;
  }
  else {
    char* top = stack + strlen(stack) -1;
    while(top != stack-1 && (*top != '(' && getPriority(*beg) <= getPriority(*top))) {
      pushBack(result, ' ');
      pushBack(result, *top);
      *top = '\0';
      top--;
    }
    pushBack(stack, *beg);
  }
  strcpy(input, input+whiteChars+len);
  return 0;
}

int main(int argc, char* argv[]) {
  
  if(argc >= 2)
    strcpy(input, argv[1]);

  if(strlen(input) == 0)
    return 0;
  
  //This variable controls main loop
  //Each process ends its loop after creating another process
  //or after finishing calculations
  int loop = 1;

  //wProcess = 0 means it's main process
  int wProcess = 0;

  int inputPipe[2]; //used for passing input, stack and result to the next process
  int resultPipe[2]; //used for passing result to the previous process
  int parentPipe;

  //Main loop
  while(loop) {

    //Create pipes
    if (pipe(inputPipe) == -1) syserr("can't create a pipe");
    if (pipe(resultPipe) == -1) syserr("can't create a pipe");
    
    switch(fork()) {
    case -1:
      
      syserr("fork error");
      
    case 0:

      wProcess = 1;
      parentPipe = resultPipe[1];
      
      safeClose(inputPipe[1]);
      safeClose(resultPipe[0]);
      
      //read input, stack and result
      safeRead(inputPipe[0], buffer, BUFFER_SIZE-1);
      safeClose(inputPipe[0]);
      splitBuffer();
      
      if(conversionStep() == -1) {

	//Moving elements from stack to result
        char* top = stack + strlen(stack) -1;
	while(top != stack-1) {
          pushBack(result, ' ');
	  pushBack(result, *top);
	  top--;
	}
	
        loop = 0;
	
	safeWrite(parentPipe, result, strlen(result));
	safeClose(parentPipe);
	
	exit(0);
      }
      break;
      
    default:

      safeClose(inputPipe[0]);
      safeClose(resultPipe[1]);

      int len = buildBuffer();
      safeWrite(inputPipe[1], buffer, len);
      safeClose(inputPipe[1]);
      
      loop = 0;

      safeRead(resultPipe[0], result, BUFFER_SIZE-1);
      safeClose(resultPipe[0]);
      
      if(wProcess) {
        safeWrite(parentPipe, result, strlen(result));
	safeClose(parentPipe);
      }
      
      if(wait(0) == -1) syserr("wait");
      
    }
  }
  
  if(!wProcess)
    printf("%s\n", result);
  
  return 0;
}
