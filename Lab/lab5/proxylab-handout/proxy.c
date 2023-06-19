#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

struct http_header {
    char *key;
    char *value;
    struct http_header *next;
};
typedef struct http_header http_header;

struct http_request {
    char uri[MAXLINE];
    char path[MAXLINE];
    char version[MAXLINE];
    char host[MAXLINE];
    char hostname[MAXLINE];
    char port[MAXLINE];

    http_header *extra_hdrs;
};
typedef struct http_request http_request;

void error(const char *msg);
void *proxy_thread(void *vargp);

int main(int argc, char *argv[])
{
    int listenfd;
    char *port;
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);
    pthread_t tid;

    // Check port number
    if (argc < 2) {
        error("ERROR, no port provided\n");
        exit(1);
    }
    port = argv[1];

    // Establish listening requests
    listenfd = Open_listenfd(port);
    if (listenfd < 0) {
        error("ERROR, while opening listenfd\n");
    }

    // Accept client request
    while (1) {
        int *connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Pthread_create(&tid, NULL, proxy_thread, connfdp);
    }

    return 0;
}

void error(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
}

http_request parse_request(int connfd)
{
    printf("parse_request\n");

    rio_t rio;
    char buf[MAXLINE];
    http_request request;
    http_header *curr = NULL;

    memset(&request, 0, sizeof(http_request));
    memset(buf, 0, sizeof(buf));

    // Read request
    Rio_readinitb(&rio, connfd);

    Rio_readlineb(&rio, buf, MAXLINE);
    printf("%s", buf);
    sscanf(buf, "GET %s %s", request.uri, request.version);
    sscanf(request.uri, "http://%*[^/]%s", request.path);
    if (strcmp(request.path, "") == 0) {
        strcpy(request.path, "/");
    }
    
    while (Rio_readlineb(&rio, buf, MAXLINE) != 0) {
        printf("%s", buf);

        // Last line of request
        if (strcmp(buf, "\r\n") == 0) {
            break;
        }

        char *key = trim(strtok(buf, ":"));
        char *value = trim(strtok(NULL, "\r\n"));

        // If Host header is found, parse hostname and port
        if (strcmp(key, "Host") == 0) {
            sscanf(value, "%s", request.host);
            sscanf(value, "%[^:]:%s", request.hostname, request.port);
            if (strcmp(request.port, "") == 0) {
                strcpy(request.port, "80");
            }
        // Ignore headers
        } else if (strcmp(key, "User-Agent") == 0 || strcmp(key, "Connection") == 0 || strcmp(key, "Proxy-Connection") == 0) {
            continue;
        // Add extra header to linked list
        } else {
            http_header *hdr = Malloc(sizeof(http_header));
            hdr->key = strdup(key);
            hdr->value = strdup(value);
            hdr->next = NULL;

            if (curr == NULL) {
                request.extra_hdrs = hdr;
                curr = hdr;
            } else {
                curr->next = hdr;
                curr = curr->next;
            }
        }
    }

    return request;
}

int forward_request(http_request request)
{
    printf("\nforward_request\n");

    int n;
    int clientfd;
    char buf[MAXLINE];
    rio_t rio;
    http_header *curr = request.extra_hdrs;

    // Open client connection
    clientfd = Open_clientfd(request.hostname, request.port);
    if (clientfd < 0) {
        error("ERROR, while opening clientfd\n");
    }

    // Send request to server
    Rio_readinitb(&rio, clientfd);

    sprintf(buf, "GET %s HTTP/1.0\r\n", request.path);
    printf("%s", buf);
    Rio_writen(clientfd, buf, strlen(buf));

    sprintf(buf, "Host: %s\r\n", request.host);
    printf("%s", buf);
    Rio_writen(clientfd, buf, strlen(buf));

    sprintf(buf, "%s", user_agent_hdr);
    printf("%s", buf);
    Rio_writen(clientfd, buf, strlen(buf));

    sprintf(buf, "%s", connection_hdr);
    printf("%s", buf);
    Rio_writen(clientfd, buf, strlen(buf));

    sprintf(buf, "%s", proxy_connection_hdr);
    printf("%s", buf);
    Rio_writen(clientfd, buf, strlen(buf));

    while (curr != NULL) {
        sprintf(buf, "%s: %s\r\n", curr->key, curr->value);
        printf("%s", buf);
        Rio_writen(clientfd, buf, strlen(buf));
        curr = curr->next;
    }
    Rio_writen(clientfd, "\r\n", strlen("\r\n"));

    return clientfd;
}

void forward_response(int clientfd, int connfd)
{
    printf("\nforward_response\n");

    int n;
    char buf[MAXLINE];
    rio_t rio;
    
    // Read response from server and forward to client
    Rio_readinitb(&rio, clientfd);
    while ((n = Rio_readnb(&rio, buf, MAXLINE)) != 0) {
        printf("%s", buf);
        Rio_writen(connfd, buf, n);
    }
}

void *proxy_thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);

    http_request request = parse_request(connfd);
    int clientfd = forward_request(request);
    forward_response(clientfd, connfd);

    Close(connfd);
}