#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

#include "memdbmanager.h"
#include "diskdbmanager.h"
#include "./../server.h"

MemDBManager::MemDBManager()
{

}

MemDBManager::~MemDBManager()
{

}

bool MemDBManager::Connect(Configuration con)
{
    m_redis = redisConnect(con.ip, atoi(con.port));
    if(m_redis == NULL || m_redis->err)
    {
        printf("Error: %s\n", m_redis->errstr);
        redisFree(m_redis);
        return false;
    }
    else
        return true;
}

bool MemDBManager::Connect()
{
    m_redis = redisConnect(REDISIP, REDISPORT);
    if(m_redis == NULL || m_redis->err)
    {
        printf("Error: %s\n", m_redis->errstr);
        redisFree(m_redis);
        return false;
    }
    else
        return true;
}

bool MemDBManager::Disconnect()
{
    redisFree(m_redis);
    return true;
}


//可变参
void* MemDBManager::SetC(const char *format,...)
{
    va_list ap;
    void *t_reply = NULL;
    va_start(ap,format);
    t_reply = redisvCommand(m_redis,format,ap);
    va_end(ap);
    return t_reply;
}

void MemDBManager::SetB(const char *key, void *value, int len)
{
//    char buf[256] = {0};
//    return Set("LPUSH %s %b",key,value,len);
    printf(" Set %s %s\n", key, value);

    redisReply* t_reply = (redisReply*)SetC("SET %s %b",key,value,len);
    if(!t_reply)
    {
        redisFree(m_redis);
        printf("redisCommand set %s error\n", key);
        Connect();
        SetC("SET %s %b",key,value,len);
    }
    if(!(t_reply->type == REDIS_REPLY_STATUS && strcasecmp(t_reply->str, "OK") == 0))
    {
       printf("redisCommand %s error\n",key);
       freeReplyObject(t_reply);
    }
}

void MemDBManager::DelKey(const char* str)
{
    redisReply* t_reply = (redisReply*)redisCommand(m_redis, str);
    if(!t_reply)
    {
       redisFree(m_redis);
       printf("redisCommand %s error\n",str);
       Connect();
       DelKey(str);
    }
    else
    {
        freeReplyObject(t_reply);
    }
}


bool MemDBManager::SetA(const char* setStr)
{
    redisReply* t_reply = (redisReply*)redisCommand(m_redis, setStr);
    if(!t_reply)
    {
        redisFree(m_redis);
        Connect();
        SetA(setStr);
    }
    if(!(t_reply->type == REDIS_REPLY_STATUS && strcasecmp(t_reply->str, "OK") == 0))
    {
       printf("redisCommand %s error\n",setStr);
       freeReplyObject(t_reply);
       return false;
    }
    else
    {
        freeReplyObject(t_reply);
        return true;
    }
}


void* MemDBManager::Get(const char* getStr)
{
    redisReply* t_reply = (redisReply*)redisCommand(m_redis, getStr);
    if(t_reply == NULL || t_reply->type == REDIS_REPLY_ERROR)
    {
        printf("redisCommand %s error\n",getStr);
        freeReplyObject(t_reply);
        redisFree(m_redis);
        return NULL;
    }
    else if(t_reply->type == REDIS_REPLY_NIL)
    {
        printf("key value does not exist\n");
        freeReplyObject(t_reply);
        return NULL;
    }
    else if(t_reply->type == REDIS_REPLY_STRING) //字符串
    {
        int t_length = t_reply->len;
        hf_char* t_res = (hf_char*)Server::GetInstance()->malloc();
        if(t_res == NULL)
        {
            freeReplyObject(t_reply);
            return NULL;
        }
        else
        {
            memcpy(t_res, t_reply->str, t_length);
            freeReplyObject(t_reply);
            return t_res;
        }
    }
    else if(t_reply->type == REDIS_REPLY_INTEGER) //整数
    {

        long long num = t_reply->integer;
        hf_char* t_res = (hf_char*)Server::GetInstance()->malloc();
        if(t_res == NULL)
        {
            freeReplyObject(t_reply);
            return NULL;
        }
        else
        {
            memcpy(t_res, &num, sizeof(num));
            freeReplyObject(t_reply);
            return t_res;
        }
    }
//    else if(t_reply->type == REDIS_REPLY_ARRAY) //数组
//    {
//        for(int i = 0; i < t_reply->elements; i++)
//            redisReply* childReply = r_element[i];


//    }
}

//bool MemDBManager::GetDbData(Configuration DiskCon, Configuration MemCon)
//{
//    DiskDBManager t_disk;
//    if(!t_disk.Connect(DiskCon))
//        return false;
//    if(!Connect(MemCon))
//        return false;
////    People   p;
////    p.age = 10;
////    strcpy(p.name,"Yang");
//    const char* str = (const char*)t_disk.Get("select * from test;");
//    Result t_result;
//    memset(&t_result, 0, sizeof(t_result));
//    memcpy(&t_result, str, sizeof(t_result));
//    char* t_colname = (char*)malloc(20);
//    char* t_colvalue = (char*)malloc(20);
//    char* t_str = (char*)malloc(50);

//    char cmd[128] = {0};
//    char buf[256] = {0};
//    string   s;
////    memcpy(buf,&p,sizeof(People));

////    Set("SET yang %b",(void*)&p,sizeof(p));
////    SetB("yang2",&p,sizeof(p));
////    p.age = 11;
////    SetB("yang2",&p,sizeof(p));
//    //Get
////    People *ptr = (People*)Get("RPOP yang2");
////    ptr = (People*)Get("RPOP yang2");
////    ptr = (People*)Get("RPOP yang2");

//    if(t_result.result == true)
//    {
//        str += sizeof(t_result);
//        printf("%s\n", str);
//        int num = strlen(str);
//        while(num > 0)
//        {
//            memcpy(t_colname, str, 2);
//            str += 2;
//            memcpy(t_colvalue, str, 1);
//            str += 1;
//            sprintf(t_str, "SET %s %s", t_colname, t_colvalue);

//            memset(&t_result, 0, sizeof(t_result));
//            memcpy(&t_result, Set(t_str), sizeof(t_result));
//            if(t_result.result == false)
//                break;

//            memcpy(t_colname, str, 3);
//            str += 3;
//            memcpy(t_colvalue, str, 2);
//            str += 2;
//            sprintf(t_str, "SET %s %s", t_colname, t_colvalue);

//            memset(&t_result, 0, sizeof(t_result));
//            memcpy(&t_result, Set(t_str), sizeof(t_result));
//            if(t_result.result == false)
//                break;
//            num -= 8;
//        }
//        free(t_colname);
//        free(t_colvalue);
//        free(t_str);
//        return t_result.result;
//    }
//    else
//    {
//        free(t_colname);
//        free(t_colvalue);
//        free(t_str);
//        return t_result.result;
//    }
//}
