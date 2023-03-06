# CS118 Project 2

Simple Window Based Reliable Data Transfer

## Members

Dylan Phe 505834475

Xuanzhe Han 505536013

## Makefile

This provides a couple make targets for things.
By default (all target), it makes the `server` and `client` executables.

It provides a `clean` target, and `zip` target to create the submission file as well.

You will need to modify the `Makefile` USERID to add your userid for the `.zip` turn-in at the top of the file.

## Academic Integrity Note

You are encouraged to host your code in private repositories on [GitHub](https://github.com/), [GitLab](https://gitlab.com), or other places.  At the same time, you are PROHIBITED to make your code for the class project public during the class or any time after the class.  If you do so, you will be violating academic honestly policy that you have signed, as well as the student code of conduct and be subject to serious sanctions.

## Overview of the Design

This project is reliable transfer built on top of UDP. The server listens to connection request from the client, and will establis a connection with the client through three way handshake with the client when a request is received. After the connection is established, the client opens the file to be sent and checks its size and sets a boundary according to the size. It will then enter a loop to send the file in packets until it reaches that boundary, meaning it has finished sending the file. It keeps track of the oldest unacked packet and the next packet to be sent, maintaining a timer for the oldest sent packet. In the loop, if the sending window is not full yet, the client will send a packet and updates the next packet to be sent. It will then check the timer. If the timer timed out, it will check the window and send every packet in the window and reset the timer. Otherwise, it will determine whether the window is now full with the newly sent packet and set the variable that indicates it for the next iteration of the loop. Then it will check if a packet from server arrived. If an in order ACK is received, it will advance the window and reset the timer. If all packets are sent, the client will initiate connection teardown and send FIN packet to close the connection. On the server side, it will enter a loop after the threeway handshake and will only break out if a FIN packet is received. Within the loop, it checks if it received a packet from client. If it did, it will check if the packet is in or out of order. If it is in order, it sends an ACK, if it is out of order but within window size, the server sends cumulative ack of the last correctly received packet. If it is out of window size, it will send a DUPACK to advance the window on the client side.

We faced several obstacles when building this project. It took some time to figure out the format of the outputs, and it was difficult to handle the timeout events. It was difficult trying to take into account all of the possible behaviors in the window if a timeout event was to occur, and we had a persistent problem where the first payload that was sent alongside of the third packet during the threeway handshake can sometimes be lost, and the server will not resend a ACK upon receiving the timeout resend, causing the first packet to be timed out over and over again. 