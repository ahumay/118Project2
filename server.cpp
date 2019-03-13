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
#include <inttypes.h>
#include <fcntl.h>
#include <set>

#include "packet.h"

void ehandler(char indicator, int errorCode);
void sighandler(int num); 

struct client_struct{
  Packet* last_packet;
  struct timeval* last_received;
  std::fstream* file;
  uint32_t next_expected_seqNum; // last ack# sent by server
};

bool sent_fin[20];
uint32_t initial_sequenceNum = 4321;
uint32_t next_connectionID = 1;
client_struct* clients[20];
uint32_t MAX_SEQUENCE_NUMBER = 102400;

int main(int argc, char ** argv){
  //////
  // handle signals
  //////
  signal(SIGINT, sighandler);  
  signal(SIGTERM, sighandler); 
  std::set<uint32_t> activeConnections;

  //////
  // handle arguments
  //////
  if (argc < 3) {
    ehandler('m', -1);
  }

  int PORTNUM = std::stoi(argv[1]);
  if (PORTNUM <= 1023){
    ehandler('p', -1);
  }
  std::string FILEDIR = argv[2];

  // set up the directory 
  char * directoryPtr = argv[2];
  // directoryPtr++;
  mkdir(directoryPtr, 0777);

  //////
  // create UDP socket
  //////
  int sockFD;
  struct sockaddr_in sa; 
  socklen_t fromlen;

  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  sa.sin_port = htons(PORTNUM);
  fromlen = sizeof sa;

  sockFD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (bind(sockFD, (struct sockaddr *)&sa, sizeof sa) == -1) {
    close(sockFD);
    ehandler('b', -1);
  }

  // set each connection struct pointer to NULL
  int useless = 0;
  while(useless < 20) {
    clients[useless] = NULL;
    sent_fin[useless] = false;
    useless++;
  }

  //////
  // wait for packets
  //////
  while(1) {
    char buffer[524];
    int recsize;
    memset(buffer, 0, 524);
    bool droppedThisPacket = false;

    // receive packet
    recsize = recvfrom(sockFD, (void*)buffer, sizeof buffer, 0, (struct sockaddr*)&sa, &fromlen);
    ehandler('r', recsize);
    Packet * p1 = new Packet((void*) buffer, recsize);

    // drop any unknown connection packets
    if ( (p1->m_connectionID < 0) || (p1->m_connectionID > 20) || 
      ((p1->m_connectionID != 0) && (activeConnections.count(p1->m_connectionID) == 0))) {
        std::cout << "DROP " << p1->m_sequenceNum << " " << p1->m_ackNum << " " << p1->m_connectionID;
        if (p1->m_ackFlag == 1){ std::cout << " ACK"; }
        if (p1->m_synFlag == 1){ std::cout << " SYN"; }
        if (p1->m_finFlag == 1){ std::cout << " FIN"; }
        std::cout << std::endl;
        continue;
    }
    else if( ( !p1->m_synFlag  && (p1->m_sequenceNum != clients[p1->m_connectionID]->next_expected_seqNum) ) || 
    (activeConnections.count(p1->m_connectionID) > 0 && p1->m_synFlag) ) {
      std::cout << "DROP " << p1->m_sequenceNum << " " << p1->m_ackNum << " " << p1->m_connectionID;
      if (p1->m_ackFlag == 1){ std::cout << " ACK"; }
      if (p1->m_synFlag == 1){ std::cout << " SYN"; }
      if (p1->m_finFlag == 1){ std::cout << " FIN"; }
      std::cout << std::endl;

      droppedThisPacket = true;
      //continue;
    }
    else {
      // print what we received
      std::cout << "RECV " << p1->m_sequenceNum << " " << p1->m_ackNum << " " << p1->m_connectionID;
      if (p1->m_ackFlag == 1){ std::cout << " ACK"; }
      if (p1->m_synFlag == 1){ std::cout << " SYN"; }
      if (p1->m_finFlag == 1){ std::cout << " FIN"; }
      std::cout << std::endl;
    }

    ////
    // SYN packet
    ////
    if (p1->m_synFlag == 1 && !droppedThisPacket) {
      // get new connection ID
      p1->m_connectionID = next_connectionID;
      activeConnections.insert(p1->m_connectionID);
      next_connectionID++;

      // set timer
      timeval * cur = new timeval;
      gettimeofday(cur, 0);

      // save new client struct to array
      client_struct* a = new client_struct;
      a->last_packet = p1;
      a->last_received = cur;
      a->file = NULL;
      a->next_expected_seqNum = (p1->m_sequenceNum + 1);
      clients[p1->m_connectionID] = a;

      // send SYN|ACK
      Packet p2 = Packet(initial_sequenceNum, (p1->m_sequenceNum + 1), p1->m_connectionID, 1, 1, 0, NULL);
      sendto(sockFD, (char*) p2.create_network_packet(), 12, 0,(struct sockaddr*)&sa, sizeof sa);
      std::cout << "SEND " << p2.m_sequenceNum << " " << p2.m_ackNum << " " << p2.m_connectionID;
      if (p2.m_ackFlag == 1){ std::cout << " ACK"; }
      if (p2.m_synFlag == 1){ std::cout << " SYN"; }
      if (p2.m_finFlag == 1){ std::cout << " FIN"; }
      std::cout << std::endl;
    }
    ////
    // FIN packet
    ////
    else if(p1->m_finFlag && !droppedThisPacket) {
      // send FIN ACK 
      client_struct * c = clients[p1->m_connectionID];
      // CHANGED
      Packet p2 = Packet(4322 /*c->last_packet->m_ackNum*/, p1->m_sequenceNum + 1, p1->m_connectionID, 1, 0, 1, NULL);
      c->next_expected_seqNum++; // this line
      sent_fin[p1->m_connectionID] = true;
      sendto(sockFD, (char*) p2.create_network_packet(), 12, 0,(struct sockaddr*)&sa, sizeof sa); 
      std::cout << "SEND " << p2.m_sequenceNum << " " << p2.m_ackNum << " " << p2.m_connectionID;
      if (p2.m_ackFlag == 1){ std::cout << " ACK"; }
      if (p2.m_synFlag == 1){ std::cout << " SYN"; }
      if (p2.m_finFlag == 1){ std::cout << " FIN"; }
      std::cout << std::endl;
    }
    ////
    // normal packet (non SYN | FIN)
    ////
    else {
      //
      // normal packet but the sequence number doesn't match what we expect it to
      //
      if (p1->m_sequenceNum != clients[p1->m_connectionID]->next_expected_seqNum) {
        // update timer for last received packet
        timeval * cur = new timeval;
        gettimeofday(cur, 0);
        free(clients[p1->m_connectionID]->last_received);
        clients[p1->m_connectionID]->last_received = cur;

        // send back ACK with next_expected_seqNum
        client_struct * c = clients[p1->m_connectionID];
        Packet p2;
        if(!p1->m_synFlag && !p1->m_finFlag) {
          p2 = Packet((uint32_t) 4322, c->next_expected_seqNum, p1->m_connectionID, 1, 0, 0, NULL);
        }
        else if(p1->m_synFlag){
          p2 = Packet((uint32_t) 4321, c->next_expected_seqNum, p1->m_connectionID, 1, 1, 0, NULL);
        }
        else {
          p2 = Packet((uint32_t) 4322, c->next_expected_seqNum, p1->m_connectionID, 1, 0, 1, NULL);
        }
        sendto(sockFD, (char*) p2.create_network_packet(), 12, 0,(struct sockaddr*)&sa, sizeof sa);
        std::cout << "SEND " << p2.m_sequenceNum << " " << p2.m_ackNum << " " << p2.m_connectionID;
        if (p2.m_ackFlag == 1){ std::cout << " ACK"; }
        if (p2.m_synFlag == 1){ std::cout << " SYN"; }
        if (p2.m_finFlag == 1){ std::cout << " FIN"; }
        std::cout << " DUP" << std::endl;

        // throw out packet
        free(p1);
      } 
      //
      // normal packet, so we can finally save the file
      //
      else {
        if(sent_fin[p1->m_connectionID] == false) {
          // write data to file
          if (clients[p1->m_connectionID]->file == NULL){
            // create new file to write to
            std::fstream* newFile = new std::fstream;
            // new std::fstream newFile;
            std::string clientDataFilePath = "./" + FILEDIR + "/" + std::to_string(p1->m_connectionID) + ".file";
            newFile -> open(clientDataFilePath, std::fstream::in | std::fstream::out | std::fstream::app);

            clients[p1->m_connectionID]->file = newFile;
          } 
          std::fstream * fileout = clients[p1->m_connectionID]->file;
          fileout -> write(p1->m_data, (int)recsize - 12);
          flush(*fileout);

          // get new timer
          timeval* cur = new timeval;
          gettimeofday(cur, 0);

          // create new client_struct
          client_struct* a = new client_struct;
          if(!(p1->m_ackFlag)) {
            p1->m_ackFlag = 1;
            p1->m_ackNum = clients[p1->m_connectionID]->last_packet->m_ackNum;
          }
          a->last_packet = p1;
          a->last_received = cur;
          a->file = clients[p1->m_connectionID]->file;
          if ((p1->m_sequenceNum + recsize - 12) > MAX_SEQUENCE_NUMBER){
            a->next_expected_seqNum = ((p1->m_sequenceNum + recsize - 12) - MAX_SEQUENCE_NUMBER - 1);
          } else {
            a->next_expected_seqNum = (p1->m_sequenceNum + recsize - 12);
          }

          // free old client struct/timer
          free(clients[p1->m_connectionID]->last_received);
          free(clients[p1->m_connectionID]);
          clients[p1->m_connectionID] = a;
          
          // send ACK
          uint32_t newAck;
          if(p1->m_sequenceNum + ((uint32_t)recsize - 12) > MAX_SEQUENCE_NUMBER) {
            newAck = (p1->m_sequenceNum + ((uint32_t)recsize - 12) - MAX_SEQUENCE_NUMBER - 1);
          }
          else {
            newAck = p1->m_sequenceNum + ((uint32_t)recsize - 12);
          }

          // if the packet is empty, don't print an ACK
          if (recsize <= 12){ 
            continue;
          } else { 
            Packet p2 = Packet(4322, newAck, p1->m_connectionID, 1, 0, 0, NULL);
            sendto(sockFD, (char*) p2.create_network_packet(), 12, 0,(struct sockaddr*)&sa, sizeof sa);        
            std::cout << "SEND " << p2.m_sequenceNum << " " << newAck << " " << p2.m_connectionID;
            if (p2.m_ackFlag == 1){ std::cout << " ACK"; }
            if (p2.m_synFlag == 1){ std::cout << " SYN"; }
            if (p2.m_finFlag == 1){ std::cout << " FIN"; }
            std::cout << std::endl;
          }
        }
      }
    }

  }
}

