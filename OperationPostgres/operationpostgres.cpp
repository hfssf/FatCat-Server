#include "operationpostgres.h"
#include "./../PlayerLogin/playerlogin.h"
#include "./../server.h"
#include "./../memManage/diskdbmanager.h"
#include "./../memManage/memdbmanager.h"

#include <cstdio>

OperationPostgres::OperationPostgres():
    m_UpdateMoney(new boost::lockfree::queue<UpdateMoney>(100)),
    m_UpdateLevel(new boost::lockfree::queue<UpdateLevel>(100)),
    m_UpdateExp(new boost::lockfree::queue<UpdateExp>(100)),
    m_UpdateGoods(new boost::lockfree::queue<UpdateGoods>(100)),
    m_UpdateEquAttr(new boost::lockfree::queue<UpdateEquAttr>(100)),
    m_UpdateTask(new boost::lockfree::queue<UpdateTask>(100)),
    m_UpdateCompleteTask(new boost::lockfree::queue<UpdateCompleteTask>(100))
{

}

OperationPostgres::~OperationPostgres()
{
    delete m_UpdateMoney;
    delete m_UpdateLevel;
    delete m_UpdateExp;
    delete m_UpdateGoods;
    delete m_UpdateEquAttr;
    delete m_UpdateTask;
    delete m_UpdateCompleteTask;
}

//该函数负责实时将玩家数据写入数据库
void OperationPostgres::UpdateRedisData()
{
    UpdateMoney t_updateMoney;
    UpdateLevel t_updateLevel;
    UpdateExp t_updateExp;
    UpdateGoods t_updateGoods;
    UpdateEquAttr t_updateEquAttr;
    UpdateTask t_updateTask;
    UpdateCompleteTask t_comTask;
    MemDBManager* t_redis = Server::GetInstance()->getMemDB();
    hf_char buff[64] = { 0 };
    while(1)
    {
        if(m_UpdateMoney->pop(t_updateMoney))
        {
            t_redis->SetC("set %u%u %u",t_updateMoney.RoleID, t_updateMoney.Money.TypeID,t_updateMoney.Money.Count);


//            memset(buff, 0 ,sizeof(buff));
//            sprintf(buff,"get %d%d",t_updateMoney.RoleID, t_updateMoney.Money.TypeID);
//            hf_char* money = (hf_char*)t_redis->Get(buff);
//            hf_uint32 monenCount = atoi(money);
//            free(money);
        }

        if(m_UpdateLevel->pop(t_updateLevel))
        {
            t_redis->SetC("set %u %u",t_updateLevel.RoleID, t_updateLevel.Level);
        }

        if(m_UpdateExp->pop(t_updateExp))
        {
            t_redis->SetC("set %u0 %u",t_updateExp.RoleID, t_updateExp.Exp);
        }


        if(m_UpdateGoods->pop(t_updateGoods))
        {
            memset(buff, 0 ,sizeof(buff));
            if(t_updateGoods.Operate == PostDelete)
            {
               sprintf(buff,"del %u%u",t_updateGoods.RoleID,t_updateGoods.Goods.Position);
                t_redis->DelKey(buff);
            }
            else
            {
                sprintf(buff,"%u%u",t_updateGoods.RoleID,t_updateGoods.Goods.Position);
                t_redis->SetB(buff,&t_updateGoods.Goods, sizeof(t_updateGoods.Goods));

//                memset(buff, 0 ,sizeof(buff));
//                sprintf(buff,"get %u%u",t_updateGoods.RoleID,t_updateGoods.Goods.Position);
//                STR_Goods* goods = (STR_Goods*)t_redis->Get(buff);
//                free(goods);
            }
        }

        if(m_UpdateEquAttr->pop(t_updateEquAttr))
        {
            if(t_updateEquAttr.Operate == PostDelete)
            {
                memset(buff, 0 ,sizeof(buff));
                sprintf(buff,"del %u%u",t_updateEquAttr.RoleID,t_updateEquAttr.EquAttr.EquID);
                 t_redis->DelKey(buff);
            }
            else
            {
                memset(buff, 0 ,sizeof(buff));
                sprintf(buff,"%u%u",t_updateEquAttr.RoleID,t_updateEquAttr.EquAttr.EquID);
                t_redis->SetB(buff,&t_updateEquAttr.EquAttr, sizeof(t_updateEquAttr.EquAttr));
            }
        }

        if(m_UpdateTask->pop(t_updateTask))
        {
            if(t_updateTask.Operate == PostDelete)
            {
                memset(buff, 0 ,sizeof(buff));
                sprintf(buff,"del %u%u",t_updateTask.RoleID,t_updateTask.TaskProcess.TaskID);
                 t_redis->DelKey(buff);
            }
            else
            {
                memset(buff, 0 ,sizeof(buff));
                sprintf(buff,"%u%u",t_updateTask.RoleID,t_updateTask.TaskProcess.TaskID);
                t_redis->SetB(buff,&t_updateTask.TaskProcess, sizeof(t_updateTask.TaskProcess));
            }
        }

        if(m_UpdateCompleteTask->pop(t_comTask))
        {
            memset(buff, 0 ,sizeof(buff));
            sprintf(buff,"del %u%u",t_comTask.RoleID,t_comTask.TaskID);
             t_redis->DelKey(buff);
        }

        usleep(1000);
    }
}

