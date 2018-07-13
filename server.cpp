#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <dirent.h>
#include <pthread.h>
#include "common.h"

static char gCurrentFolder[PATH_MAX];

int ResponseOK(int fd, const char* comment)
{
    const char text[]= "OK\n";
    int n = write(fd,text, strlen(text));
    if (n < 0) {
        perror("ERROR writing to socket");
        return n;
    }
    if(comment){
        n = write(fd,comment, strlen(comment));
    }
    return n;
}
int ResponseFailed(int fd, const char* reason)
{
    char buffer[256];
    sprintf(buffer, "FAILED\n%s\n", reason);
    int n = write(fd,buffer, strlen(buffer));
    if (n < 0) {
        perror("ERROR writing to socket");
        return n;
    }
    return 0;
}

int HandleCommandList(int fd)
{
    char text[256];
    char buffer[1024];
    ResponseOK(fd, NULL);
    buffer[0] = 0;

    DIR *d;
    struct dirent *dir;

    d = opendir(gCurrentFolder);
    int remainsize = sizeof(buffer);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            remainsize -= strlen(dir->d_name)+1;
            if (remainsize < 0)
                break;
          strcat(buffer, dir->d_name);
          strcat(buffer, "\n");
        }
        closedir(d);
    }

    int len = strlen(buffer)+1;
    sprintf(text, "%d\n", len);
    int n = write(fd,text, strlen(text));
    if (n < 0) {
        perror("ERROR writing to socket 1");
        return n;
    }
    n = write(fd,buffer, strlen(buffer)+1);
    if (n < 0) {
        perror("ERROR writing to socket 2");
        return n;
    }
    return 0;
}

int HandleCommandText(int fd, char* arg)
{
    char buffer[256];
    int nLen = atoi(arg);
    if (nLen < 256)
        ResponseOK(fd, NULL);
    else {
        sprintf(buffer, "FAILED. Message too long, the maximum is %d characters.\n", (int) sizeof(buffer)-1);
        return ResponseFailed(fd, buffer);
    }

    //prepare to read message
    memset(buffer, 0, 256);
    int n = read(fd,buffer,255);
    if (n < 0) {
      perror("ERROR reading from socket");
    }
    buffer[n] = 0;
    printf("Client says: %s\n", buffer);
    ResponseOK(fd, NULL);
    return 0;
}
int HandleCommandSetDir(int fd, char* folder)
{
    char buffer[512];
    int res = chdir(folder);
    if (res ==0) {
        getcwd(gCurrentFolder, sizeof(gCurrentFolder));
        sprintf(buffer, "Now current folder is %s\n", gCurrentFolder);
        puts(buffer);
        return ResponseOK(fd, buffer);
    }

    sprintf(buffer, "FAILED: error code =%d\n", res);
    return ResponseFailed(fd, buffer);

}
int HandleCommandRemoveFile(int fd, char* file)
{
    char buffer[512];

    int res = remove(file);
    if (res ==0) {
        sprintf(buffer, "File %s s removed.\n", file);
        return ResponseOK(fd, buffer);
    }

    sprintf(buffer, "FAILED: error code =%d\n", res);
    return ResponseFailed(fd, buffer);

}

