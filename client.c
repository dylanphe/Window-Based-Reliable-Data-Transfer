#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netdb.h> 

// =====================================

#define RTO 500000 /* timeout in microseconds */
#define HDR_SIZE 12 /* header size*/
#define PKT_SIZE 524 /* total packet size */
#define PAYLOAD_SIZE 512 /* PKT_SIZE - HDR_SIZE */
#define WND_SIZE 10 /* window size*/
#define MAX_SEQN 25601 /* number of sequence numbers [0-25600] */
#define FIN_WAIT 2 /* seconds to wait after receiving FIN*/

// Packet Structure: Described in Section 2.1.1 of the spec. DO NOT CHANGE!
struct packet {
    unsigned short seqnum;
    unsigned short acknum;
    char syn;
    char fin;
    char ack;
    char dupack;
    unsigned int length;
    char payload[PAYLOAD_SIZE];
};

// Printing Functions: Call them on receiving/sending/packet timeout according
// Section 2.6 of the spec. The content is already conformant with the spec,
// no need to change. Only call them at correct times.
void printRecv(struct packet* pkt) {
    printf("RECV %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", (pkt->ack || pkt->dupack) ? " ACK": "");
}

void printSend(struct packet* pkt, int resend) {
    if (resend)
        printf("RESEND %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "");
    else
        printf("SEND %d %d%s%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "", pkt->dupack ? " DUP-ACK": "");
}

void printTimeout(struct packet* pkt) {
    printf("TIMEOUT %d\n", pkt->seqnum);
}

// Building a packet by filling the header and contents.
// This function is provided to you and you can use it directly
void buildPkt(struct packet* pkt, unsigned short seqnum, unsigned short acknum, char syn, char fin, char ack, char dupack, unsigned int length, const char* payload) {
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->syn = syn;
    pkt->fin = fin;
    pkt->ack = ack;
    pkt->dupack = dupack;
    pkt->length = length;
    //printf("%s",payload);
    memcpy(pkt->payload, payload, length);
}

// =====================================

double setTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double) RTO/1000000;
}

double setFinTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double) FIN_WAIT;
}

int isTimeout(double end) {
    struct timeval s;
    gettimeofday(&s, NULL);
    double start = (double) s.tv_sec + (double) s.tv_usec/1000000;
    return ((end - start) < 0.0);
}

// =====================================

