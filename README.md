# CS118 Project 2

Simple Window Based Reliable Data Transfer

## Members

Dylan Phe 505834475 dylanphe@g.ucla.edu

Xuanzhe Han 505536013 xuanzhehan2002@g.ucla.edu

## Makefile

This provides a couple make targets for things.
By default (all target), it makes the `server` and `client` executables.

It provides a `clean` target, and `zip` target to create the submission file as well.

You will need to modify the `Makefile` USERID to add your userid for the `.zip` turn-in at the top of the file.

## Academic Integrity Note

You are encouraged to host your code in private repositories on [GitHub](https://github.com/), [GitLab](https://gitlab.com), or other places.  At the same time, you are PROHIBITED to make your code for the class project public during the class or any time after the class.  If you do so, you will be violating academic honestly policy that you have signed, as well as the student code of conduct and be subject to serious sanctions.

## Overview of the Design

This project is reliable transfer built on top of UDP. The server listens to connection request from the client, and will establish a connection with the client through three way handshake with the client when a request is received. After the connection is established, the client opens the file to be sent and checks its size and sets a boundary according to the size. It will then enter a loop to send the file in packets until it reaches that boundary, meaning it has finished sending the file. The client has an array of size 10, which contains the packets in the GBN window. It maintains two variables e and s, which are the indices of the next packet to be sent and the oldest unacked packet, marking the beginning and end of the GBN window. It maintains a timer for the oldest unacked packet. In addition, it maintains a variable that indicates the last ack number it received. The window will be initialized and filled with 10 packets once the connection is established to the server. In the loop, if the client receives a packet, it will check if it is a duplicate packet. If it is, it will check if it has the same ack number as the packet it last received. If it does, the packet is ignored. Otherwise, it checks if the packet is in order. If it is, it will increment the oldest unacked packet and reset the timer. Else it will check the rest of the window ahead of the oldest unacked packet, since this means one or more acks are lost and the server's window is ahead of the client. If it finds the packet ahead that has the same ack number, it will set the oldest unacked packet to that packet and reset the timer. Then the window is refilled to full. Then the client will then check the timer. If the timer timed out, it will resend every packet in the window beginning from the oldest unacked packet and reset the timer. If all packets are sent, the client will initiate connection teardown and send FIN packet to close the connection. On the server side, it will enter a loop after the threeway handshake and will only break out if a FIN packet is received. Within the loop, it checks if it received a packet from client. If it did, it will check if the packet is in or out of order. If it is in order, it sends an ACK. If it is out of order, it will send a cumulative ack of the expected in order packet. Once it receives a FIN packet, it will initialize the connection teardown. 

We faced several obstacles when building this project. It took some time to figure out the format of the outputs, and it was difficult to handle the timeout events. WIt was difficult trying to take into account all of the possible behaviors in the window if a timeout event was to occur. At first, we made the server too complicated by having it judge if the client was behind, when all server had to do was to send acks and dupacks based on its expected in order packet. We also overcomplicated the client, making it repopulate the window one packet at a time, judging if the window if full at the end of each loop. We later realized we can simply fill the window all at once and repopulate it back to 10 each time the window is changed.

We had issues with the loss of ack packages. At first we did not take it into consideration that an ack can be lost and the server can be several steps ahead of the client, so even though the client received an out of order packet, it does not need to timeout the oldest unacked packet but instead can move the window several steps forward. We also had a problem where not only the ack was lost but the ack after that is a dupack, so the client willfully ignores the packet even though it should update the window. We had no idea what was causing a timeout issue happening a packet ahead of the one that should be timed out for some time.

It was in general very difficult to debug this project considering the randomness of loss, and the logs generated for 10k and bigger files are extremely long and hard to read through, making it difficult to locate the issues. 