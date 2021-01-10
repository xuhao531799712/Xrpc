#include<string>
#include<iostream>

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>

#include <msgpack.hpp>
#include <utility>

#define MAXPACKSIZE 1024


class client : public std::enable_shared_from_this<client> {
public:
    client(std::string ip_, int port_)  : ip(std::move(ip_)), port(port_), result(0)
    {
        memset(buffer, '\0', MAXPACKSIZE);
        *len_ = '\0'; // 初始化成员变量

        struct sockaddr_in server_addr{};
        bzero(&server_addr, sizeof(server_addr));
        server_addr.sin_family=AF_INET;
        inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);
        server_addr.sin_port=htons(port);

        sock=socket(PF_INET, SOCK_STREAM, 0);
        assert(sock>=0);

        int ret=connect(sock, (sockaddr*)&server_addr, sizeof(server_addr));
        assert(ret!=-1);
    }

    int start(int a, int b) { // 整个流程为：传入两个整形数据，发送给服务端，服务端再返回一个数据。
        construct_rpc_data(a , b); // 构造发送给服务端的数据，包含包的长度和序列化之后的数据。整个包存储在成员变量 buffer 里面
        send_recive_rpc_data(); // 接受来自服务端的信息，存储在成员变量 result 里面
        std::cout << "send_recive_rpc_data 返回值：" << result << std::endl;
        return result;
    }

private:
    void construct_rpc_data(int a, int b) // 构造 msgpack 包，获取 msgpack 的长度，将以上两个信息写入成员变量 buffer
    {
        std::tuple<int, int>  src(a,b);
        std::stringstream sbuffer;
        msgpack::pack(sbuffer, src);
        std::string strbuf(sbuffer.str());

        std::cout << " len " << strbuf.size() << std::endl;
        size_t len_bigend = htonl(strbuf.size());
        memcpy(buffer, &len_bigend, 4);
        memcpy(buffer + 4, strbuf.data(), strbuf.size());
        len = strbuf.size() + 4;
    }
    void send_recive_rpc_data() // 将数据发送给服务端，并接受来自服务端的信息
    {
        send(sock, buffer, len, 0);
        std::cout << "客户端发送数据 : " << buffer << std::endl;
        recive_rpc_data(); // 接收数据
    }

    void recive_rpc_data() {
        std::cout << "发送完毕，开始接受数据" << std::endl;
        int ret = recv(sock, len_, 4, 0);
        std::cout << "收到包头 : " << len_ << std::endl;

        len = ntohl(int(*(int*)len_)); // 转换为主机字节序， char为1个字节，此处将 char* 转为 int* 后就将 len_[4] 作为一个整 int 数进行读取了
        std::cout << "解析包头得包体长度 : " << len << std::endl;
        ret = recv(sock, buffer, len, 0);

        std::cout << "数据读取完成" << std::endl;
        handle_rpc_data(); // 对读到的数据进行处理
    }

    void handle_rpc_data() {

//        std::cout << "读到数据：" << buffer << std::endl;
        std::cout << "读到数据：" << std::endl;
        msgpack::object_handle  msg = msgpack::unpack(buffer, len);
        auto tp = msg.get().as<std::tuple<int>>();
        std::cout << " magpack " << std::get<0>(tp) << std::endl;
        result = std::get<0>(tp);
    }

private:
    char buffer[MAXPACKSIZE]{};
    std::string ip;
    int port;
    int sock;
    char len_[4]{};
    int len{};
    int result;
};





class session : public std::enable_shared_from_this<session> {
public:
    session(int connfd_, struct  sockaddr_in client_) : connfd(connfd_),client(client_)
    {
        memset(buffer, '\0', MAXPACKSIZE);
        *len_ = '\0'; // 初始化成员变量
    }


    void start() {
        start_chains(); // 开始 读取头部 -> 读取 msgpack 包 -> 读取头部 的循环

    }

private:
    void start_chains()
    {
        read_msgpack_len();
    }


