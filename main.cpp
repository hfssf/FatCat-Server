/*
 * main.cpp
 *
 *  Created on: 2015年4月26日
 *      Author: yang
 */

#include "server.h"
#include "Game/log.h"


int main()
{
    //数据库初始化
     Logger::GetLogger()->Info("Init DB ........");
     Server::GetInstance()->InitDB();
     Logger::GetLogger()->Debug("Init DB Finished!");

    //开始监听客户端连接
    Logger::GetLogger()->Info("Init Listen.......");
    Server::GetInstance()->InitListen();

	return 0;
}

