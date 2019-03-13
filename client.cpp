/*
Things to work on:
Do they need to be sequentially sending
Output in the beginning
DO we need the thing about received recently 
Retransmitting packets - based off congestion window
*/

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
#include <inttypes.h>
#include <signal.h>
#include <fstream>
#include <sys/stat.h>
#include <pthread.h>
#include <inttypes.h>
#include <fcntl.h>
#include <queue>
#include <unordered_map>

#include "packet.h"

// get new timer
timeval timeLastReceived;
timeval retransmissionTimer;
uint32_t MAX_SEQUENCE_NUMBER = 102400;

void ehandler(char indicator, int errorCode);
void sighandler(int num);
bool checkTimeExpired(timeval t1, bool is10SecTimeout);
void closeClient();
void printPacket(Packet current, bool send, int congestion_window, int SS_thresh, bool restransmit);


int main(int argc, char ** argv){
  //// Congestion Window 
  int congestion_window = 512;
  int SS_thresh = 10000;
  int byte_last_sent =12345, byte_last_acked=12345;
  int byte_last_retransmitted;
  std::queue<Packet> packets_in_network;
  std::queue<Packet> retransmit_queue;
  bool retransmiting = false;
  std::unordered_map<int,int> sizeMap;
  bool all_packets_ack = false;
  //////
  // handle signals
  //////
  signal(SIGINT, sighandler);  
  signal(SIGTERM, sighandler); 

  //////
  // handle arguments
  //////
  if (argc < 4){
    ehandler('m', -1);
  }

  // std::string HOSTNAME = argv[1];
  struct hostent* serv = gethostbyname(argv[1]);
  if(serv == NULL) {
    ehandler('h', -1);
  }


  int PORTNUM = std::stoi(argv[2]);
  if (PORTNUM <= 1023){
    ehandler('p', -1);
  }
  const char * FILENAME = argv[3];

  //////
  // create UDP socket
  //////
  int sockFD;
  struct sockaddr_in sa;
  socklen_t fromlen;

  /* create an Internet, datagram, socket using UDP */
  sockFD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockFD == -1) {
    /* if socket failed to initialize, exit */
    ehandler('z', -1);
  }
 
  /* Zero out socket address */
  memset(&sa, 0, sizeof sa);
  
  /* The address is IPv4 */
  sa.sin_family = AF_INET;
 
  /* IPv4 adresses is a uint32_t, convert a string representation of the octets to the appropriate value */
  // sa.sin_addr.s_addr = inet_addr("127.0.0.1");
  memcpy(&sa.sin_addr.s_addr, serv->h_addr, serv->h_length);
  
  /* sockets are unsigned shorts, htons(x) ensures x is in network byte order, set the port to 7654 */
  sa.sin_port = htons(PORTNUM);

  // set to non-blocking
  fcntl(sockFD, F_SETFL, O_NONBLOCK);
  // make poll struct
  struct pollfd pollstatus[1];
  pollstatus[0].events = POLLIN | POLLHUP | POLLERR | POLLNVAL | POLLOUT;
  pollstatus[0].fd = sockFD;

  //////
  // handshake
  //////
  // SYN from client
  int numSent;
  fromlen = sizeof sa;
  uint32_t sequenceNum = 12345;
  uint32_t ackNum = 0;
  uint32_t connectionID = 0;
  Packet p1 = Packet(sequenceNum, ackNum, connectionID, 0, 1, 0, NULL);
  while(1){
    poll(pollstatus, 1, 0);
    if (pollstatus[0].revents & POLLOUT){
      numSent = sendto(sockFD, (char*) p1.create_network_packet(), 12, 0,(struct sockaddr*)&sa, sizeof sa);
      printPacket(p1,true,congestion_window,SS_thresh,false);
      gettimeofday(&timeLastReceived, 0);
      gettimeofday(&retransmissionTimer, 0);
      break;
    }
  }

  // receive SYN ACK from server
  Packet p2 = Packet();
  while(1){
    checkTimeExpired(timeLastReceived, true);
    bool expired = checkTimeExpired(retransmissionTimer, false);
    poll(pollstatus, 1, 0);
    if (pollstatus[0].revents & POLLIN){
      char buffer[12];
      ssize_t recsize;
      recsize = recvfrom(sockFD, (void*)buffer, sizeof buffer, 0, (struct sockaddr*)&sa, &fromlen);
      if (recsize < 0) {
        ehandler('r', -1);
      }
      gettimeofday(&timeLastReceived, 0);
      gettimeofday(&retransmissionTimer, 0);
      //printf("Received : %d bytes\n", (int)recsize);
      p2 = Packet((void*) buffer, recsize);
      printPacket(p2,false,congestion_window,SS_thresh,false);
      break;
    }
    if (expired){
      sendto(sockFD, (char*) p1.create_network_packet(), 12, 0,(struct sockaddr*)&sa, sizeof sa);
      //std::cout << "time is " << retransmissionTimer.tv_sec << " " << retransmissionTimer.tv_usec << std::endl;
      gettimeofday(&retransmissionTimer, 0);
      printPacket(p1,true,congestion_window,SS_thresh,true);
      //std::cout << "numSent : " << numSent << std::endl;
    }
  }

  // send final handshake ACK 
  connectionID = p2.m_connectionID;
  sequenceNum = p2.m_ackNum;
  ackNum = p2.m_sequenceNum + 1;
  Packet p3 = Packet(sequenceNum, ackNum, connectionID, 1, 0, 0, NULL);
  numSent = sendto(sockFD, (char*) p3.create_network_packet(), 12, 0,(struct sockaddr*)&sa, sizeof sa);
  printPacket(p3,true,congestion_window,SS_thresh,false);
  //std::cout << "numSent : " << numSent << std::endl;

  //byte_last_sent = sequenceNum;
  //////
  // send file
  //////
  FILE * clientFile = fopen(FILENAME, "rb");

  //bool receivedRecently = false;

  //std::cout << "HANDSHAKE IS DONE" << std::endl;


  while (!all_packets_ack)
  {
    //std::cout << "In the operations" << std::endl;
    ////
    // do receive (update ack number)
    ////
    char buffer[524];
    ssize_t recsize;
    memset(buffer, 0, 524);

    checkTimeExpired(timeLastReceived, true);
    bool expired = checkTimeExpired(retransmissionTimer, false);

    //if(!packets_in_network.empty()){
    //std::cout << "The top of the packet queue has sequence num: " << packets_in_network.front().m_sequenceNum << std::endl;
    //std::cout << "The last byte that was acknowledged was: " << byte_last_acked << std::endl;}  

    // receive packet
    poll(pollstatus, 1, 0);
    if (pollstatus[0].revents & POLLIN){
      recsize = recvfrom(sockFD, (void*)buffer, sizeof buffer, 0, (struct sockaddr*)&sa, &fromlen);
      if (recsize < 0) {
        ehandler('r', -1);
      }
      gettimeofday(&timeLastReceived, 0);
      
      Packet * p1 = new Packet((void*) buffer, recsize);
      //receivedRecently = true;

      ackNum = p1->m_sequenceNum;
      byte_last_acked = p1->m_ackNum;

      // 

      //updating congestion control window
      if(congestion_window < SS_thresh)
      {
        congestion_window += 512;
        //std::cout << "Slow Start: " << congestion_window << std::endl;
      } else if (congestion_window >= SS_thresh)
      {
        congestion_window += ((512*512)/congestion_window);
        if(congestion_window > 51200)
        {
          congestion_window = 51200;
        }
        //std::cout << "Congestion Avoidance: " << congestion_window << std::endl;
      }

      printPacket(*p1,false,congestion_window,SS_thresh,false);

      if(!packets_in_network.empty())
      {
        //checking acknowledged packets
        Packet temp = packets_in_network.front();
        //std::cout << "Before new " << std::endl;
        while( temp.m_sequenceNum < byte_last_acked || (temp.m_sequenceNum - byte_last_acked > 51200))
        {
          packets_in_network.pop();
          if(!packets_in_network.empty())
          {
            temp = packets_in_network.front();
          } else
          {
            break;
          }
        }
      }

      if(feof(clientFile) && packets_in_network.empty())
        break;

    }

    ////
    // send data
    ////
    if(!(expired || retransmiting) && !feof(clientFile))
    {
      //checking bytes available to send
      int bytes_in_network = byte_last_sent - byte_last_acked;
      if(bytes_in_network < 0)
      {
        bytes_in_network = (MAX_SEQUENCE_NUMBER - byte_last_acked) + byte_last_sent;
      }
      //this is just checking standard 512
      if (bytes_in_network + 512 > congestion_window)
      {
        // std::cout << "Skipping due to congestion control: "<< std::endl << 
        // "Bytes in network: " << bytes_in_network << std::endl <<
        // "Byte last sent: " << byte_last_sent << std::endl <<
        // "Byte last acked: " << byte_last_acked << std::endl <<
        // "Congestion window: " << congestion_window << std::endl;
      }
      else {
        char fileData[512] = {0};
        int bytesRead = 0;
        bytesRead = fread(fileData, 1, 512, clientFile);
        if (bytesRead < 0){
          ehandler('r', -1);
        }
        Packet outgoingPacket;

        outgoingPacket = Packet(sequenceNum, 0, connectionID, 0, 0, 0, fileData);
        sizeMap[sequenceNum] = bytesRead + 12;
        void* netOutgoingPacket = outgoingPacket.create_network_packet();
        sendto(sockFD, (void*)netOutgoingPacket, (bytesRead + 12), 0,(struct sockaddr*)&sa, sizeof sa);
        sequenceNum += bytesRead;
        if (sequenceNum > MAX_SEQUENCE_NUMBER){
          sequenceNum = sequenceNum - MAX_SEQUENCE_NUMBER - 1;
        } 

        byte_last_sent = sequenceNum;
        // CHECKING THE SIZE OF THE PACKET BEFORE PUSHING
        if(bytesRead > 0)
        {
          packets_in_network.push(outgoingPacket);
        }
        printPacket(outgoingPacket,true,congestion_window,SS_thresh,false);
      }
    } else if (expired || retransmiting)
    {
      if(expired)
      {
        // std::cout << "We have expired and we will retransmit " << packets_in_network.size() << " packets " << std::endl << std::endl;
        //std::cout << "We have timed out and the current size of the restransmit queue is " << packets_in_network.size() << std::endl
        //<< std::endl<< std::endl<< std::endl<< std::endl<< std::endl;
        SS_thresh = congestion_window / 2;
        congestion_window = 512;
        retransmit_queue = packets_in_network;
        retransmiting = true;

        gettimeofday(&retransmissionTimer,0);

        if (!retransmit_queue.empty())
        {
          Packet current = retransmit_queue.front();
          retransmit_queue.pop();
          void* netOutgoingPacket = current.create_network_packet();
          int size = sizeMap[current.m_sequenceNum];
          sendto(sockFD, (void*)netOutgoingPacket, size , 0,(struct sockaddr*)&sa, sizeof sa);
          printPacket(current,true,congestion_window,SS_thresh,true);
          //check if this is fine
          byte_last_retransmitted = current.m_sequenceNum + size;
          if (byte_last_retransmitted > MAX_SEQUENCE_NUMBER){
              byte_last_retransmitted = byte_last_retransmitted - MAX_SEQUENCE_NUMBER - 1;
          } 
        }
      }

      int bytes_in_network = byte_last_retransmitted - byte_last_acked;
      if(bytes_in_network < 0)
      {
        bytes_in_network = (MAX_SEQUENCE_NUMBER - byte_last_acked) + byte_last_sent;
      }

      //std::cout << "We are in the retransmission phase and the current size of the restransmit queue is " << retransmit_queue.size() << std::endl
      //<< std::endl<< std::endl<< std::endl<< std::endl<< std::endl;

      while(!retransmit_queue.empty() && (bytes_in_network + (sizeMap[retransmit_queue.front().m_sequenceNum]) < congestion_window))
      {
        Packet current = retransmit_queue.front();

        retransmit_queue.pop();
        void* netOutgoingPacket = current.create_network_packet();
        int size = sizeMap[current.m_sequenceNum];
        sendto(sockFD, (void*)netOutgoingPacket, size, 0,(struct sockaddr*)&sa, sizeof sa);
        printPacket(current,true,congestion_window,SS_thresh,true);

        byte_last_retransmitted = current.m_sequenceNum + size;
        if (byte_last_retransmitted > MAX_SEQUENCE_NUMBER){
            byte_last_retransmitted = byte_last_retransmitted - MAX_SEQUENCE_NUMBER - 1;
        } 

        bytes_in_network = byte_last_retransmitted - byte_last_acked;
        if(bytes_in_network < 0)
        {
          bytes_in_network = (MAX_SEQUENCE_NUMBER - byte_last_acked) + byte_last_sent;
        }

      }

      if(retransmit_queue.empty())
      {
        retransmiting = false;
      }

    }
    //std::cout << "------------" << std::endl;
  }


  
  //////
  // FIN
  //////
  // send FIN 
  Packet p5 = Packet(sequenceNum, 0, connectionID, 0, 0, 1, NULL);
  numSent = sendto(sockFD, (char*) p5.create_network_packet(), 12, 0,(struct sockaddr*)&sa, sizeof sa);
  printPacket(p5, true, congestion_window, SS_thresh,false);
  // receive server's FIN ACK
  while(1){
    char buffer[524];
    checkTimeExpired(timeLastReceived, true);
    bool expired = checkTimeExpired(retransmissionTimer, false);

    poll(pollstatus, 1, 0);
    if (pollstatus[0].revents & POLLIN){
      int recsize = recvfrom(sockFD, (void*)buffer, sizeof buffer, 0, (struct sockaddr*)&sa, &fromlen);
      ehandler('r', recsize);

      Packet p6 = Packet((void*) buffer, recsize);
      printPacket(p6, false, congestion_window, SS_thresh,false);


      // sequenceNum = p6.m_ackNum;
      ackNum = p6.m_sequenceNum + 1;

      gettimeofday(&timeLastReceived, 0);
      gettimeofday(&retransmissionTimer, 0);
      break;
    }
    if (expired){
      sendto(sockFD, (char*) p5.create_network_packet(), 12, 0,(struct sockaddr*)&sa, sizeof sa);
      gettimeofday(&retransmissionTimer, 0);
      printPacket(p5, true, congestion_window, SS_thresh,false);
    }
  }

  // send ACK for server's FIN ACK
  Packet p7 = Packet(sequenceNum + 1, ackNum, connectionID, 1, 0, 0, NULL);
  numSent = sendto(sockFD, (char*) p7.create_network_packet(), 12, 0,(struct sockaddr*)&sa, sizeof sa);
  printPacket(p7, true, congestion_window, SS_thresh,false);

  // 2 second wait period
  while (1){
    char buffer[524];

    poll(pollstatus, 1, 0);
    if (pollstatus[0].revents & POLLIN){
      int recsize = recvfrom(sockFD, (void*)buffer, sizeof buffer, 0, (struct sockaddr*)&sa, &fromlen);
      ehandler('r', recsize);

      Packet p9 = Packet((void*) buffer, recsize);

      // resend ACK for server's FIN ACK
      if(p9.m_finFlag) {
        printPacket(p9, false, congestion_window, SS_thresh,false);


        numSent = sendto(sockFD, (char*) p7.create_network_packet(), 12, 0,(struct sockaddr*)&sa, sizeof sa);
        printPacket(p7, true, congestion_window, SS_thresh,false);
      } else { // drop all non-FIN packets
        std::cout << "DROP " << p9.m_sequenceNum << " " << p9.m_ackNum << " " << p9.m_connectionID;
        if (p9.m_ackFlag == 1){ std::cout << " ACK"; }
        if (p9.m_synFlag == 1){ std::cout << " SYN"; }
        if (p9.m_finFlag == 1){ std::cout << " FIN"; }
        std::cout << std::endl;
      }
    }

    // check if 2 sec passed
    timeval cur;
    gettimeofday(&cur, 0);
    if (cur.tv_sec - timeLastReceived.tv_sec > 2){
      close(sockFD);
      closeClient();
    }
  }

  close(sockFD);
  return 0;
}

