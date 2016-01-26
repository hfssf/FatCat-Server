

#include <boost/asio.hpp>
#include <iostream>

#include "postgresqlstruct.h"
#include "cmdtypes.h"
#include "./../server.h"

#include "userposition.hpp"
#include "log.h"
#include "./../NetWork/tcpconnection.h"
#include "./../PlayerLogin/playerlogin.h"
#include "./../GameTask/gametask.h"
#include "./../TeamFriend/teamfriend.h"
#include "./../GameAttack/gameattack.h"
#include "./../GameInterchange/gameinterchange.h"
#include "./../OperationGoods/operationgoods.h"
#include "./../GameChat/gamechat.h"
#include "cmdparse.h"

void CommandParse(TCPConnection::Pointer conn , hf_char *reg)
{
//     tcp::socket *sk =  &(conn->socket());
//    char *buf = (char*)reg;
    STR_PackHead *head = (STR_PackHead*)reg;
    hf_uint16 flag = head->Flag;
    hf_uint16 len = head->Len;
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    Session* sess = &(*smap)[conn];
    hf_uint32 hp = sess->m_roleInfo.HP;
    if(hp == 0) //玩家死亡
    {
        if(!(flag == FLAG_PlayerRevive || flag == FLAG_PlayerPosition))
            return;
    }

    switch( flag )
    {
    case FLAG_PlayerRevive:          //玩家复活   test
    {
        STR_PlayerRelive* t_relive = (STR_PlayerRelive*)(reg + sizeof(STR_PackHead));
        Server::GetInstance()->GetPlayerLogin()->PlayerRelive(conn, t_relive->mode);
        Server::GetInstance()->free(reg);
        break;
    }

    case FLAG_PlayerOffline:  //玩家下线，给其他玩家发送下线通知
    {
        STR_PackPlayerOffline *pos = (STR_PackPlayerOffline*)reg;
        Server::GetInstance()->GetPlayerLogin()->PlayerOffline(conn, pos);
        break;
    }
    case FLAG_PlayerPosition:   //玩家位置变动(test)
    {
        UserPosition::PlayerMove(conn, (STR_PackPlayerPosition*)reg);
        break;
    }

    case FLAG_PlayerDirect:
    {
        hf_float direct = 0;
        memcpy(&direct, reg + sizeof(STR_PackHead), 4);
//        printf("%f\n", direct);
        UserPosition::PlayerDirectChange(conn, direct);
        Server::GetInstance()->free(reg);
        break;
    }

    case FLAG_PlayerAction:
    {
        hf_uint8 action = 0;
        memcpy(&action, (reg + sizeof(STR_PackHead)), 1);
        UserPosition::PlayerActionChange(conn, action);
        Server::GetInstance()->free(reg);
        break;
    }
    case FLAG_PlayerMove:    //玩家移动
    {
//        Logger::GetLogger()->Debug("Player move");
//        STR_PlayerMove* t_move = (STR_PlayerMove*)Server::GetInstance()->malloc();
//        memcpy(t_move, buf + sizeof(STR_PackHead), len);
//        UserPosition::PlayerPositionMove(conn, t_move);
//        Server::GetInstance()->RunTask(boost::bind(UserPosition::PlayerPositionMove, conn, t_move));
        break;
    }
    case FLAG_UserAskTask: //玩家请求接受任务数据
    {
        hf_uint32 taskID = ((STR_PackUserAskTask*)reg)->TaskID;
        Server::GetInstance()->GetGameTask()->AskTask(conn, taskID);
        Server::GetInstance()->free(reg);
        break;
    }
    case FLAG_QuitTask:   //玩家请求放弃任务
    {
        hf_uint32 taskID = ((STR_PackQuitTask*)reg)->TaskID;
        Server::GetInstance()->GetGameTask()->QuitTask(conn, taskID);
        Server::GetInstance()->free(reg);
        break;
    }
    case FLAG_AskFinishTask:  //玩家请求完成任务
    {
        STR_PackFinishTask* finishTask = (STR_PackFinishTask*)reg;
        Server::GetInstance()->GetGameTask()->AskFinishTask(conn, finishTask);

        break;
    }
    case FLAG_TaskExeDlg:
    {
        STR_PackAskTaskExeDlg* askTaskExeDlg = (STR_PackAskTaskExeDlg*)reg;
        Server::GetInstance()->GetGameTask()->AskTaskExeDialog(conn, askTaskExeDlg);

        break;
    }
    case FLAG_TaskExeDlgFinish:
    {
        STR_PackAskTaskExeDlg* askTaskExeDlg = (STR_PackAskTaskExeDlg*)reg;
        Server::GetInstance()->GetGameTask()->TaskExeDialogFinish(conn, askTaskExeDlg);
        break;
    }
    case FLAG_AskTaskData:    //请求任务数据包
    {               
        AskTaskData* t_ask = (AskTaskData*)(reg + sizeof(STR_PackHead));
        GameTask* t_task = Server::GetInstance()->GetGameTask();
        switch(t_ask->Flag)
        {
        case FLAG_TaskStartDlg:
        {
            t_task->StartTaskDlg(conn, t_ask->TaskID);
            break;
        }
        case FLAG_TaskFinishDlg:
        {
            t_task->FinishTaskDlg(conn, t_ask->TaskID);;
            break;
        }
        case FLAG_TaskDescription:
        {
            t_task->TaskDescription(conn, t_ask->TaskID);
            break;
        }
        case FLAG_TaskAim:
        {
            t_task->TaskAim(conn, t_ask->TaskID);
            break;
        }
        case FLAG_TaskReward:
        {
            t_task->TaskReward(conn, t_ask->TaskID);
            break;
        }
        default:
            break;
        }
        Server::GetInstance()->free(reg);
        break;
    }
    case FLAG_AddFriend:     //添加好友
    {
        STR_PackAddFriend* t_add = (STR_PackAddFriend*)reg;
        Server::GetInstance()->GetTeamFriend()->addFriend(conn, t_add);
        break;

    }
    case FLAG_DeleteFriend:   //删除好友
    {     
        hf_uint32 friendID = ((STR_DeleteFriend*)(reg + sizeof(STR_PackHead)))->RoleID;
        Server::GetInstance()->GetTeamFriend()->deleteFriend(conn, friendID);
        Server::GetInstance()->free(reg);
        break;
    }
    case FLAG_AddFriendReturn:  //添加好友客户端返回
    {
        STR_PackAddFriendReturn* t_AddFriend =(STR_PackAddFriendReturn*)reg;
        Server::GetInstance()->GetTeamFriend()->ReciveAddFriend(conn, t_AddFriend);
        break;
    }
    case FLAG_UserAttackAim:  //攻击目标
    {
        STR_PackUserAttackAim* t_attack =(STR_PackUserAttackAim*)reg;
        Server::GetInstance()->GetGameAttack()->AttackAim(conn, t_attack);
        break;
    }
    case FLAG_UserAttackPoint:  //攻击点
    {
        STR_PackUserAttackPoint* t_attack =(STR_PackUserAttackPoint*)reg;
        Server::GetInstance()->GetGameAttack()->AttackPoint(conn, t_attack);
        break;
    }
    case FLAG_PickGoods:      //捡物品
    {
        STR_PackPickGoods* t_pick =(STR_PackPickGoods*)reg;
        Server::GetInstance()->GetOperationGoods()->PickUpGoods(conn, t_pick);
        break;
    }
    case FLAG_RemoveGoods:  //丢弃物品
    {
        STR_PackRemoveBagGoods* t_remove = (STR_PackRemoveBagGoods*)reg;
        Server::GetInstance()->GetOperationGoods()->RemoveBagGoods(conn, t_remove);
        break;
    }
    case FLAG_MoveGoods:  //移动或分割物品
    {
        STR_PackMoveBagGoods* t_move = (STR_PackMoveBagGoods*)reg;
        Server::GetInstance()->GetOperationGoods()->MoveBagGoods(conn, t_move);
        break;
    }
    case FLAG_BuyGoods:    //购买物品
    {
        STR_PackBuyGoods* t_buy = (STR_PackBuyGoods*)reg;
        Server::GetInstance()->GetOperationGoods()->BuyGoods(conn, t_buy);
        break;
    }
    case FLAG_SellGoods:   //出售物品
    {
        STR_PackSellGoods* t_sell = (STR_PackSellGoods*)reg;
        Server::GetInstance()->GetOperationGoods()->SellGoods(conn, t_sell);
        break;
    }
    case FLAG_ArrangeBagGoods:   //整理背包物品
    {
        Server::GetInstance()->GetOperationGoods()->ArrangeBagGoods(conn);
        break;
    }

    case FLAG_WearBodyEqu: //穿装备
    {
        STR_WearEqu* t_wearEqu = (STR_WearEqu*)(reg + sizeof(STR_PackHead));
        Server::GetInstance()->GetOperationGoods()->WearBodyEqu(conn, t_wearEqu->equid, t_wearEqu->pos);
        break;
    }

    case FLAG_TakeOffEqu:  //脱装备
    {
        hf_uint32 equid = 0;
        memcpy(&equid, reg + sizeof(STR_PackHead), sizeof(equid));
        Server::GetInstance()->GetOperationGoods()->TakeOffBodyEqu(conn, equid);
        break;
    }

    case FLAG_UseBagGoods:
    {
        STR_UseBagGoods* bagGoods = (STR_UseBagGoods*)(reg + sizeof(STR_PackHead));
        Server::GetInstance()->GetOperationGoods()->UseBagGoods(conn, bagGoods->goodsid, bagGoods->pos);
        break;
    }

    case FLAG_Chat:   //聊天 test
    {
        STR_PackRecvChat* t_chat = (STR_PackRecvChat*)reg;
        Server::GetInstance()->GetGameChat()->Chat(conn, t_chat);
        break;
    }
    case 1000:
    {
        conn->Write_all(reg, sizeof(STR_PackHead) + len);
        break;
    }
//    case FLAG_TradeOper:
//    {
//        STR_PackRequestOper* t_oper = (STR_PackRequestOper*)Server::GetInstance()->malloc();
//        memcpy(t_oper, buf, len + sizeof(STR_PackHead));

//        break;
//    }
//    case FLAG_TradeOperResult:
//    {

//        break;
//    }
//    case FLAG_TradeOperGoods:
//    {

//        break;
//    }
//    case FLAG_TradeOperMoney:
//    {

//        break;
//    }
    case FLAG_OperRequest:
    {
        operationRequest* t_operReq = (operationRequest*)reg;
        switch(t_operReq->operType)
        {
            case FLAG_WantToChange:
            {
                Server::GetInstance()->GetGameInterchange()->operRequest(conn, t_operReq);
                break;
            }
            default:
                break;
        }
        break;
    }

    case FLAG_OperResult:
    {
        operationRequestResult* t_operReq = (operationRequestResult*)reg;
        switch(t_operReq->operType)
        {
            case FLAG_WantToChange:
            {
                Server::GetInstance()->GetGameInterchange()->operResponse(conn, t_operReq);
                break;
            }
            default:
                break;
        }
        break;
    }
    case FLAG_InterchangeMoneyCount:
    {
        interchangeMoney* t_Money = (interchangeMoney*)reg;
        Server::GetInstance()->GetGameInterchange()->operMoneyChanges(conn, t_Money);
        break;
    }
    case FLAG_InterchangeGoods:
    {
        interchangeOperGoods* t_OperPro = (interchangeOperGoods*)reg;
        Server::GetInstance()->GetGameInterchange()->operChanges(conn, t_OperPro);
        break;
    }
    case  FLAG_InterchangeOperPro:
    {
        interchangeOperPro* t_OperPro = (interchangeOperPro*)reg;
        GameInterchange* t_gameInterchange = Server::GetInstance()->GetGameInterchange();
        switch(t_OperPro->operType)
        {
            case Interchange_Change:  //交易
            {
                t_gameInterchange->operProCheckChange(conn, t_OperPro);
                break;
            }
            case Interchange_Lock:  //锁定
            {
                t_gameInterchange->operProLock(conn, t_OperPro);
                break;
            }
            case Interchange_Unlock:  //取消锁定
            {
                t_gameInterchange->operProUnlock(conn, t_OperPro);
                break;
            }
            case Interchange_CancelChange:  //取消交易
            {
                t_gameInterchange->operProCancelChange(conn, t_OperPro);
                break;
            }
            default:
                break;
        }
        break;
    }

    default:
    {
        Logger::GetLogger()->Info("Unkown pack");
        Logger::GetLogger()->Error("flag:%d, len:%d\n", head->Flag, head->Len);
        break;
    }
    }
    Server::GetInstance()->CmdparseAdd();
}

