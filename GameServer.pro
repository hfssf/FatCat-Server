TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt


QMAKE_CXXFLAGS += -std=c++0x -USTRICT_ANSI

#QMAKE_CXXFLAGS += -g

SOURCES += main.cpp \
    memManage/diskdbmanager.cpp \
    memManage/idbmanager.cpp \
    memManage/memdbmanager.cpp \
    NetWork/netclient.cpp \
    NetWork/tcpconnection.cpp \
    NetWork/tcpserver.cpp \
    md5.cpp \
    rpc.cpp \
    server.cpp \
    Monster/monster.cpp \
    Game/getdefinevalue.cpp \
    GameAttack/gameattack.cpp \
    GameTask/gametask.cpp \
    PlayerLogin/playerlogin.cpp \
    TeamFriend/teamfriend.cpp \
    GameInterchange/gameinterchange.cpp \
    OperationGoods/operationgoods.cpp \
    OperationPostgres/operationpostgres.cpp \
    Game/cmdparse.cpp \
#    Monster/monsterstruct.cpp
    GameChat/gamechat.cpp \
    fileOperation/fileoperation.cpp \
    PlayerTrading/playertrading.cpp \
    Game/log.cpp \




include(deployment.pri)
qtcAddDeployment()

HEADERS += \
#    Game/cmdparse.hpp \
    Game/cmdtypes.h \
#    Game/log.hpp \
    Game/packheadflag.h \
    Game/postgresqlstruct.h \
    Game/session.hpp \
#    Game/transfer.hpp \
    Game/userposition.hpp \
    memManage/diskdbmanager.h \
    memManage/idbmanager.h \
    memManage/memdbmanager.h \
    NetWork/netclient.h \
    NetWork/tcpconnection.h \
    NetWork/tcpserver.h \
    utils/stringbuilder.hpp \
    hf_types.h \
    md5.h \
    rpc.h \
    server.h \
    Monster/monster.h \
    Game/getdefinevalue.h \
    Game/levelexperience.h \
    GameAttack/gameattack.h \
    GameTask/gametask.h \
    PlayerLogin/playerlogin.h \
    TeamFriend/teamfriend.h \
    GameInterchange/gameinterchange.h \
    OperationGoods/operationgoods.h \
    OperationPostgres/operationpostgres.h \
    Game/cmdparse.h \
    Monster/monsterstruct.h \
    GameChat/gamechat.h \
    fileOperation/fileoperation.h \
    PlayerTrading/playertrading.h \
    Game/log.h \
#    Game/log.h




INCLUDEPATH += ./LIBS/boost_1_58_0 \
./LIBS/threadpool-0_2_5-src/threadpool/boost

#LIBS += ./LIBS/libhiredis.a \
#./LIBS/libpq.so \
#./LIBS/libboost_system.a \
#./LIBS/libboost_thread.a \
#./LIBS/libboost_libboost_log.a \
#./LIBS/liblog4c.a

#INCLUDEPATH += /home/hf02/soft/boost_1_58_0
#INCLUDEPATH += /home/hf02/soft/threadpool-0_2_5-src/threadpool/boost

LIBS += /home/hf02/soft/hiredis-master/libhiredis.a
LIBS += /usr/lib64/libpq.so
LIBS += /home/hf02/soft/boost_1_58_0/stage/lib/libboost_system.a
LIBS += /home/hf02/soft/boost_1_58_0/stage/lib/libboost_thread.a
LIBS += /home/hf02/soft/boost_1_58_0/stage/lib/libboost_log.a
LIBS += /usr/local/lib/liblog4c.a


#QMAKE_LFLAGS += -pthread

#DISTFILES += \
#    Game/log4crc
