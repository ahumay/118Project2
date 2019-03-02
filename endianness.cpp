#include <iostream>
#include <netinet/in.h>
#include <iomanip>
#include <cstdint>
using namespace std;

void printInt_32(uint32_t x)
{
    cout << setfill('0') << setw(8) << hex << x << '\n';
}

void printInt_16(uint16_t x)
{
    cout << setfill('0') << setw(4) << hex << x << '\n';
}

int main() 
{
    uint32_t myInt = 12345;
    printInt_32(myInt);

    //Convert byte order from HOST to NETWORK
    uint32_t myInt_n = htonl(myInt);
    printInt_32(myInt_n);

    //Convert back to HOST
    uint32_t myInt_h = ntohl(myInt_n);
    printInt_32(myInt_h);


    uint16_t myInt16 = 0;
    printInt_16(myInt16);

    uint16_t ACK = 1 << 2;
    printInt_16(ACK);
    
    uint16_t FIN = 1;

    myInt16 = ACK | FIN;
    printInt_16(myInt16);

    uint16_t myInt16_n = htons(myInt16);
    printInt_16(myInt16_n);

    return 0;

}
