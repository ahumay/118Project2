#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
void ehandler(char indicator, int errorCode);
int main(int argc, char ** argv){
  if (argc < 4){
    ehandler('m', -1);
  }

  string HOSTNAME = argv[1];
  int PORTNUM = stoi(argv[2]);
  if (PORTNUM <= 1023){
    ehandler('p', -1);
  }
  const char * FILENAME = argv[3];

  int sockFD;
  struct sockaddr_in sa;
  int bytes_sent;
  char buffer[200];
 
  strcpy(buffer, "hello world!");
 
  /* create an Internet, datagram, socket using UDP */
  sockFD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockFD == -1) {
      /* if socket failed to initialize, exit */
      printf("Error Creating Socket");
      exit(EXIT_FAILURE);
  }
 
  /* Zero out socket address */
  memset(&sa, 0, sizeof sa);
  
  /* The address is IPv4 */
  sa.sin_family = AF_INET;
 
   /* IPv4 adresses is a uint32_t, convert a string representation of the octets to the appropriate value */
  sa.sin_addr.s_addr = inet_addr("127.0.0.1");
  
  /* sockets are unsigned shorts, htons(x) ensures x is in network byte order, set the port to 7654 */
  sa.sin_port = htons(PORTNUM);
 
  bytes_sent = sendto(sockFD, buffer, strlen(buffer), 0,(struct sockaddr*)&sa, sizeof sa);
  if (bytes_sent < 0) {
    printf("Error sending packet: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
 
  close(sockFD); /* close the socket */
  return 0;
}

void ehandler(char indicator, int errorCode){
  if (errorCode < 0){
    if (indicator == 'a'){
      cerr << "ERROR: Unknown or invalid argument.\n" << endl;
    }
    if (indicator == 'c'){
      cerr << "ERROR: Failure connect()ing to server.\n" << endl;
    }
    if (indicator == 'm'){
      cerr << "ERROR: Missing argument.\n" << endl;
    }
    if (indicator == 'p'){
      cerr << "ERROR: Invalid port.\n" << endl;
    }
    if (indicator == 'h'){
      cerr << "ERROR: Invalid hostname.\n" << endl;
    }
    if (indicator == 'w'){
      cerr << "ERROR: Failure write()ing.\n" << endl;
    }
    if (indicator == 'r'){
      cerr << "ERROR: Failure read()ing.\n" << endl;
    }
    if (indicator == 'l'){
      cerr << "ERROR: Failure trying to poll().\n" << endl;
    }
    if (indicator == 't'){
      cerr << "ERROR: Timeout.\n" << endl;
    }
    if (indicator == 's'){
      cerr << "ERROR: Sending SYN.\n" << endl;
    }

    exit(1);
  }
}