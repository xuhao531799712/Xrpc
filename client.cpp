#include<interface.hpp>

int main()
{
    client c("127.0.0.1" , 2019);
    c.start(3, 2);

    return 0;
}