
#include "http_server.h"
/*
作用：声明外部变量。使变量或对象可以被跨文件访问

c++语言支持分离式编译机制，该机制允许将程序分割为若干个文件，每个文件可被独立编译。

因此在编译期间，各个文件中定义的全局变量互相不透明，也就是说，在编译时，全局变量的可见域限制在文件内部。

对于A.cpp和B.cpp中，分别含有同名全局变量i，两个cpp文件会成功编译，但是在链接时会将两个cpp合二为一就会出现错误。这是因为同名存在重复定义。

如果在其中一个变量前添加extern关键字进行编译，再次进行链接时就会成功。完成了变量跨文件访问。
*/




// extern void addfd( int epollfd, int fd, bool one_shot );
// extern void removefd( int epollfd, int fd );
HttpServer::HttpServer(const char* ip, const int port): 
                      m_ip(ip), m_port(port), m_epollfd(-1), m_threadpool(new ThreadPool), m_stop(false){
    init_socket();
    m_event = EPOLLRDHUP | EPOLLET;
};

HttpServer::~HttpServer(){
    close(m_listenfd);
    close(m_epollfd);
    m_stop = true;
}


bool HttpServer::init_socket(){
    int ret;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, m_ip, &address.sin_addr );
    address.sin_port = htons( m_port );

    m_listenfd = socket( AF_INET, SOCK_STREAM, 0 );
    assert( m_listenfd >= 0 );
    // struct linger tmp = { 1, 0 };
    struct linger optLinger = { 0 };
    // if(openLinger_) {
    //     /* 优雅关闭: 直到所剩数据发送完毕或超时 */
    //     optLinger.l_onoff = 1;
    //     optLinger.l_linger = 1;
    // }
    ret = setsockopt( m_listenfd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof( optLinger ) );
    assert(ret >= 0);

    //port reuse
    int optval = 1;
    ret = setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(optval));
    assert(ret >= 0);

    ret = bind( m_listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret >= 0 );

    ret = listen( m_listenfd, 5 );
    assert( ret >= 0 );


    m_epollfd = epoll_create( MAX_EVENT_NUMBER );
    // AddFd(m_epollfd, m_listenfd,  listenEvent_ | EPOLLIN);
    // setnonblocking(m_listenfd);
    // assert( m_epollfd != -1 );
    addfd( m_epollfd, m_listenfd, false, m_event);
    // http_conn::m_epollfd = m_epollfd;
    
    return true;
}

void HttpServer::handle_listen(){
    std::cout<<"handle listen"<<std::endl;
    struct sockaddr_in client_address;
    socklen_t len = sizeof( client_address );
    int connfd = accept( m_listenfd, ( struct sockaddr* )&client_address, &len );
    if ( connfd < 0 )
    {
        printf( "errno is: %d\n", errno );
        return;
    }
    m_users[connfd].init( connfd, client_address );
    addfd( m_epollfd, connfd, true, m_event);
    // AddClient_(connfd, client_address);

}

void HttpServer::process(http_conn* client){
    std::cout<<"in process"<<std::endl;
    // std::cout<<"user count: "<<m_user_count<<std::endl;
    http_conn::HTTP_CODE read_ret = client->process_read();
    std::cout<<"process read return" << read_ret<<std::endl;
    if ( read_ret == http_conn::NO_REQUEST )
    {
        modfd( client->m_epollfd, client->m_sockfd, EPOLLIN );
        return;
    }


    bool write_ret = client->process_write( read_ret );
    if ( ! write_ret )
    {
        std::cout<<"close"<<std::endl;
        client->close_conn();
    }
    // std::cout<<write_ret<<std::endl;
    

    modfd( client->m_epollfd, client->m_sockfd, EPOLLOUT );
}