//该函数负责实时将玩家数据写入数据库
void OperationPostgres::UpdatePostgresData()
{
    UpdateMoney t_updateMoney;
    UpdateLevel t_updateLevel;
    UpdateExp t_updateExp;
    UpdateGoods t_updateGoods;
    UpdateEquAttr t_updateEquAttr;
    UpdateTask t_updateTask;
    UpdateCompleteTask t_comTask;
    hf_uint32 connectTime = 0;
    while(1)
    {
        if(m_UpdateMoney->pop(t_updateMoney))
            PlayerLogin::UpdatePlayerMoney(&t_updateMoney);

        if(m_UpdateLevel->pop(t_updateLevel))
            PlayerLogin::UpdatePlayerLevel(&t_updateLevel);

        if(m_UpdateExp->pop(t_updateExp))
            PlayerLogin::UpdatePlayerExp(&t_updateExp);


        if(m_UpdateGoods->pop(t_updateGoods))
        {
            if(t_updateGoods.Operate == PostUpdate)
                PlayerLogin::UpdatePlayerGoods(t_updateGoods.RoleID, &t_updateGoods.Goods);
            else if(t_updateGoods.Operate == PostInsert)
                PlayerLogin::InsertPlayerGoods(t_updateGoods.RoleID, &t_updateGoods.Goods);
            else if(t_updateGoods.Operate == PostDelete)
                PlayerLogin::DeletePlayerGoods(t_updateGoods.RoleID, t_updateGoods.Goods.Position);
        }

        if(m_UpdateEquAttr->pop(t_updateEquAttr))
        {
            if(t_updateEquAttr.Operate == PostUpdate)
                PlayerLogin::UpdatePlayerEquAttr(&t_updateEquAttr.EquAttr);
            else if(t_updateEquAttr.Operate == PostInsert)
                PlayerLogin::InsertPlayerEquAttr(t_updateEquAttr.RoleID, &t_updateEquAttr.EquAttr);
            else if(t_updateEquAttr.Operate == PostDelete)
                PlayerLogin::DeletePlayerEquAttr(t_updateEquAttr.EquAttr.EquID);
        }

        if(m_UpdateTask->pop(t_updateTask))
        {
            if(t_updateTask.Operate == PostUpdate)
                PlayerLogin::UpdatePlayerTask(t_updateTask.RoleID, &t_updateTask.TaskProcess);
            else if(t_updateTask.Operate == PostInsert)
                PlayerLogin::InsertPlayerTask(t_updateTask.RoleID, &t_updateTask.TaskProcess);
            else if(t_updateTask.Operate == PostDelete)
                PlayerLogin::DeletePlayerTask(t_updateTask.RoleID, t_updateTask.TaskProcess.TaskID);
        }

        if(m_UpdateCompleteTask->pop(t_comTask))
            PlayerLogin::InsertPlayerCompleteTask(t_comTask.RoleID, t_comTask.TaskID);


        usleep(1000);
//        if(connectTime++ == 300000)
//        {
//            if(Server::GetInstance()->getDiskDB()->GetSqlResult("select * from t_skillinfo where skillid = 0") == -1)
//            {
//                printf("postgres break off\n");
//            }
//            connectTime = 0;
//        }
    }
}

