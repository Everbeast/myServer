#include <iostream>
#include "http_conn.h"

//响应状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";
const char* doc_root = "./html"; //网站根目录

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn()
{
    // if(real_close && (m_sockfd != -1))
    // {
    //     removefd(m_epollfd, m_sockfd);
    //     m_sockfd = -1;
    //     m_user_count--;
    // }
    if(m_sockfd != -1){
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::init(int sockfd, const sockaddr_in& addr)
{
    std::cout<<"init :" <<sockfd<<std::endl;
    m_sockfd = sockfd;
    m_address = addr;
    m_user_count++;

    init();
}

void http_conn::init()
{
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;

    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for(; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r')
        {
            if((m_checked_idx + 1) == m_read_idx)
            {
                //缓存中的idx到了最后
                return LINE_OPEN; //表示缓存中的字符已经checked完毕
            }
            else if(m_read_buf[m_checked_idx + 1] == '\n')
            {
                //遇到了\r\n 则将\r\n换成\0\0 ，分割
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        // else if(temp == '\n')
        // {
        //     if((m_checked_idx > 1) && (m_read_buf[m_checked_idx-1] == '\r'))
        //     {
        //         m_read_buf[m_checked_idx-1] = '\0';
        //         m_read_buf[m_checked_idx++] = '\0';
        //         return LINE_OK;
        //     }
        //     return LINE_BAD;
        // }
    }
    return LINE_OPEN;
}

//读取客户数据，知道没有数据可读或者对方关闭链接
bool http_conn::read()
{
    if(m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }

    int bytes_read = 0;
    while(true)
    {
        // std::cout<<"bytes recv:" << m_read_idx<<std::endl;
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE-m_read_idx, 0);
        if(bytes_read == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)//?
            {
                break;
            }
            return false;
        }
        else if(bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

//解析请求行，获得请求方法 目标url http版本
http_conn::HTTP_CODE http_conn::parse_request_line(char* text)
{
    //GET /js/helper.js HTTP/1.1
    m_url = strpbrk(text, " \t"); //在text找到第一个含有' \t'的位置 此处为4
    if(!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0'; 

    char* method = text;
    if(strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else
    {
        return BAD_REQUEST;
    }
    m_url += strspn(m_url, " \t"); //返回m_url中含有' \t'的长度；即+第一个 \t的长度，m_url就指着"/"
    // std::cout<<"in parse request line";
    m_version = strpbrk(m_url, " \t");//找/js/helper.js HTTP/1.1 第一个 \t的位置 即s后一个位置
    // std::cout<<m_url<<std::endl;
    // std::cout<<m_version<<std::endl;
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++ = '\0'; //分割
    m_version += strspn(m_version, " \t"); // 加上第一个 \t的长度，即m_version指向"H"
    if(strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }

    std::cout<<m_url<<std::endl;
    if ( strncasecmp( m_url, "http://", 7 ) == 0 )
    {
        m_url += 7;
        m_url = strchr( m_url, '/' );
    }
    // std::cout<<m_url<<std::endl;
    // if(strncasecmp(m_url, "/index.html", 10) == 0){
    //     return GET_REQUEST;
    // }

    // 不以 "/"开头
    if ( ! m_url || m_url[ 0 ] != '/' )
    {
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER; // 改变状态机 转去调用parse header
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char* text)
{
    //空行表示解析完毕
    if(text[0] == '\0')
    {
        //若有消息体则读取，并转移状态
        if(m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        //否则说明已得到完整的请求
        return GET_REQUEST;
    }
    //处理connection
    else if(strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0)
            m_linger = true;
    }
    //处理content lenght头部字段
    else if(strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    //处理host头部字段
    else if(strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        // printf("unknown head %s \n", text);
    }
    return NO_REQUEST;
}

//仅判断是否完全读入
http_conn::HTTP_CODE http_conn::parse_content(char* text)
{
    if(m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}


http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while ( ( ( m_check_state == CHECK_STATE_CONTENT ) && ( line_status == LINE_OK  ) )
                || ( ( line_status = parse_line() ) == LINE_OK ) )
    {
        text = get_line();
        m_start_line = m_checked_idx;
        // printf( "got 1 http line: %s\n", text );
        // printf("state %d\n", m_check_state);

        switch ( m_check_state )
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line( text );
                // std::cout<<"check state requestline:" << ret << std::endl;
                if ( ret == BAD_REQUEST )
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret = parse_headers( text );
                // std::cout<<"parse headers ret"<< ret << std::endl;
                if ( ret == BAD_REQUEST )
                {
                    return BAD_REQUEST;
                }
                else if ( ret == GET_REQUEST )
                {
                    std::cout<<"do request"<<std::endl;
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content( text );
                if ( ret == GET_REQUEST )
                {
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }

    return NO_REQUEST;
}

//分析目标文件，都正常就使用mmap将其银蛇到内存地址m_file_address处，告诉调用者成功获取文件
http_conn::HTTP_CODE http_conn::do_request()
{
    if(strcasecmp(m_url, "/") == 0){
        m_url = "/index.html";
    }
    strcpy( m_real_file, doc_root );
    int len = strlen( doc_root );
    strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );//从m_real_file + len开始的位置复制m_url
    if ( stat( m_real_file, &m_file_stat ) < 0 )
    {   
        std::cout<<"no source" << std::endl;
        return NO_RESOURCE;
    }

    if ( ! ( m_file_stat.st_mode & S_IROTH ) )
    {
        return FORBIDDEN_REQUEST;
    }

    if ( S_ISDIR( m_file_stat.st_mode ) )
    {
        return BAD_REQUEST;
    }

    int fd = open( m_real_file, O_RDONLY );
    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    // std::cout<<"file request"<<std::endl;
    return FILE_REQUEST;
}
//unmap
//关于mmap：https://www.jianshu.com/p/755338d11865
void http_conn::unmap()
{
    if( m_file_address )
    {
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = nullptr;
    }
}
//http响应
//集中写 第六章
bool http_conn::write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    std::cout<<"in write"<<std::endl;
    std::cout<<m_write_idx<<std::endl;
    if ( bytes_to_send == 0 )//没有数据发送 等待下轮epollin后 并重置writeidx readidx
    {
        modfd( m_epollfd, m_sockfd, EPOLLIN );
        init();
        return true;
    }

    while( 1 )
    {
        temp = writev( m_sockfd, m_iv, m_iv_count );
        if ( temp <= -1 )
        {
            //tcp写缓存没有空间，等待下一轮epollout
            if( errno == EAGAIN )
            {
                modfd( m_epollfd, m_sockfd, EPOLLOUT );
                return true;
            }
            unmap();
            return false;
        }

        bytes_to_send -= temp;
        bytes_have_send += temp;
        // std::cout<<bytes_to_send<<" "<<bytes_have_send<<std::endl;

        //发送http响应成功
        if ( bytes_to_send <= bytes_have_send )
        {
            unmap();
            if( m_linger ) //若是keep alive 则保持连接
            {
                init();
                modfd( m_epollfd, m_sockfd, EPOLLIN );
                return true;
            }
            else
            {
                modfd( m_epollfd, m_sockfd, EPOLLIN );
                return false;
            } 
        }
    }
}

//往缓存中写入待发送数据
/*
#include <stdarg.h>
void test_char(const char* format, ...){
    char buf[512];
 
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
 
    if (strlen(buf) >= sizeof(buf) - 1) {
        printf("buffer may have been truncated \n");
    }
    printf("%s \n",buf);
}
int main()
{
    char test1[5]="is";
    char test2[5]="char";
    char test3[5]="test";
    int testint = 5;
    test_char("This %s %s %s %d",test1,test2,test3,testint);
}

*/
bool http_conn::add_response( const char* format, ... )
{
    if( m_write_idx >= WRITE_BUFFER_SIZE )
    {
        return false;
    }
    va_list arg_list;
    va_start( arg_list, format );
    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list );
    if( len >= ( WRITE_BUFFER_SIZE - 1 - m_write_idx ) )
    {
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );
    return true;
}

bool http_conn::add_status_line( int status, const char* title )
{
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}

bool http_conn::add_headers( int content_len )
{
    add_content_length( content_len );
    add_response("Content-Type: %s\r\n", "text/html");
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length( int content_len )
{
    return add_response( "Content-Length: %d\r\n", content_len );
}

bool http_conn::add_linger()
{
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}

bool http_conn::add_blank_line()
{
    return add_response( "%s", "\r\n" );
}

bool http_conn::add_content( const char* content )
{
    return add_response( "%s", content );
}

//更具处理http请求的结果，决定返回客户端的内容
bool http_conn::process_write( HTTP_CODE ret )
{
    switch ( ret )
    {
        case INTERNAL_ERROR:
        {
            add_status_line( 500, error_500_title );
            add_headers( strlen( error_500_form ) );
            if ( ! add_content( error_500_form ) )
            {
                return false;
            }
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line( 400, error_400_title );
            add_headers( strlen( error_400_form ) );
            if ( ! add_content( error_400_form ) )
            {
                return false;
            }
            break;
        }
        case NO_RESOURCE:
        {
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) )
            {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line( 403, error_403_title );
            add_headers( strlen( error_403_form ) );
            if ( ! add_content( error_403_form ) )
            {
                return false;
            }
            break;
        }
        case FILE_REQUEST:
        {
            // char header[2048];
            // bzero(header, '\0');
            // sprintf(header, "%s%s %d %s\r\n",header, "HTTP/1.1", 200, "OK");
            // sprintf(header, "%sContent-length: %d\r\n\r\n", header, strlen(INDEX_PAGE));
            // sprintf(header, "%s%s", header, INDEX_PAGE);
            // send(m_sockfd, header, strlen(header), 0);
            // return false;
            std::cout<<"in file request"<<std::endl;
            add_status_line( 200, ok_200_title );
            if ( m_file_stat.st_size != 0 )
            {
                add_headers( m_file_stat.st_size );
                m_iv[ 0 ].iov_base = m_write_buf;
                m_iv[ 0 ].iov_len = m_write_idx;
                m_iv[ 1 ].iov_base = m_file_address;
                m_iv[ 1 ].iov_len = m_file_stat.st_size;
                // std::cout<<m_write_idx<<std::endl;
                m_iv_count = 2;
                return true;
            }
            else
            {
                const char* ok_string = "<html><body></body></html>";
                add_headers( strlen( ok_string ) );
                if ( ! add_content( ok_string ) )
                {
                    return false;
                }
            }
        }
        default:
        {
            return false;
        }
    }

    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

//由线程池的worker线程调用，是处理http请求的入口
// void http_conn::process()
// {
//     std::cout<<"in process"<<std::endl;
//     std::cout<<"user count: "<<m_user_count<<std::endl;
//     HTTP_CODE read_ret = process_read();
//     std::cout<<"process read return" << read_ret<<std::endl;
//     if ( read_ret == NO_REQUEST )
//     {
//         modfd( m_epollfd, m_sockfd, EPOLLIN );
//         return;
//     }


//     bool write_ret = process_write( read_ret );
//     if ( ! write_ret )
//     {
//         std::cout<<"close"<<std::endl;
//         close_conn(true);
//     }
//     // std::cout<<write_ret<<std::endl;
    

//     modfd( m_epollfd, m_sockfd, EPOLLOUT );


// }



