#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

int main( int argc, char **argv ) {
    int sock;
    struct sockaddr_in server;
    char* message = "Hello, world!";
    int message_length = strlen( message );
    int sent_length;

    inet_pton( AF_INET, argv[ 1 ], &server.sin_addr );
    server.sin_family = AF_INET;
    server.sin_port = htons( atoi( argv[ 2 ] ) );

    sock = socket( AF_INET, SOCK_STREAM, 0 );
    connect(
        sock,
        (struct sockaddr*) &server,
        sizeof( server )
    );
    sent_length = send( sock, message, message_length, 0 );
    close( sock );

    assert( sent_length == message_length );
    return 0;
}
