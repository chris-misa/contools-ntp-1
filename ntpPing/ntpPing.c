//
// A simple NTP client that does not care about time
// but only reports the latencies recorded in the ntp
// protocol: for pinging ntp servers using ntp.
//
// Mostly adaped from https://github.com/lettier/ntpclient/blob/master/source/c/main.c
//

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define USAGE "getLatencies <ipv4 addr>\n"

#define OFFSET 2208988800ULL

//
// Struct for ntp packet data
//
typedef struct
{
  uint8_t li_vn_mode;

  uint8_t stratum;
  uint8_t poll;
  uint8_t precision;

  uint32_t rootDelay;
  uint32_t rootDispersion;
  uint32_t refId;

  uint32_t refTm_s;
  uint32_t refTm_f;

  uint32_t origTm_s;
  uint32_t origTm_f;

  uint32_t recTm_s;
  uint32_t recTm_f;
  
  uint32_t transTm_s;
  uint32_t transTm_f;
} ntp_packet;

//
// Functions to convert between struct timeval and ntp times stamp
//
// Adapted from https://stackoverflow.com/questions/29112071/how-to-convert-ntp-time-to-unix-epoch-time-in-c-language-linux
//
void tv2ntp(struct timeval *tv, uint32_t *s, uint32_t *f)
{
  uint64_t aux = 0;

  // Convert micro-seconds into fractional seconds
  aux = tv->tv_usec;
  aux <<= 32;
  aux /= 1000000;

  *f = htonl(aux);

  // Convert unix epoc seconds into ntp epoch seconds
  aux = tv->tv_sec;
  aux += OFFSET;
  
  *s = htonl(aux);
}

void ntp2tv(uint32_t s, uint32_t f, struct timeval *tv)
{
  uint64_t aux = 0;
  aux = ntohl(s);
  tv->tv_sec = aux - OFFSET;
  aux = ntohl(f);
  aux *= 1000000;
  aux >>= 32;
  tv->tv_usec = aux;
}

//
// Utility for printing timeval
//
void print_tv(struct timeval *t)
{
  printf("%ld.%06ld\n", t->tv_sec, t->tv_usec);
}

//
// Utility for subtracting timeval structs
// From: http://www.gnu.org/software/libc/manual/html_node/Elapsed-Time.html
//
int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
  // Perform carry for subtraction by updating y if needed
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  // Do the subtraction
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  // Return 1 if result is negative
  return x->tv_sec < y->tv_sec;
}

//
// Main
//
int main( int argc, char *argv[] )
{
  int sockfd, n;
  int portno = 123;
  struct sockaddr_in target_addr;
  struct hostent *server;
  ntp_packet sendPkt = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  struct timeval sendTime;
  struct timeval targetRecTime;
  struct timeval targetSendTime;
  struct timeval recTime;
  struct timeval sendBias;
  struct timeval recvBias;
  

  if ( argc != 2 ) {
    printf(USAGE);
    exit(0);
  }

  // Populate the target address structure
  memset( &target_addr, 0, sizeof( struct sockaddr_in ) );
  target_addr.sin_family = AF_INET;
  target_addr.sin_port = htons( portno );
  inet_pton(AF_INET, argv[1], &target_addr.sin_addr);

  // Populate the ntp request packet
  *( (char*) &sendPkt ) = 0x1b; // set li = 0, vn = 3, mode = 3
  gettimeofday(&sendTime, NULL);
  tv2ntp(&sendTime,&sendPkt.origTm_s,&sendPkt.origTm_f);

  // debugging: write out packet
  // write(1, (const void *)&sendPkt, sizeof(ntp_packet));
  // return 0;

  // Get a udp socket
  sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
  if ( sockfd < 0 ) {
    printf("Error creating UDP socket\n");
    exit(1);
  }

  // Connect to target
  if ( connect( sockfd, (struct sockaddr *) &target_addr, sizeof( target_addr ) ) < 0 ) {
    printf("Error connecting to server\n");
    exit(1);
  }

  // Send ntp packet
  n = write( sockfd, (char*) &sendPkt, sizeof( ntp_packet ) ); 

  if ( n < 0 ) {
    printf("Error writing to socket\n");
    exit(1);
  }

  // Receive response
  n = read( sockfd, (char*) &sendPkt, sizeof( ntp_packet ));

  gettimeofday(&recTime, NULL);

  if ( n < 0 ) {
    printf("Error reading from socket\n");
    exit(1);
  }

  // Move ntp timestamps from response back to timevals
  ntp2tv(sendPkt.recTm_s,sendPkt.recTm_f,&targetRecTime);
  ntp2tv(sendPkt.transTm_s,sendPkt.transTm_f,&targetSendTime);

  // Print everything out
  printf("Origin send: ");
  print_tv(&sendTime);
  printf("Target rec:  ");
  print_tv(&targetRecTime);

  timeval_subtract(&sendBias, &targetRecTime, &sendTime);
  printf("  Outbound Bias: ");
  print_tv(&sendBias);

  printf("Target send: ");
  print_tv(&targetSendTime);
  printf("Origin rec:  ");
  print_tv(&recTime);

  timeval_subtract(&recvBias, &recTime, &targetSendTime);
  printf("  Inbound Bias:  ");
  print_tv(&recvBias);
  

  return 0;
}
