#ifndef SERVER_H
#define SERVER_H

#include <threadpool.hpp>
#include <boost/bind.hpp>
#include <boost/pool/pool.hpp>
#include <boost/lockfree/queue.hpp>

#include "Game/cmdtypes.h"

class Monster;
class JobPool;
class Task;
class MemDBManager;
class DiskDBManager;
class PlayerLogin;
class GameTask;
class TeamFriend;
class GameAttack;
class GameInterchange;
class OperationGoods;
class OperationPostgres;
class CmdParse;
class GameChat;


using namespace boost::threadpool;

class Server
{
public:

    virtual ~Server();


    void* malloc()
    {
        m_mtx.lock();
        void *res = m_memory_factory.malloc();
        m_mtx.unlock();
        return res;
    }
    void    free(void *ptr)
    {
        m_mtx.lock();
        m_memory_factory.free( ptr );
        m_mtx.unlock();
    }

    static void memDelete(void *ptr)
    {
        Server::GetInstance()->free(ptr);
    }

    /**
     * @brief InitDB
     *
     * 初始化数据库连接
     *
     */
    void  InitDB();
    /**
     * @brief InitListen
     * 初始化监听套接字
     */
    void    InitListen();
    /**
     * @brief RunTask
     */
    template <typename T>
    void RunTask( T task)
    {
        m_task_pool.schedule(task);
    }

    template <typename T>
    void RunCmdparse(T task)
    {
        m_cmdparse_pool.schedule(task);
    }

    static Server*          GetInstance();

    DiskDBManager* getDiskDB()
    {
        return m_DiskDB;
    }

    MemDBManager* getMemDB()
    {
        return m_MemDB;
    }

    Monster         *GetMonster()  { return m_monster;}

    PlayerLogin* GetPlayerLogin()
    {
        return m_playerLogin;
    }

    GameTask* GetGameTask()
    {
        return m_gameTask;
    }

    TeamFriend* GetTeamFriend()
    {
        return m_teamFriend;
    }

    GameAttack* GetGameAttack()
    {
        return m_gameAttack;
    }

    GameInterchange* GetGameInterchange()
    {
        return m_gameInterchange;
    }

    OperationGoods* GetOperationGoods()
    {
        return m_operationGoods;
    }
    OperationPostgres* GetOperationPostgres()
    {
        return m_operationPostgres;
    }
//    CmdParse* GetCmdParse()
//    {
//        return m_cmdParse;
//    }

    GameChat* GetGameChat()
    {
        return m_gameChat;
    }

    void PushPackage(STR_Package package)
    {
        m_package->push(package);
        push_num++;
        if(push_num - pop_num > 10000)
            printf("push_num - pop_num:%u,%u,%u\n", push_num,pop_num,push_num - pop_num);
    }

    void PopPackage();

    hf_uint32 push_num;
    hf_uint32 pop_num;
private:
    Server();
    static  Server                 *m_instance;
    MemDBManager                   *m_MemDB;
    DiskDBManager                  *m_DiskDB;
    ////////////////////////////////////////////////////
    boost::threadpool::pool         m_task_pool;
    boost::threadpool::pool         m_cmdparse_pool;
    boost::pool<>                   m_memory_factory;
    boost::mutex                    m_mtx;
    Monster                         *m_monster;
    PlayerLogin                     *m_playerLogin;
    GameTask                        *m_gameTask;
    TeamFriend                      *m_teamFriend;
    GameAttack                      *m_gameAttack;
    GameInterchange                 *m_gameInterchange;
    OperationGoods                  *m_operationGoods;
    OperationPostgres               *m_operationPostgres;
//    CmdParse                        *m_cmdParse;
    GameChat                        *m_gameChat;

    boost::lockfree::queue<STR_Package>            *m_package;

};

#endif // SERVER_H