void HttpServer::serve()
{
    //忽略sigpipe信号
    // addsig( SIGPIPE, SIG_IGN );
    
    http_conn::m_epollfd = m_epollfd;
    // std::unordered_map<int, http_conn*> http_conn_map;
    // assert(users);
    // int user_count = 0;

    

    while (!m_stop)
    {
        int timeMs = -1;
        // if(timeout > 0){
        //     timeMs = timer_->GetNextTick();
        // }
        int number = epoll_wait(m_epollfd, m_events, MAX_EVENT_NUMBER, timeMs);
        // int number = epoller_->Wait(timeMs);
        if((number < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; i++)
        {
            int sockfd = m_events[i].data.fd;
            // int sockfd = epoller_->GetEventFd(i);
            // auto events = epoller_->GetEvents(i);
            if(sockfd == m_listenfd)
            {
                handle_listen();
                // DealListen_();

            }
            else if(m_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                m_users[sockfd].close_conn();
            }
            else if(m_events[i].events & EPOLLIN)
            {
                if(m_users[sockfd].read())
                {
                    m_threadpool->append(std::bind(&HttpServer::process, this, &m_users[sockfd]));
                }
                else
                {
                    std::cout<<"close in read"<<std::endl;
                    m_users[sockfd].close_conn();
                }
                // DealRead_(&m_users[sockfd]);
                
            }
            else if(m_events[i].events & EPOLLOUT)
            {
                std::cout<<"epollout"<<std::endl;
                if(!m_users[sockfd].write()){
                    m_users[sockfd].close_conn();
                }   
            }
            else
            {
                
            }
            
        }
    }
}


// void HttpServer::SendError_(int fd, const char*info) {
//     assert(fd > 0);
//     int ret = send(fd, info, strlen(info), 0);
//     if(ret < 0) {
//         LOG_WARN("send error to client[%d] error!", fd);
//     }
//     close(fd);
// }

// void HttpServer::CloseConn_(HttpConn* client) {
//     assert(client);
//     // LOG_INFO("Client[%d] quit!", client->GetFd());
//     // DelFd(m_epollfd,client->GetFd());
//     removefd(m_epollfd, client->GetFd());
//     client->Close();
// }

// void HttpServer::AddClient_(int fd, sockaddr_in addr) {
//     assert(fd > 0);
//     m_users[fd].init(fd, addr);
//     // if(timeout > 0) {
//     //     timer_->add(fd, timeout, std::bind(&HttpServer::CloseConn_, this, &m_users[fd]));
//     // }
//     // AddFd(m_epollfd, fd, EPOLLIN | connEvent_);
//     // setnonblocking(fd);
//     addfd( m_epollfd, fd, true, m_event);
//     // SetFdNonblock(fd);
//     // LOG_INFO("Client[%d] in!", users_[fd].GetFd());
// }

// void HttpServer::DealListen_() {
//     struct sockaddr_in addr;
//     socklen_t len = sizeof(addr);
//     do {
//         int fd = accept(m_listenfd, (struct sockaddr *)&addr, &len);
//         if(fd <= 0) { return;}
//         // else if(HttpConn::userCount >= MAX_FD) {
//         //     // SendError_(fd, "Server busy!");
//         //     // LOG_WARN("Clients is full!");
//         //     return;
//         // }
//         AddClient_(fd, addr);
//     } while(m_listenfd & EPOLLET);
// }

// void HttpServer::DealRead_(HttpConn* client) {
//     assert(client);
//     // ExtentTime_(client);
//     m_threadpool->append(std::bind(&HttpServer::OnRead_, this, client));
// }

// void HttpServer::DealWrite_(HttpConn* client) {
//     assert(client);
//     // ExtentTime_(client);
//     m_threadpool->append(std::bind(&HttpServer::OnWrite_, this, client));
// }

// // void HttpServer::ExtentTime_(HttpConn* client) {
// //     assert(client);
// //     if(timeout > 0) { timer_->adjust(client->GetFd(), timeout); }
// // }

// void HttpServer::OnRead_(HttpConn* client) {
//     assert(client);
//     int ret = -1;
//     int readErrno = 0;
//     std::cout<<"on read" << std::endl;
//     ret = client->read(&readErrno);
//     if(ret <= 0 && readErrno != EAGAIN) {
//         CloseConn_(client);
//         return;
//     }
//     OnProcess(client);
// }

// void HttpServer::OnProcess(HttpConn* client) {
//     std::cout<<"on process"<<std::endl;
//     if(client->process()) {
//         std::cout<<"change epollout"<<std::endl;
//         // ModFd(m_epollfd, client->GetFd(), connEvent_ | EPOLLOUT);
//         modfd( m_epollfd, client->GetFd(), EPOLLOUT );
//     } else {
//         // ModFd(m_epollfd, client->GetFd(), connEvent_ | EPOLLIN);
//         std::cout<<"change epollin"<<std::endl;
//         modfd( m_epollfd, client->GetFd(), EPOLLIN );
//     }
// }

// void HttpServer::OnWrite_(HttpConn* client) {
//     std::cout<<"on write" <<std::endl;
//     assert(client);
//     int ret = -1;
//     int writeErrno = 0;
//     ret = client->write(&writeErrno);
//     if(client->ToWriteBytes() == 0) {
//         /* 传输完成 */
//         if(client->IsKeepAlive()) {
//             OnProcess(client);
//             return;
//         }
//     }
//     else if(ret < 0) {
//         if(writeErrno == EAGAIN) {
//             /* 继续传输 */
//             // ModFd(m_epollfd, client->GetFd(), connEvent_ | EPOLLOUT);
//             modfd( m_epollfd, client->GetFd(), EPOLLOUT );
//             return;
//         }
//     }
//     CloseConn_(client);
// }
/*
EPOLLOUT事件表示fd的发送缓冲区可写，在一次发送大量数据（超过发送缓冲区大小）的情况下很有用。
要理解该事件的意义首先要清楚一下几个知识：1、多路分离器。多路分离器存在的意义在于可以同时监测多个fd的事件，
便于单线程处理多个fd，epoll是众多多路分离器的一种，类似的还有select、poll等。
服务器程序通常需要具备较高处理用户并发的能力，使用多路分离器意味着可以用一个线程同时处理多个用户并发请求。

2、非阻塞套接字。   
 2.1 阻塞。         
 在了解非阻塞之前先了解一下阻塞，阻塞指的是用户态程序调用系统api进入内核态后，
 如果条件不满足则被加入到对应的等待队列中，直到条件满足。比如：sleep 2s。
 在此期间线程得不到CPU调度，自然也就不会往下执行，表现的现象为线程卡在系统api不返回。   
 2.2 非阻塞。         
 非阻塞则相反，不论条件是否满足都会立即返回到用户态，线程的CPU资源不会被剥夺，也就意味着程序可以继续往下执行。   
 2.3、高性能。在一次发送大量数据（超过发送缓冲区大小）的情况下，如果使用阻塞方式，程序一直阻塞，
 直到所有的数据都写入到缓冲区中。例如，要发送M字节数据，套接字发送缓冲区大小为B字节，
 只有当对端向本机返回ack表明其接收到大于等于M-B字节时，才意味着所有的数据都写入到缓冲区中。
 很明显，如果一次发送的数据量非常大，比如M=10GB、B=64KB，则：
 1）一次发送过程中本机线程会在一个fd上阻塞相当长一段时间，其他fd得不到及时处理；
 2）如果出现发送失败，无从得知到底有多少数据发送成功，应用程序只能选择重新发送这10G数据，
 结合考虑网络的稳定性，只能呵呵；总之，上述两点都是无法接受的。因此，对性能有要求的服务器一般不采用阻塞而采用非阻塞。
 
3、使用非阻塞套接字时的处理流程。    采用非阻塞套接字一次发送大量数据的流程：
1）使劲往发送缓冲区中写数据，直到返回不可写；
2）等待下一次缓冲区可写；
3）要发送的数据写完；    
 其中2）可以有两种方式：
  a）查询式，程序不停地查询是否可写；
  b）程序去干其他的事情（多路分离器的优势所在），等出现可写事件后再接着写；很明显方式b）更加优雅。

4、EPOLLOUT事件的用途。    EPOLLOUT事件就是以事件的方式通知用户程序，可以继续往缓冲区写数据了。
*/
    