bool checkTimeExpired(timeval t1, bool is10SecTimeout){
  if (is10SecTimeout){
    timeval cur;
    gettimeofday(&cur, 0);
    timeval result;
    timersub(&cur, &t1, &result);
    if (result.tv_sec >= 10){
      std::cout << "ERROR: No response from server" << std::endl;
      closeClient();
    }
  } else {
    //std::cout << "got in checkTimeExpired" << std::endl;
    timeval cur;
    gettimeofday(&cur, 0);
    timeval result;
    // std::cout << "cur is " << cur.tv_usec << std::endl;
    // std::cout << "old is " << t1.tv_usec << std::endl;
    //unsigned long long diff = cur.tv_usec - t1.tv_usec;
    // std::cout << "diff is : " << diff << std::endl;
    timersub(&cur, &t1, &result);

    if (result.tv_usec >= 500000){
      //std::cout << "in checkTimeExpired and 0.5 seconds have passed" << std::endl;
      return true;
    }
  }
  return false;
}

void closeClient(){
  //std::cout << "Closing" << std::endl;
  exit(1);
}

void ehandler(char indicator, int errorCode){
  if (errorCode < 0){
    if (indicator == 'a'){
      std::cerr << "ERROR: Unknown or invalid argument.\n" << std::endl;
    }
    if (indicator == 'c'){
      std::cerr << "ERROR: Failure connect()ing to server.\n" << std::endl;
    }
    if (indicator == 'm'){
      std::cerr << "ERROR: Missing argument.\n" << std::endl;
    }
    if (indicator == 'p'){
      std::cerr << "ERROR: Invalid port.\n" << std::endl;
    }
    if (indicator == 'h'){
      std::cerr << "ERROR: Invalid hostname/IP address.\n" << std::endl;
    }
    if (indicator == 'w'){
      std::cerr << "ERROR: Failure write()ing.\n" << std::endl;
    }
    if (indicator == 'r'){
      std::cerr << "ERROR: Failure read()ing.\n" << std::endl;
    }
    if (indicator == 'l'){
      std::cerr << "ERROR: Failure trying to poll().\n" << std::endl;
    }
    if (indicator == 't'){
      std::cerr << "ERROR: Timeout.\n" << std::endl;
    }
    if (indicator == 's'){
      std::cerr << "ERROR: Sending SYN.\n" << std::endl;
    }
    if (indicator == 'z'){
      std::cerr << "ERROR: creating socket.\n" << std::endl;
    }

    exit(1);
  }
}

void sighandler(int num){
   std::cerr << "Signal number : " << num << " ended the client process." << std::endl;
   exit(0);  
}

void printPacket(Packet current, bool send, int congestion_window, int SS_thresh, bool retransmit)
{
  if(send)
  {
    std::cout << "SEND " << current.m_sequenceNum << " " <<  current.m_ackNum << " " << current.m_connectionID << " " 
    << congestion_window << " " << SS_thresh;
    if (current.m_ackFlag == 1){ std::cout << " ACK"; }
    if (current.m_synFlag == 1){ std::cout << " SYN"; }
    if (current.m_finFlag == 1){ std::cout << " FIN"; }
    if (retransmit){ std::cout << " DUP"; }
    std::cout << std::endl;
  } else
  {
    std::cout << "RECV " << current.m_sequenceNum << " " <<  current.m_ackNum << " " <<current.m_connectionID << " " 
    << congestion_window << " " << SS_thresh;
    if (current.m_ackFlag == 1){ std::cout << " ACK"; }
    if (current.m_synFlag == 1){ std::cout << " SYN"; }
    if (current.m_finFlag == 1){ std::cout << " FIN"; }
    std::cout << std::endl;
  }
}