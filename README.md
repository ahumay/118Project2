# CS118 Project 2
## Makefile

This provides a couple make targets for things.
By default (all target), it makes the `server` and `client` executables.

It provides a `clean` target, and `tarball` target to create the submission file as well.

You will need to modify the `Makefile` to add your userid for the `.tar.gz` turn-in at the top of the file.

## Provided Files

`server.cpp` and `client.cpp` are the entry points for the server and client part of the project.

## Academic Integrity Note

You are encouraged to host your code in private repositories on [GitHub](https://github.com/), [GitLab](https://gitlab.com), or other places.  At the same time, you are PROHIBITED to make your code for the class project public during the class or any time after the class.  If you do so, you will be violating academic honestly policy that you have signed, as well as the student code of conduct and be subject to serious sanctions.

## Wireshark dissector

For debugging purposes, you can use the wireshark dissector from `tcp.lua`. The dissector requires
at least version 1.12.6 of Wireshark with LUA support enabled.

To enable the dissector for Wireshark session, use `-X` command line option, specifying the full
path to the `tcp.lua` script:

    wireshark -X lua_script:./confundo.lua

To dissect tcpdump-recorded file, you can use `-r <pcapfile>` option. For example:

    wireshark -X lua_script:./confundo.lua -r confundo.pcap

## TODO
Anthony Humay 304731856
Primarily worked on server and bug fixes
Clayton Green 404546151 
Architected server and client, worked on both
Andrew Musk 704771548
Worked heavily on client, handled congestion control

//////
High level design of your server and client
//////
Our server accepts three arguments as specified and has a separate error
handler that ensures everything is correct. 
We then set up our sockets in the same way that we were encouraged to
by the link in the spec (https://en.wikipedia.org/wiki/Berkeley_sockets).
We then have a while loop that waits for incoming packets. Generally, 
we distinguish packets into 3 categories: SYN, FIN, or normal. 
When a packet comes in, we first check to see if it's one of those three. If
the packet is out of order (based on our next expected sequence number), we DROP.
If SYN, we handshake and create the necessary resources for a new connection. 
The necessary resources are a file path of where to save the data, 
the last packet received from that connection, and the next sequence number
that we're expecting from that connection. 
If FIN, we end the process. If normal, we save the the data to the file
and send an ACK. We then update the next expected sequence number based on 
their current sequence number + 512 (the number of bytes we just ACK'd). 

In our client, we similarly first handle bad arguments and set up the 
BSD sockets. Then  we initiate the handshake with the server. We send SYN, 
wait for SYN ACK, and then ACK. 
Then, we send the file. We read using fread() from the file into a 524 buffer, 
and package the data into a packet. We then send that packet and start a timer
called retransmissionTimer that waits 0.5 seconds for an ACK and sends a DUP
if it doensn't. There's another timer timeLastReceived that is constantly waiting
for the server to not respond for 10 seconds, and as the name indicates marks the 
last time we received something from the server. 
Congestion control is implemented in this loop, and keeps a queue of packets that
haven't been ACK'd yet, and takes them out when they are. 

We also had a separate class for packets that contained all information requested in the
spec. The create_network_packet() function converts that information to network order. 

//////
Problems
//////
A big problem we ran into was how to create a network order packet. Creating a
packet in a separate class was pretty straight forward; we just had to make sure 
it had the right information. However, whenever we tried to convert this information
to network order, something would be wrong. We ended up realizing that our bit shifting
to the right by 16 was deleting the flags that we were saving there, which is why 
we never got flags. 

Another problem we had was the sequence numbers from the client not looping around. 
Once above 102400, they continued to jump intermiddently even though we had written
code to take the wrapping around into account. It turned out that that was never being run
because we were checking ACK number instead of SEQ.

Initially, we had a big problem with the creating new connections. We had a separate helper 
function for SYN packets, but due to scope issues, it just caused headaches. Just having
another check for SYN packets in the main reception while loop was much simpler and more effective. 

//////
Additional libraries used
//////
Our "packet.h" class that we created

Along with:
<stdlib.h>
<stdio.h>
<errno.h>
<string.h>
<sys/socket.h>
<sys/types.h>
<netinet/in.h>
<unistd.h>
<arpa/inet.h>
<inttypes.h>
<signal.h>
<fstream>
<sys/stat.h>
<pthread.h>
<inttypes.h>
<fcntl.h>
<queue>
<unordered_map>

//////
Acknowledgement of any online tutorials
//////
The only outside information we used was 
https://en.wikipedia.org/wiki/Berkeley_sockets
