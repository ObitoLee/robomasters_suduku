#include <iostream>
//Serialport need:
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

using namespace std;

//@brief:
//@brief:linux下的串口通信类，可以通过构造函数直接打开一个串口，并初始化（默认9600波特率，8位数据，无奇偶校验，1位停止位）
//             send()成员函数可以直接发送字符串，set_opt()更改参数。串口会在析构函数中自动关闭
//@example:Serialport exp("/dev/ttyUSB0");
//                  exp.set_opt(115200,8,'N',1);
//                  exp.send("1123dd");
class Serialport
{
public:
        Serialport(string port);//定义Serialport类的成员函数，
        Serialport();
        ~ Serialport();
        int open_port(string port);
        int set_opt(int nSpeed = 9600 , int nBits = 8, char nEvent ='N', int nStop = 1);
        bool send(char *str);
        bool sendAngle(short angleYaw,short anglePitch);
private:
         int fd ;
         char tmpchar[6];
         const char *buffer;
};

//@brief:linux下的串口通信类的成员函数。
//       open_port()成员函数可以打开一个串口，set_opt()更改参数。
//@example:open_port("/dev/ttyUSB0");
//         set_opt(115200, 8, 'N', 1);
Serialport::Serialport(string port)
{
        open_port(port);
        set_opt();
}

//Serialport下的成员函数open_port()的实现；
int Serialport::open_port(string port)
{
    // char *dev[]={"/dev/ttyS0","/dev/ttyS1","/dev/ttyS2"};
    // fd = open( "/dev/ttyS0", O_RDWR|O_NOCTTY|O_NDELAY);
    // fd = open( "/dev/ttyUSB0", O_RDWR|O_NOCTTY|O_NDELAY);
    fd = open(port.c_str() , O_RDWR|O_NOCTTY|O_NDELAY);
    if (-1 == fd)
    {
        printf("\n\e[31m\e[1m ERROR:打开串口%s失败 \e[0m\n\n",port.c_str());
		return -1;
    }
    else
    {
        fcntl(fd, F_SETFL, 0);
    }

    if(fcntl(fd, F_SETFL, 0) < 0)
        printf("fcntl failed!\n");
    else
        printf("fcntl=%d\n",fcntl(fd, F_SETFL,0));

    if(isatty(STDIN_FILENO)==0)
        printf("standard input is not a terminal device\n");
    else
        printf("isatty success!\n");

    //printf("fd-open=%d\n",fd);

    return fd;
}

    /*设置串口属性：
       fd: 文件描述符
       nSpeed: 波特率
       nBits: 数据位
       nEvent: 奇偶校验
       nStop: 停止位*/
int Serialport::set_opt(int nSpeed , int nBits, char nEvent , int nStop )
{
    struct termios newtio,oldtio;
    if ( tcgetattr( fd,&oldtio) != 0)
    {
        perror("SetupSerial error");
        return -1;
    }
    bzero( &newtio, sizeof( newtio ) );
    newtio.c_cflag |= CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;
    switch( nBits )
    {
    case 7:
        newtio.c_cflag |= CS7;
        break;
    case 8:
        newtio.c_cflag |= CS8;
        break;
    }
    switch( nEvent )
    {
    case 'O':
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= PARODD;
        newtio.c_iflag |= (INPCK | ISTRIP);
        break;
    case 'E':
        newtio.c_iflag |= (INPCK | ISTRIP);
        newtio.c_cflag |= PARENB;
        newtio.c_cflag &= ~PARODD;
        break;
    case 'N':
        newtio.c_cflag &= ~PARENB;
        break;
    }
    switch( nSpeed )
    {
    case 2400:
        cfsetispeed(&newtio, B2400);
        cfsetospeed(&newtio, B2400);
        break;
    case 4800:
        cfsetispeed(&newtio, B4800);
        cfsetospeed(&newtio, B4800);
        break;
    case 9600:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    case 115200:
        cfsetispeed(&newtio, B115200);
        cfsetospeed(&newtio, B115200);
        break;
    default:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    }

    if( nStop == 1 )
        newtio.c_cflag &= ~CSTOPB;
    else if ( nStop == 2 )
        newtio.c_cflag |= CSTOPB;

    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;
    tcflush(fd,TCIFLUSH);

    if((tcsetattr(fd,TCSANOW,&newtio))!=0)
    {
        perror("com set error");
        return -1;
    }
    printf("Serial port set done!\n");
    return 0;
}

bool Serialport::send(char *str)
{
    buffer = str;
    if(fd < 0 || write(fd, buffer, 1) < 0)
    {
        perror("\n\e[31m\e[1m ERROR:串口通信失败 \e[0m\n\n");
        return false;
    }
    return true;
}

bool Serialport::sendAngle(short _angle1,short _angle2)
{
    	memset(tmpchar, 0x00, sizeof(tmpchar));    //对tempchar清零
        tmpchar[0] = 0xAA;                                        //起始标志
        tmpchar[1] = _angle1 & 0x00ff;                      //第一个角度的低8位
        tmpchar[2] = _angle1 >> 8;                            //第一个角度的高8位
        tmpchar[3] = _angle2 & 0x00ff;                      //第二个角度的低8位
        tmpchar[4] = _angle2 >> 8;                          //第二个角度的高8位
        tmpchar[5] = 0xBB;                                        //结束标志
        if(send(tmpchar))
            return true;
        else
            return false;
}

Serialport:: ~Serialport()
{
    close(fd);
}

