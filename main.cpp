#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
 #include <unistd.h>
#include <sys/resource.h>
#include <sched.h>

#define DEFAULT_PORT    9000


const char optStr[] = "sp:c:t:d:w:r:g:m:fl";
const char usageStr[] =
        "Server:\n"
        "  -s                  run as a server. Default is as server.\n"
        "  -p <port>           server port to listen on Default 9000\n\n"
        "Client:\n"
        "  -c <host>           run as a client, connect to server ip <host>\n"
        "  -p <port>           server port to connect to\n"
        "  -t <text>           send text to server\n"
        "  -d <folder>         set server current folder\n"
        "  -f                  get server current folder\n"
        "  -m <file>           remove file\n"
        "  -w <file>           write a file to server on current folder\n"
        "  -r <file>           read a file in current folder of server\n"
        "  -g <channel>        get <channel> image of the server\n"
        "  -l                  list files in current folder";
void PrintUsage(const char* name)
{
    printf("Usage: %s [-s|-c host] [options]\n", name);
    puts(usageStr);

}
void RunClient(char* serverIp, int serverPort, int command, char* argument);
void RunServer(int serverPort);

int main(int argc, char *argv[])
{
    int isRoleClient = 0; //default as client
    int serverPort = DEFAULT_PORT;
    char* serverIp = NULL;
    int command = 0;
    char* argument = NULL;

    int c;
    while ((c = getopt (argc, argv, optStr)) != -1) {
        switch (c)
        {
        case 'c':
            isRoleClient = 1;
            serverIp = optarg;
            break;
        case 'p':
            serverPort = atoi(optarg);
            break;
        case 't':
        case 'd':
        case 'w':
        case 'r':
        case 'g':
        case 'm':
            command = c;
            argument = optarg;
            break;

        case 'l':
        case 'f':
            command = c;
            break;
        default:
            PrintUsage(argv[0]);
            exit(0);
        }
    }
    if (isRoleClient)
        RunClient(serverIp, serverPort, command, argument);
    else
        RunServer(serverPort);


    return 0;
}

