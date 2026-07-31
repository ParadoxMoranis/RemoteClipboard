#include "../server_common/serverapp.h"

int main(int argc, char* argv[])
{
    return runServerApplication(argc,
        argv,
        "server-64mb",
        "remote-clipboard-server-64mb",
        64ull * 1024ull * 1024ull);
}
