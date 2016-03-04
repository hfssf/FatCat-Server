#!/bin/sh

INCLUDE="-I/home/hf02/Qt/000111/LIBS/threadpool-0_2_5-src/threadpool/boost"\
" -I/usr/local/log4cpp/include"\
" -I/home/hf02/Qt/000111/LIBS/hiredis-master"\
" -I/usr/include/boost"\
" -I/home/hf02/Qt/000111/LIBS/postgresql"


LIBS="/usr/lib/libboost_system.a"\
" /usr/lib/libboost_thread.a"\
" /home/hf02/Qt/000111/LIBS/libhiredis.a"\
" /usr/local/log4cpp/lib/liblog4cpp.a"\
" /usr/lib64/libpq.so"


echo $INCLUDE
g++ -c *.cpp $INCLUDE
g++ -c ./Game/*.cpp $INCLUDE
g++ -c ./GameAttack/*.cpp $INCLUDE
g++ -c ./GameChat/*.cpp $INCLUDE
g++ -c ./GameInterchange/*.cpp $INCLUDE
g++ -c ./GameTask/*.cpp $INCLUDE
g++ -c ./memManage/*.cpp $INCLUDE
g++ -c ./Monster/*.cpp $INCLUDE
g++ -c ./NetWork/*.cpp $INCLUDE
g++ -c ./OperationGoods/*.cpp $INCLUDE
g++ -c ./OperationPostgres/*.cpp $INCLUDE
g++ -c ./PlayerLogin/*.cpp $INCLUDE
g++ -c ./TeamFriend/*.cpp $INCLUDE

g++ -std=c++11 *.o $LIBS -o Server  -lpthread

 
