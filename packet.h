#ifndef PACKET_H
#define PACKET_H

#include <string>
#include <sys/time.h>
#include <stdio.h>
#include <bitset>
#include <cstring>

class Packet{
  public:
    Packet() {}
    uint16_t m_connectionID;
    bool m_ackFlag:1;
    bool m_synFlag:1;
    bool m_finFlag:1;
    uint32_t m_sequenceNum; 
    uint32_t m_ackNum;  
    char* m_data; 

    Packet(uint32_t seq_num, uint32_t ack_num, uint16_t ID, bool ack, bool syn, bool fin, char * data){
      m_sequenceNum = seq_num;
      m_ackNum = ack_num;
      m_connectionID = ID;
      m_ackFlag = ack;
      m_synFlag = syn;
      m_finFlag = fin;
      m_data = data;
    }


    Packet(void* network_packet, int total_length){
      // given binary

      uint32_t sequenceNumberFromPacket = ntohl(((uint32_t *) network_packet)[0]);
      uint32_t ackNumberFromPacket = ntohl(((uint32_t *) network_packet)[1]);
      /*
      printf("Unpacking sequence number: %" PRIu32 "\n", sequenceNumberFromPacket);
      printf("Unpacking ack number : %" PRIu32 "\n", ackNumberFromPacket);
      */

      uint16_t net_con_id_print = ((uint16_t *) network_packet)[4];
      uint16_t con_id_print = ntohs(net_con_id_print);

      uint16_t net_flags_print = (((uint16_t *) network_packet)[5]);
      uint16_t flags_print = ntohs(net_flags_print);

      /*
      printf("Unpacking connection ID : %" PRIu16 "\n", con_id_print);
      printf("Unpacking flags : %" PRIu16 "\n", flags_print);
      */

      m_sequenceNum = sequenceNumberFromPacket;
      m_ackNum = ackNumberFromPacket;
      m_connectionID = con_id_print;

      uint16_t ack_mask = 4;
      uint16_t syn_mask = 2;
      uint16_t fin_mask = 1;

      if (flags_print & ack_mask){
        // printf("Unpacked m_ackFlag\n"); 
        m_ackFlag = 1;
      } else {m_ackFlag = 0;}
      if (flags_print & syn_mask){
        // printf("Unpacked m_synFlag\n"); 
        m_synFlag = 1;
      } else {m_synFlag = 0;}
      if (flags_print & fin_mask){
        // printf("Unpacked m_finFlag\n"); 
        m_finFlag = 1;
      } else {m_finFlag = 0;}

      m_data = (char*)malloc(total_length - 12);
      char * actualLoc = ((char*)network_packet);
      m_data = actualLoc + 12;
      // std::fstream newFileFD;
      // newFileFD.open("test.txt", std::fstream::in | std::fstream::out | std::fstream::app);
      // newFileFD.write(m_data, total_length - 12);
    }

    void* create_network_packet() {
      // get network byte order for header fields
      uint32_t net_seq_num = htonl(m_sequenceNum);
      uint32_t net_ack_num = htonl(m_ackNum);
      uint16_t net_connectionID = htons(m_connectionID);      
      uint16_t flags = 0;
      if(m_finFlag){
        flags += 1;
      }
      if(m_synFlag){
        flags += 2;
      }
      if(m_ackFlag){
        flags+= 4;
      }
      uint16_t net_flags = htons(flags);

      // insert header fields into a char buffer
      void* buffer = malloc(524);
      ((uint32_t*) buffer)[0] = net_seq_num;
      ((uint32_t*) buffer)[1] = net_ack_num;

      //((uint32_t*) buffer)[2] = third_byte;
      ((uint16_t*) buffer)[4] = net_connectionID;
      ((uint16_t*) buffer)[5] = net_flags;

      // check to make sure they're correct 
      
      // uint16_t net_con_id_print = ((uint16_t *) buffer)[4];
      // uint16_t con_id_print = ntohs(net_con_id_print);

      // uint16_t net_flags_print = (((uint16_t *) buffer)[5]);
      // uint16_t flags_print = ntohs(net_flags_print);

      // printf("Sequence number: %" PRIu32 "\n", ntohl(((uint32_t *) buffer)[0]));
      // printf("Ack number : %" PRIu32 "\n", ntohl(((uint32_t *) buffer)[1]));
      // printf("Connection ID : %" PRIu16 "\n", con_id_print);
      // printf("Flags : %" PRIu16 "\n", flags_print); 

      // insert data into the char buffer
      if((! m_synFlag) && m_data != NULL) {
        int i = 12;
        while(1) {
          char a = m_data[(i - 12)];
          if(a == '\0') {
            break;
          } else {
            ((char*) buffer)[i] = a;
            i++;
          }
        }
      }

      // return buffer
      return buffer;
    }
};
#endif