void CommandParseLogin(TCPConnection::Pointer conn, hf_char* reg)
{
    STR_PackHead *head = (STR_PackHead*)reg;
    hf_uint16 flag = head->Flag;
    hf_uint16 len = head->Len;

    PlayerLogin* t_playerLogin = Server::GetInstance()->GetPlayerLogin();
    switch( flag )
    {
    //注册玩家帐号
    case FLAG_PlayerRegisterUserId:
    {
        STR_PackRegisterUserId* t_reg = (STR_PackRegisterUserId*)Server::GetInstance()->malloc();

        memcpy(t_reg, reg, len + sizeof(STR_PackHead));
        t_playerLogin->RegisterUserID(conn, t_reg);
        break;
    }
        //注册玩家角色
    case FLAG_PlayerRegisterRole:
    {
        STR_PackRegisterRole* t_reg =(STR_PackRegisterRole*)Server::GetInstance()->malloc();
        memcpy(t_reg, reg, len + sizeof(STR_PackHead));
        t_playerLogin->RegisterRole(conn, t_reg);
        break;
    }
        //删除角色
    case FLAG_UserDeleteRole:
    {
        STR_PlayerRole* t_reg = (STR_PlayerRole*)(reg + sizeof(STR_PackHead));
        t_playerLogin->DeleteRole(conn, t_reg->Role);
        break;
    }
        //登陆帐号
    case FLAG_PlayerLoginUserId:
    {
        STR_PackLoginUserId* t_login = (STR_PackLoginUserId*)Server::GetInstance()->malloc();
        memcpy(t_login, reg, len + sizeof(STR_PackHead));
        t_playerLogin->LoginUserId(conn, t_login);
        break;
    }
        //登陆角色
    case FLAG_PlayerLoginRole:
    {
        STR_PlayerRole* t_login = (STR_PlayerRole*)(reg + sizeof(STR_PackHead));
        t_playerLogin->LoginRole(conn, t_login->Role);
        break;
    }
    default:
    {
        Logger::GetLogger()->Info("Unkown pack");
        Logger::GetLogger()->Error("flag:%d, len:%d\n", head->Flag, head->Len);
        break;
    }
    }
}