void ehandler(char indicator, int errorCode){
  if (errorCode < 0){
    if (indicator == 'a'){
      std::cerr << "ERROR: Unknown or invalid argument.\n" << std::endl;
    }
    if (indicator == 'b'){
      std::cerr << "ERROR: Failure to bind().\n" << std::endl;
    }
    if (indicator == 'c'){
      std::cerr << "ERROR: Failure connect()ing to server.\n" << std::endl;
    }
    if (indicator == 'd'){
      std::cerr << "ERROR: Failure to accept() a connection.\n" << std::endl;
    }
    if (indicator == 'm'){
      std::cerr << "ERROR: Missing argument.\n" << std::endl;
    }
    if (indicator == 'p'){
      std::cerr << "ERROR: Invalid port (must be above 1023).\n" << std::endl;
    }
    if (indicator == 'w'){
      std::cerr << "ERROR: Failure write()ing.\n" << std::endl;
    }
    if (indicator == 'r'){
      std::cerr << "ERROR: Failure read()ing.\n" << std::endl;
    }
    if (indicator == 's'){
      std::cerr << "ERROR: Failure making a socket() on the client.\n" << std::endl;
    }
    if (indicator == 'l'){
      std::cerr << "ERROR: Failure trying to poll().\n" << std::endl;
    }
    if (indicator == 'i'){
      std::cerr << "ERROR: Failure to listen().\n" << std::endl;
    }    
    if (indicator == 't'){
      std::cerr << "ERROR: Timeout.\n" << std::endl;
    }
    exit(1);
  }
}

void sighandler(int num){
   std::cerr << "Signal number : " << num << " ended the server process." << std::endl;
   exit(0);  
}