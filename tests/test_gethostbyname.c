#include <assert.h>
#include <netdb.h>

int main( int argc, char **argv ) {
    assert( gethostbyname( argv[ 1 ] ) );
    return 0;
}