void OperationPostgres::PopUpdateMoney()
{
    UpdateMoney t_updateMoney;
    while(1)
    {
        if(m_UpdateMoney->pop(t_updateMoney))
        {
            PlayerLogin::UpdatePlayerMoney(&t_updateMoney);
        }
        else
        {
           usleep(1000);
        }
    }
}

void OperationPostgres::PopUpdateLevel()
{
    UpdateLevel t_updateLevel;
    while(1)
    {
        if(m_UpdateLevel->pop(t_updateLevel))
        {
            PlayerLogin::UpdatePlayerLevel(&t_updateLevel);
        }
        else
        {
            usleep(1000);
        }
    }
}

void OperationPostgres::PopUpdateExp()
{
    UpdateExp t_updateExp;
    while(1)
    {
        if(m_UpdateExp->pop(t_updateExp))
        {
            PlayerLogin::UpdatePlayerExp(&t_updateExp);
        }
        else
        {
            usleep(1000);
        }
    }
}


void OperationPostgres::PopUpdateGoods()
{
    UpdateGoods t_updateGoods;
    while(1)
    {
        if(m_UpdateGoods->pop(t_updateGoods))
        {
            cout << t_updateGoods.Goods.GoodsID << endl;
            if(t_updateGoods.Operate == PostUpdate)
            {
                PlayerLogin::UpdatePlayerGoods(t_updateGoods.RoleID, &t_updateGoods.Goods);
            }
            else if(t_updateGoods.Operate == PostInsert)
            {
                PlayerLogin::InsertPlayerGoods(t_updateGoods.RoleID, &t_updateGoods.Goods);
            }
            else if(t_updateGoods.Operate == PostDelete)
            {
                PlayerLogin::DeletePlayerGoods(t_updateGoods.RoleID, t_updateGoods.Goods.Position);
            }
        }
        else
        {
            usleep(1000);
        }
    }
}

void OperationPostgres::PopUpdateEquAttr()
{
    UpdateEquAttr t_updateEquAttr;
    while(1)
    {
        if(m_UpdateEquAttr->pop(t_updateEquAttr))
        {
            if(t_updateEquAttr.Operate == PostUpdate)
            {
                PlayerLogin::UpdatePlayerEquAttr(&t_updateEquAttr.EquAttr);
            }
            else if(t_updateEquAttr.Operate == PostInsert)
            {
                PlayerLogin::InsertPlayerEquAttr(t_updateEquAttr.RoleID, &t_updateEquAttr.EquAttr);
            }
            else if(t_updateEquAttr.Operate == PostDelete)
            {
                PlayerLogin::DeletePlayerEquAttr(t_updateEquAttr.EquAttr.EquID);
            }
        }
        else
        {
            usleep(1000);
        }
    }
}

void OperationPostgres::PopUpdateTask()
{
    UpdateTask t_updateTask;
    while(1)
    {
        if(m_UpdateTask->pop(t_updateTask))
        {
            if(t_updateTask.Operate == PostUpdate)
            {
                PlayerLogin::UpdatePlayerTask(t_updateTask.RoleID, &t_updateTask.TaskProcess);
            }
            else if(t_updateTask.Operate == PostInsert)
            {
                PlayerLogin::InsertPlayerTask(t_updateTask.RoleID, &t_updateTask.TaskProcess);
            }
            else if(t_updateTask.Operate == PostDelete)
            {
                PlayerLogin::DeletePlayerTask(t_updateTask.RoleID, t_updateTask.TaskProcess.TaskID);
            }
        }
        else
        {
            usleep(1000);
        }
    }
}


void OperationPostgres::PopUpdateCompleteTask()
{
    UpdateCompleteTask t_comTask;
    while(1)
    {
        if(m_UpdateCompleteTask->pop(t_comTask))
            PlayerLogin::InsertPlayerCompleteTask(t_comTask.RoleID, t_comTask.TaskID);
        else
            usleep(1000);
    }
}