int main (int argc, char *argv[])
{
    if (argc != 5) {
        perror("ERROR: incorrect number of arguments\n "
               "Please use \"./client <HOSTNAME-OR-IP> <PORT> <ISN> <FILENAME>\"\n");
        exit(1);
    }

    struct in_addr servIP;
    if (inet_aton(argv[1], &servIP) == 0) {
        struct hostent* host_entry; 
        host_entry = gethostbyname(argv[1]); 
        if (host_entry == NULL) {
            perror("ERROR: IP address not in standard dot notation\n");
            exit(1);
        }
        servIP = *((struct in_addr*) host_entry->h_addr_list[0]);
    }

    unsigned int servPort = atoi(argv[2]);
    unsigned short initialSeqNum = atoi(argv[3]);

    FILE* fp = fopen(argv[4], "r");
    if (fp == NULL) {
        perror("ERROR: File not found\n");
        exit(1);
    }

    // =====================================
    // Socket Setup

    int sockfd;
    struct sockaddr_in servaddr;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr = servIP;
    servaddr.sin_port = htons(servPort);
    memset(servaddr.sin_zero, '\0', sizeof(servaddr.sin_zero));

    int servaddrlen = sizeof(servaddr);

    // NOTE: We set the socket as non-blocking so that we can poll it until
    //       timeout instead of getting stuck. This way is not particularly
    //       efficient in real programs but considered acceptable in this
    //       project.
    //       Optionally, you could also consider adding a timeout to the socket
    //       using setsockopt with SO_RCVTIMEO instead.
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    // =====================================
    // Establish Connection: This procedure is provided to you directly and is
    // already working.
    // Note: The third step (ACK) in three way handshake is sent along with the
    // first piece of along file data thus is further below

    struct packet synpkt, synackpkt;
    unsigned short seqNum = initialSeqNum;

    buildPkt(&synpkt, seqNum, 0, 1, 0, 0, 0, 0, NULL);

    printSend(&synpkt, 0);
    sendto(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    double timer = setTimer();
    int n;

    while (1) {
        while (1) {
            n = recvfrom(sockfd, &synackpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);
            if (n > 0)
                break;
            else if (isTimeout(timer)) {
                printTimeout(&synpkt);
                printSend(&synpkt, 1);
                sendto(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timer = setTimer();
            }
        }

        printRecv(&synackpkt);
        if ((synackpkt.ack || synackpkt.dupack) && synackpkt.syn && synackpkt.acknum == (seqNum + 1) % MAX_SEQN) {
            seqNum = synackpkt.acknum;
            break;
        }
    }

    // =====================================
    // FILE READING VARIABLES
    
    char buf[PAYLOAD_SIZE];
    size_t m;

    // =====================================
    // CIRCULAR BUFFER VARIABLES

    struct packet ackpkt;
    struct packet pkts[WND_SIZE];
    // index of oldest unacked
    int s = 0;
    // index of the next packet to be sent
    int e = 0;
    int full = 0;

    // =====================================
    // Send First Packet (ACK containing payload)

    m = fread(buf, 1, PAYLOAD_SIZE, fp);
    //printf("%s\n", buf);
    buildPkt(&pkts[0], seqNum, (synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 1, 0, m, buf);
    printSend(&pkts[0], 0);
    sendto(sockfd, &pkts[0], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    timer = setTimer();
    //printf("set timer for packet %d\n", pkts[s].seqnum);
    buildPkt(&pkts[0], seqNum, (synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 0, 1, m, buf);

    // Update e, the next packet to be sent
    // bytesent - keeping track of total byte sents
    // oldacked - keep track of oldest ack
    e += 1;
    e %= 10;
    int bytesent = m;
    int oldacked = 0;
    // =====================================
    // *** TODO: Implement the rest of reliable transfer in the client ***
    // Implement GBN for basic requirement or Selective Repeat to receive bonus

    // Note: the following code is not the complete logic. It only sends a
    //       single data packet, and then tears down the connection without
    //       handling data loss.
    //       Only for demo purpose. DO NOT USE IT in your final submission

    // Check file size to set boundary
    long f_size;
    fseek(fp, 0, SEEK_END);
    f_size = ftell(fp);
    rewind(fp);
    char f_len[sizeof(long)*8+1];
    sprintf(f_len, "%ld", f_size);
    int midtimerOn = 1;
    //int timerexist = 1;

    while (1) {
        // Send Subsequent Packets while WND is not full and 
        // total byte sent has not exceed the file size
        if (full != 1 && bytesent < f_size) {
            int next_seqNum = (seqNum+bytesent)%MAX_SEQN;
            // Move pointer to the next byte to be sent so that 
            // fread can read the correct byte from the file
            fseek(fp, bytesent, SEEK_SET);
            m = fread(buf, 1, ((f_size-bytesent) <= PAYLOAD_SIZE ? (f_size-bytesent) : PAYLOAD_SIZE), fp);
            // Update bytesent so far
            bytesent += m;
            // Build the packet and send 
            // Build retransmission packet as well
            // update e as well.
            buildPkt(&pkts[e], next_seqNum%MAX_SEQN, 0, 0, 0, 0, 0, m, buf);
            printSend(&pkts[e], 0);
            sendto(sockfd, &pkts[e], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
            // if(timerexist){
            //     timer = setTimer();
            //     timerexist = 0;
            // }
            //buildPkt(&pkts[e], next_seqNum%MAX_SEQN, 0, 0, 0, 0, 1, m, buf);
            e += 1;
            e %= 10;
        }
        //printf("%d, %d\n", e, s);

        // TIMEOUT
                // If received IN ORDER ACK, and not a duplicate, reset timer as s moves up one step.
        n = recvfrom(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);
        if (n > 0) {
            //printf("%d, %d\n", ackpkt.acknum, (pkts[s].seqnum + pkts[s].length)%MAX_SEQN);
            // timer is restart
            if (ackpkt.acknum == (pkts[s].seqnum + pkts[s].length)%MAX_SEQN && !ackpkt.dupack) {
                //printf("old ack was: %d ", oldacked);
                oldacked += pkts[s].length;
                //printf("old ack is now %d\n", oldacked);
                s += 1;
                s %= 10;
                printRecv(&ackpkt);
                timer = setTimer();
                //printf("set timer for packet %d\n", pkts[s].seqnum);
            } 

            else if (ackpkt.acknum > (pkts[s].seqnum + pkts[s].length)%MAX_SEQN && !ackpkt.dupack){
                printRecv(&ackpkt);
                //printf("need to readjust window, current: %d. next: %d\n",(pkts[s].seqnum + pkts[s].length)%MAX_SEQN, ackpkt.acknum);
                while((pkts[s].seqnum + pkts[s].length)%MAX_SEQN != ackpkt.acknum){
                    //printf("adjust window, oldacked was: %d ", oldacked);
                    oldacked += pkts[s].length;
                    s += 1;
                    s %= 10;
                    //printf("oldacked is now: %d, and s is: %d, and e is: %d\n", oldacked, s, e);
                    if (bytesent < f_size && (oldacked < f_size && abs(e-s) != 0)) {
                        int next_seqNum = (seqNum+bytesent)%MAX_SEQN;
                        // Move pointer to the next byte to be sent so that 
                        // fread can read the correct byte from the file
                        fseek(fp, bytesent, SEEK_SET);
                        m = fread(buf, 1, ((f_size-bytesent) <= PAYLOAD_SIZE ? (f_size-bytesent) : PAYLOAD_SIZE), fp);
                        // Update bytesent so far
                        bytesent += m;
                        // Build the packet and send 
                        // Build retransmission packet as well
                        // update e as well.
                        buildPkt(&pkts[e], next_seqNum%MAX_SEQN, 0, 0, 0, 0, 0, m, buf);
                        printSend(&pkts[e], 0);
                        sendto(sockfd, &pkts[e], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                        e += 1;
                        e %= 10;
                        //printf("bytesent is now: %d, e is: %d\n", bytesent, e);
                    } else {
                        break;
                        //printf("bye\n");
                    }
                }
                oldacked += pkts[s].length;
                s += 1;
                s %= 10;
                timer = setTimer();
            }
            else if (ackpkt.dupack) {
                printRecv(&ackpkt);
            }
            
            // Loop breaker: when file size reached for 
            if (oldacked >= f_size && abs(e-s) == 0) {
                //printf("%ld, %d", f_size, oldacked);
                midtimerOn = 0;
                break;
            }
        } 
        else if (isTimeout(timer) && midtimerOn) {
            printTimeout(&pkts[s]);
            //printf("%d, %d\n", e, s);
            // In case of full WND
            if (e == s) {
                //printSend(&pkts[s], 1);
                sendto(sockfd, &pkts[s], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                int i = s;
                //printf("%d, %d, %d\n", e, s, i);
                while (i < e+10) {
                    //printf("%d\n",i);
                    printSend(&pkts[abs(i%10)], 1);
                    sendto(sockfd, &pkts[abs(i%10)], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                    i++;
                }
                timer = setTimer();
                //printf("set timer for packet %d\n", pkts[s].seqnum);
            }
            else if (e > s) {
                int i = s;
                //printf("%d, %d, %d\n", e, s, i);
                while (i < e) {
                    //printf("%d\n",i);
                    printSend(&pkts[abs(i%10)], 1);
                    sendto(sockfd, &pkts[abs(i%10)], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                    i++;
                }
                timer = setTimer();
                //printf("set timer for packet %d\n", pkts[s].seqnum);
            }
            else if (e < s) {
                int i = s;
                //printf("%d, %d, %d\n", e, s, i);
                while (i < e+10) {
                    //printf("%d\n",i);
                    printSend(&pkts[abs(i%10)], 1);
                    sendto(sockfd, &pkts[abs(i%10)], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                    i++;
                }
                timer = setTimer();
                //printf("set timer for packet %d\n", pkts[s].seqnum);
            }
        }

        // Determine if window is full or not.
        if (abs(e - s) == 0) {
            full = 1; 
        } else {
            full = 0;
        }

        //Sprintf("%d\n",e-s);
    }

    // *** End of your client implementation ***
    fclose(fp);

    // =====================================
    // Connection Teardown: This procedure is provided to you directly and is
    // already working.
    struct packet finpkt, recvpkt;
    buildPkt(&finpkt, ackpkt.acknum, 0, 0, 1, 0, 0, 0, NULL);
    buildPkt(&ackpkt, (ackpkt.acknum + 1) % MAX_SEQN, (ackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 1, 0, 0, NULL);

    printSend(&finpkt, 0);
    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    timer = setTimer();
    int timerOn = 1;

    double finTimer;
    int finTimerOn = 0;

    while (1) {
        while (1) {
            n = recvfrom(sockfd, &recvpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);
            if (n > 0)
                break;
            if (timerOn && isTimeout(timer)) {
                printTimeout(&finpkt);
                printSend(&finpkt, 1);
                if (finTimerOn)
                    timerOn = 0;
                else
                    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timer = setTimer();
            }
            if (finTimerOn && isTimeout(finTimer)) {
                close(sockfd);
                if (! timerOn)
                    exit(0);
            }
        }
        printRecv(&recvpkt);
        if ((recvpkt.ack || recvpkt.dupack) && recvpkt.acknum == (finpkt.seqnum + 1) % MAX_SEQN) {
            timerOn = 0;
        }
        else if (recvpkt.fin && (recvpkt.seqnum + 1) % MAX_SEQN == ackpkt.acknum) {
            printSend(&ackpkt, 0);
            sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
            finTimer = setFinTimer();
            finTimerOn = 1;
            buildPkt(&ackpkt, ackpkt.seqnum, ackpkt.acknum, 0, 0, 0, 1, 0, NULL);
        }
    }
}
