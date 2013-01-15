#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

int main( int argc, char **argv ) {
    char* message = "Hello, world!\n\r" ;
    int message_length = strlen( message );
    int sent_length;

    struct sockaddr_in si_other;
    int fd;

    fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    if ( fd == - 1 ) {
        return -1;
    } else {
        memset((char *) &si_other, 0, sizeof(si_other));
        si_other.sin_family = AF_INET;
        si_other.sin_port = htons( 53 );
        if (inet_aton( "8.8.8.8", &si_other.sin_addr)==0) {
          fprintf(stderr, "inet_aton() failed\n");
          exit(1);
        }

        sent_length = send( fd, message, message_length, 0 );
        printf(
            "message_length, sent_length = %d, %d\n",
            message_length,
            sent_length
        );
        close( fd );

        return 0;
    }
}