    void read_msgpack_len() // 读取包的长度
    {
        // 前4个字节表示整个包体的长度
        int ret = recv(connfd, len_, 4, 0);

        std::cout << inet_ntoa(client.sin_addr) << ":" << client.sin_port << " length data reception completed" << std::endl;
        std::cout << inet_ntoa(client.sin_addr) << ":" << client.sin_port << " orginal data " << len_ << std::endl;

        len = ntohl(int(*(int*)len_)); // 转换为主机字节序， char为1个字节，此处将 char* 转为 int* 后就将 len_[4] 作为一个整 int 数进行读取了
        std::cout << inet_ntoa(client.sin_addr) << ":" << client.sin_port << " length " << len << std::endl;

        read_msgpack(); // 读取 msgpack 包

    }

    void read_msgpack()
    {
        int ret = recv(connfd, buffer, len, 0);

        std::cout << inet_ntoa(client.sin_addr) << ":" << client.sin_port << " msgpack data reception completed" << std::endl;
        std::cout << inet_ntoa(client.sin_addr) << ":" << client.sin_port << " orginal data " << buffer << std::endl;

        msg = msgpack::unpack(buffer, len); // 反序列化

        send_to_client();
    }

    void send_to_client()
    {
        std::cout << "enter send_to_client" << std::endl;
        auto tp = msg.get().as<std::tuple<int, int> >();

        std::cout << inet_ntoa(client.sin_addr) << ":" << client.sin_port << " msgpack " << std::get<0>(tp) << " " << std::get<1>(tp) << std::endl;

        int result = std::get<0>(tp) + std::get<1>(tp); // 要发送的数据

        std::tuple<int>  src(result);
        std::stringstream sbuffer;
        msgpack::pack(sbuffer, src); // 序列化

        std::string strbuf(sbuffer.str());

        std::cout << inet_ntoa(client.sin_addr) << ":" << client.sin_port << " server serialization completed : " << strbuf << std::endl;

        size_t len_bigend = htonl(strbuf.size());
        memcpy(buffer, &len_bigend, 4);
        memcpy(buffer + 4, strbuf.data(), strbuf.size());
        std::cout << inet_ntoa(client.sin_addr) << ":" << client.sin_port << " server serialization length" << strbuf.size() << std::endl;
        std::cout << "服务器发送数据 : " << buffer << std::endl;

        int res = send(connfd, buffer, strbuf.size() + 4, 0);

        std::cout << inet_ntoa(client.sin_addr) << ":" << client.sin_port <<" server sent successfully" << std::endl;
    }

private:
    char buffer[MAXPACKSIZE]{};
    char len_[4]{};
    int len{};
    msgpack::object_handle  msg;

    int connfd;
    struct  sockaddr_in client;
};

class server {
public:
    server(std::string ip_, int port_)  : ip(std::move(ip_)), port(port_)
    {
        struct sockaddr_in address{};
        bzero(&address, sizeof(address));
        address.sin_family=AF_INET;
        inet_pton(AF_INET, ip.c_str(), &address.sin_addr);
        address.sin_port=htons(port);

        sock=socket(PF_INET, SOCK_STREAM, 0);
        assert(sock>=0);


        int ret = bind(sock,(sockaddr*)&address,sizeof(address));
        assert(ret!=-1);

        ret=listen(sock,5);
        assert(ret!=-1);
    }

    static void handle_accept(int connfd, struct  sockaddr_in &client) {
        std::shared_ptr<session> new_session = std::make_shared<session>(connfd, client);
        new_session->start(); // 调用 session 类的 start() 成员函数
    }

    void run() const {
        while (run_flag) {
            struct sockaddr_in client{};
            socklen_t client_addrlength=sizeof(client);
            int connfd=accept(sock,(sockaddr*)&client,&client_addrlength);
            if(connfd<=0)
            {
                printf("errno is: %d\n", errno);
            }
            else
            {
                handle_accept(connfd, client);

                close(connfd);
            }
        }
    }

private:
    std::string ip;
    int port;
    int sock;
    bool run_flag = true;
};