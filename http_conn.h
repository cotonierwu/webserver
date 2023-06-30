#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>
#include <string.h>
#include "locker.h"

class http_conn
{

public:
    static int m_epollfd;                      // 所有socket上的事件都被注册到同一个epoll对象上
    static int m_user_count;                   // 统计用户数量
    static const int READ_BUFFER_SIZE = 2048;  // 读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 2048; // 写缓冲区大小
    static const int FILENAME_LEN = 200;       // 文件名的最大长度

    // 状态机部分
    /*
    HTTP请求方法
    */
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTINONS,
        CONNECT
    };
    /*
    解析状态机请求时，主状态机的状态
    CHECK_STATE_REQUESTLINE:当前正在分析请求行
    CHECK_STATE_HEADER:当前正在分析请求头
    CHECK_STATE_CONTENT:当前正在分析请求体
    */
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    /*
    从状态机的三种状态，行的读取状态
    */

    enum LINE_STATUS
    {
        LINE_OK = 0, // 读取一行成功
        LINE_BAD,    // 读取出错
        LINE_OPEN    // 行数据不完整
    };

    // 解析的结果
    /*
    NO_REQUEST:请求不完整
    GET_REQUEST:获得正确的请求
    BAD_REQUEST:客户请求语法错误
    NO_RESOURCE:服务器没有资源
    FORBIDDEN_REQUEST:没有访问权限
    FILE_REQUEST:文件请求，获取文件成功
    INTERNAL_ERROR:服务器内部错误
    CLOSE_CONNECTION:客户端断开连接
    */
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSE_CONNECTION
    };

    http_conn() {}
    ~http_conn() {}
    void process();
    // 初始化连接
    void init(int sockfd, const sockaddr_in &addr);
    void close_conn();
    bool read();
    bool write();

private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
    int m_read_idx;                    // 标识下一次读的起始位置

    int m_check_idx;  // 当前解析的字符在读缓冲区的位置
    int m_line_start; // 当前解析的行的起始位置
    CHECK_STATE m_check_state;

    METHOD m_method;
    char *m_url;
    char *m_version; // 只支持HTTP1.1
    char *m_host;
    bool m_linger;
    int m_content_length;
    char m_real_file[FILENAME_LEN]; // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录

    char m_write_buf[WRITE_BUFFER_SIZE]; // 写缓冲区
    int m_write_idx;                     // 写缓冲区中待发送的字节数
    char *m_file_address;                // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat m_file_stat;             // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec m_iv[2];                // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量。
    int m_iv_count;

    int bytes_to_send;   // 将要发送的数据的字节数
    int bytes_have_send; // 已经发送的字节数

    void init();                              // 初始化连接其余的数据
    HTTP_CODE process_read();                 // 解析http请求
    HTTP_CODE parse_request_line(char *text); // 解析请求行
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    LINE_STATUS parse_line(); // 解析某一行
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_line_start; }

    bool process_write(HTTP_CODE ret); // 填充HTTP应答
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_content_type();
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
};

#endif