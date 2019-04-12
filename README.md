# linux-server
## 联系我:(qq:1648305422) 
    a server with epoll + multiProcess demo(一个非阻塞epoll + 进程池模型服务器) 
#### 简介: 
    使用c语言实现一个linux下的服务器程序.
#### 架构:
    一个主进程 + 和cpu核心数一样的worker进程,每个worker进程创建一个epoll用来监听新连接的
    客户端请求和读事件\写事件.
#### 编译: 
    目前只在ubuntu系统下测试过,其他linux估计也可以. 
    gcc list.h main.c  
#### 运行:  
    sudo ./a.out 
#### 测试: 
    1.浏览器可以直接返回结果(谷歌浏览器请求一次返回两次(插件的关系),其他浏览器正常) http://127.0.0.1:8080/ 
    2.ab压力测试   ab -n 100 -c 100 http://127.0.0.1:8080/ 