int HandleCommandGetDir(int fd)
{
    return ResponseOK(fd, gCurrentFolder);
}
int HandleCommandPutFile(int fd, char* name, int len)
{
    char buffer[512];
    FILE* fp;
    fp = fopen(name, "wb");
    printf("open %sto save\n", name);

    if(!fp){
        return ResponseFailed(fd, "Open file failed.");
    }
    ResponseOK(fd, NULL);

    memset(buffer, 0, sizeof(buffer));
    int remain = len;
    while(remain >0){

        int n = read(fd, buffer, min(remain, 512));
        printf("read %d bytes\n", n);
        if (n<0)
        {
           perror("ERROR reading socket\n");
           fclose(fp);
           return ResponseFailed(fd, "Socket read error.");
        }
        int m = fwrite(buffer, 1, n, fp);
        if (m != n) {
            perror("ERROR writing file\n");
            fclose(fp);
            return ResponseFailed(fd, "File I/O error.");
        }
        remain -= n;
    }
    fclose(fp);
    sprintf(buffer, "Received %d bytes\n", len);
    puts(buffer);
    return ResponseOK(fd, buffer);
}
int HandleCommandGetFile(int fd, char* name)
{
    char buffer[512];
    FILE* fp;
    long len = 0;
    fp = fopen(name, "rb");
    if(!fp) {
        return ResponseFailed(fd, "Open file failed\n");
    }


    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);


    if (len == 0){
        fclose(fp);
        return ResponseFailed(fd, "File length is zero.n");
    }
    sprintf(buffer, "%ld\n", len);
    ResponseOK(fd, buffer);

    int remain = len;
    while (remain > 0){
        int n = fread(buffer, 1, min(remain, 512), fp);
        if (n < 0){
            break;
        }
        int m = write(fd, buffer, n);
        if (m!= n){
            break;
        }
        remain -= n;
    }
    fclose(fp);
    if (remain == 0)
        printf("File %s transmitted OK.\n", name);
    else
        printf("%d bytes not transmitted!!\n", remain);
    return 0;
}
int HandleCommandGetChannel(int fd, int  channel)
{
    char buffer[512];
    if (channel >=4) {
        sprintf(buffer, "Wrong channel id %d.\n", channel);
        return ResponseFailed(fd, buffer);
    }
    int len = 512*100;
    int n = 0;
    sprintf(buffer, "%d\n", len);
    ResponseOK(fd, buffer);
    for (int i=0;i<100; i++ ){
        sprintf(buffer, "\n%d- This is channel %d", i+1, channel);
        int m = write(fd, buffer, 512);

        if (m < 0)
        {
            perror("Write socket error!\n");
            break;
        }
        n += m;
    }
    printf("GetChannel transmited %d bytes\n", n);
    return 0;
}

void* ProcessClientCommand(void* data)
{
    int fd = *(int*) data;
    int n;
    char buffer[256];
    memset(buffer, 0, 256);
    n = read(fd,buffer,255);
    if (n<0) {
        printf("Read client message error!\n");
        return (void*) -1;
    }
    buffer[n] = 0;
    char* pCmd;
    char* pArg = NULL;
    char* pArg2 = NULL;

    pCmd = strtok(buffer, "\n");
    if (!pCmd){
        printf("Unrecognized client message %s\n", buffer);
        return (void*) -2;
    }
    pArg = strtok(NULL, "\n");
    if (pArg) {
        pArg2 = pArg + strlen(pArg) + 1;
    }
    printf("Client commands: %s", pCmd);
    if (pArg) {
        printf("- %s\n", pArg);
    } else {
        printf("\n");
    }
    if (strcmp(pCmd, PUT_TEXT) ==0 ){
        HandleCommandText(fd, pArg);
    } else if(strcmp(pCmd, SET_DIR) ==0 ) {
         HandleCommandSetDir(fd, pArg);
    } else if(strcmp(pCmd, GET_DIR) ==0 ) {
         HandleCommandGetDir(fd);
    } else if(strcmp(pCmd, PUT_FILE) ==0 ) {
         HandleCommandPutFile(fd, pArg, atoi(pArg2));
    } else if(strcmp(pCmd, GET_FILE) ==0 ) {
         HandleCommandGetFile(fd, pArg);
    } else if(strcmp(pCmd, GET_CHANNEL) ==0 ) {
         HandleCommandGetChannel(fd, atoi(pArg));
    } else if(strcmp(pCmd, RM_FILE) ==0 ) {
         HandleCommandRemoveFile(fd, pArg);
    } else if(strcmp(pCmd, LIST_DIR) ==0 ) {
        HandleCommandList(fd);
    }

    return (void*) 0;
}

int CreateServer(int port)
{
    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return -1;
    }
    /* Initialize socket structure */
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    printf("Bind to %d\n", port);
    /* Now bind the host address using bind() call.*/
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        return -1;
    }
    listen(sockfd,5);
    return sockfd;
}
int OnAccept(int fd)
{
    struct sockaddr_in cli_addr;
    socklen_t clilen= sizeof(cli_addr);
    int newsockfd = accept(fd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
       perror("ERROR on accept");
    }
    return newsockfd;
}

void RunServer(int serverPort)
{
    //set current folder as global, since cwd works for child thread only
    getcwd(gCurrentFolder, sizeof(gCurrentFolder));

    int fd = CreateServer(serverPort);
    while (fd > 0) {
      int fdClient = OnAccept(fd);
      if (fdClient < 0){
          printf("something broken\n");
          close(fd);
          break;
      }
      pthread_t thread;
      pthread_create(&thread, NULL, ProcessClientCommand, (void*) &fdClient);

    }
    printf("Server terminated.\n");
}

