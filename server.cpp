#include<interface.hpp>

int main(int argc, char* argv[])
{
    server s("127.0.0.1" , 2019);
    s.run();

    return 0;
}