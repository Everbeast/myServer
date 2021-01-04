#ifndef HTTPSERVER_H
#define HTTPSERVER_H


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <unordered_map>
#include <functional>

#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"
#include "util.h"


#define MAX_FD 65536
#define MAX_EVENT_NUMBER 1024

class HttpServer {
public:
    HttpServer(const char* ip, const int port);

    ~HttpServer();
    void serve();

private:
    bool init_socket();
    void handle_listen();
    void process(http_conn* client);

    std::unordered_map<int, http_conn> m_users;
    uint32_t m_event;

    // int timeout;
    int m_port;
    const char* m_ip;
    int m_listenfd;
    int m_epollfd;
    epoll_event m_events[ MAX_EVENT_NUMBER ];
    // std::unique_ptr<ThreadPool> m_threadpool;
    ThreadPool* m_threadpool;
    bool m_stop;
    // char* srcDir_;
    // Epoller* epoller_;
    // std::unique_ptr<HeapTimer> timer_;    
};


#endif