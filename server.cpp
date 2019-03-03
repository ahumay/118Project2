#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <signal.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/time.h>
#include <poll.h>
#include <fcntl.h>
#include <pthread.h>
using namespace std;
void ehandler(char indicator, int errorCode);
int main(int argc, char ** argv){
  // check the arguments
  if (argc < 3) {
    ehandler('m', -1);
  }

  int PORTNUM = stoi(argv[1]);
  if (PORTNUM <= 1023){
    ehandler('p', -1);
  }
  string FILEDIR = argv[2];
  int CONNECTION_ID = 1;

  // set up the directory 
  char * directoryPtr = argv[2];
  directoryPtr++;
  mkdir(directoryPtr, 0777);

  int sockFD;
  struct sockaddr_in sa; 
  char buffer[1024];
  ssize_t recsize;
  socklen_t fromlen;

  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  sa.sin_port = htons(PORTNUM);
  fromlen = sizeof sa;

  sockFD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (bind(sockFD, (struct sockaddr *)&sa, sizeof sa) == -1) {
    perror("error bind failed");
    close(sockFD);
    exit(EXIT_FAILURE);
  }

  for (;;) {
    recsize = recvfrom(sockFD, (void*)buffer, sizeof buffer, 0, (struct sockaddr*)&sa, &fromlen);
    if (recsize < 0) {
      fprintf(stderr, "%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    printf("recsize: %d\n ", (int)recsize);
    sleep(1);
    printf("datagram: %.*s\n", (int)recsize, buffer);
  }
}

void ehandler(char indicator, int errorCode){
  if (errorCode < 0){
    if (indicator == 'a'){
      cerr << "ERROR: Unknown or invalid argument.\n" << endl;
    }
    if (indicator == 'b'){
      cerr << "ERROR: Failure to bind().\n" << endl;
    }
    if (indicator == 'c'){
      cerr << "ERROR: Failure connect()ing to server.\n" << endl;
    }
    if (indicator == 'd'){
      cerr << "ERROR: Failure to accept() a connection.\n" << endl;
    }
    if (indicator == 'm'){
      cerr << "ERROR: Missing argument.\n" << endl;
    }
    if (indicator == 'p'){
      cerr << "ERROR: Invalid port (must be above 1023).\n" << endl;
    }
    if (indicator == 'w'){
      cerr << "ERROR: Failure write()ing.\n" << endl;
    }
    if (indicator == 'r'){
      cerr << "ERROR: Failure read()ing.\n" << endl;
    }
    if (indicator == 's'){
      cerr << "ERROR: Failure making a socket() on the client.\n" << endl;
    }
    if (indicator == 'l'){
      cerr << "ERROR: Failure trying to poll().\n" << endl;
    }
    if (indicator == 'i'){
      cerr << "ERROR: Failure to listen().\n" << endl;
    }    
    if (indicator == 't'){
      cerr << "ERROR: Timeout.\n" << endl;
    }
    exit(1);
  }
}