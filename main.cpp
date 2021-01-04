#include <unistd.h>
#include "util.h"
#include "http_server.h"

int main( int argc, char* argv[] )
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi( argv[2] );

    handle_for_sigpipe();//?
    HttpServer http_server(ip, port);
    http_server.serve();
}

