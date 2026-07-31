#include "../server_common/serverapp.h"

int main(int argc, char* argv[])
{
    return runServerApplication(argc,
        argv,
        "server-512mb",
        "remote-clipboard-server-512mb",
        512ull * 1024ull * 1024ull);
}
