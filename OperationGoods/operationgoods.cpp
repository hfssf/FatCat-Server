#include "./../memManage/diskdbmanager.h"
#include "./../GameTask/gametask.h"
#include "./../utils/stringbuilder.hpp"
#include "./../Game/session.hpp"
#include "./../PlayerLogin/playerlogin.h"
#include "./../GameAttack/gameattack.h"
#include "./../OperationPostgres/operationpostgres.h"
#include "./../server.h"

#include "operationgoods.h"


hf_uint32 OperationGoods::m_equipmentID = EquipMentID;
hf_uint32 OperationGoods::m_lootGoodsID = 1;
OperationGoods::OperationGoods()
    :m_goodsPrice(new umap_goodsPrice)
    ,m_consumableAttr(new umap_consumable)
    ,m_equAttr(new umap_equAttr)
{

}

OperationGoods::~OperationGoods()
{
    delete m_goodsPrice;
    delete m_consumableAttr;
    delete m_equAttr;
}

//得到装备编号
hf_uint32  OperationGoods::GetEquipmentID()
{
    return ++m_equipmentID;
}

hf_uint32  OperationGoods::GetLootGoodsID()
{
    if(m_lootGoodsID > 100000000)
    {
        m_lootGoodsID = 1;
        return m_lootGoodsID;
    }
    else
        return ++m_lootGoodsID;
}

//使用位置
void OperationGoods::UsePos(TCPConnection::Pointer conn, hf_uint16 pos)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    (*smap)[conn].m_goodsPosition[pos] = POS_NONEMPTY;
}

//释放位置
void OperationGoods::ReleasePos(TCPConnection::Pointer conn, hf_uint16 pos)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    (*smap)[conn].m_goodsPosition[pos] = POS_EMPTY;
}

//得到空位置总数
hf_uint8 OperationGoods::GetEmptyPosCount(TCPConnection::Pointer conn)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    hf_uint16 emptyPos = 0;
    for(hf_int32 j = 1; j <= BAGCAPACITY; j++)   //找空位置
    {
        if((*smap)[conn].m_goodsPosition[j] == POS_EMPTY)
        {
            emptyPos++;
        }
    }
    return emptyPos;
}

//查找空位置
hf_uint8 OperationGoods::GetEmptyPos(TCPConnection::Pointer conn)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    for(hf_int16 j = 1; j <= BAGCAPACITY;)   //找空位置
    {
        if((*smap)[conn].m_goodsPosition[j] == POS_EMPTY)
        {
            (*smap)[conn].m_goodsPosition[j] = POS_NONEMPTY;
            return j;
        }
        j++;
        if(j == BAGCAPACITY)
        {
            return 0;
        }
    }
}


//判断能否放下
hf_uint8 OperationGoods::JudgeEmptyPos(TCPConnection::Pointer conn, hf_uint8 count)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    for(hf_int32 j = 1; j <= BAGCAPACITY; j++)   //找空位置
    {
        if((*smap)[conn].m_goodsPosition[j] == POS_EMPTY)
        {
            --count;
        }
        if(count == 0)
            return count;
    }
    return count;
}

void OperationGoods::PickUpMoney(TCPConnection::Pointer conn, STR_LootGoods* lootGoods)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    OperationPostgres* t_post = Server::GetInstance()->GetOperationPostgres();

    umap_roleMoney playerMoney = (*smap)[conn].m_playerMoney;
    STR_PlayerMoney* money = &(*playerMoney)[Money_1];

    money->Count += lootGoods->Count;

    hf_uint32 roleid = (*smap)[conn].m_roleid;
    t_post->PushUpdateMoney(roleid, money); //将金钱变动插入到list

    STR_PackPlayerMoney t_money(money);
    conn->Write_all(&t_money, sizeof(STR_PackPlayerMoney));

    STR_PackPickGoodsResult t_pickResult(lootGoods->UniqueID, lootGoods->Count, PICK_SUCCESS);
    conn->Write_all(&t_pickResult, sizeof(STR_PackPickGoodsResult));
}

hf_uint8 OperationGoods::PickUpEqu(TCPConnection::Pointer conn, STR_LootGoods* lootGoods)
{
    hf_uint8 empty_pos = OperationGoods::GetEmptyPos(conn);
    if(empty_pos == 0) //没有空位置
    {
        STR_PackPickGoodsResult t_pickResult(lootGoods->UniqueID, 0, PICK_BAGFULL);
        conn->Write_all(&t_pickResult, sizeof(STR_PackPickGoodsResult));
        return 0;
    }

    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    OperationPostgres* t_post = Server::GetInstance()->GetOperationPostgres();
    hf_uint32 roleid = (*smap)[conn].m_roleid;
    umap_roleEqu playerEqu = (*smap)[conn].m_playerEqu;

    STR_PlayerEqu t_equ;
    t_equ.goods.TypeID = lootGoods->TypeID;
    t_equ.goods.Position = empty_pos;
    t_equ.goods.Count = 1;
    t_equ.goods.Source = Source_Pick;

    umap_lootEquAttr t_lootEquAttr = SessionMgr::Instance()->GetLootEquAttr();
    _umap_lootEquAttr::iterator equAttr_it = t_lootEquAttr->find(lootGoods->GoodsID);
    if(equAttr_it != t_lootEquAttr->end())
    {
        memcpy(&t_equ.equAttr, &equAttr_it->second, sizeof(STR_EquipmentAttr));
        t_equ.goods.GoodsID = t_equ.equAttr.EquID;
    }
    else
    {
        t_equ.goods.GoodsID = OperationGoods::GetEquipmentID();
        SetEquAttr(&t_equ.equAttr, lootGoods->TypeID);   //给新捡装备属性附初值
        t_equ.equAttr.EquID = t_equ.goods.GoodsID;
    }


   (*playerEqu)[t_equ.goods.GoodsID] = t_equ;

    t_post->PushUpdateGoods(roleid, &t_equ.goods, PostInsert); //将新买的物品添加到list
    t_post->PushUpdateEquAttr(roleid, &t_equ.equAttr, PostInsert); //将新买的物品添加到list
    STR_PackGoods t_goods(&t_equ.goods);
    conn->Write_all (&t_goods, sizeof(STR_PackGoods));

    STR_PackEquipmentAttr t_equAttr(&t_equ.equAttr);
    conn->Write_all (&t_equAttr, sizeof(STR_PackEquipmentAttr));

    STR_PackPickGoodsResult t_pickResult(lootGoods->UniqueID, 1, PICK_SUCCESS);
    conn->Write_all(&t_pickResult, sizeof(STR_PackPickGoodsResult));

    Server::GetInstance()->GetGameTask()->UpdateCollectGoodsTaskProcess(conn, lootGoods->TypeID);
    return 1;
}

hf_uint8 OperationGoods::PickUpcommonGoods(TCPConnection::Pointer conn, STR_LootGoods* lootGoods)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    OperationPostgres* t_post = Server::GetInstance()->GetOperationPostgres();
    hf_uint32 roleid = (*smap)[conn].m_roleid;

    umap_roleGoods playerBagGoods = (*smap)[conn].m_playerGoods;

    _umap_roleGoods::iterator it = playerBagGoods->find(lootGoods->TypeID);
    if(it != playerBagGoods->end()) //物品已有位置
    {
        STR_PackPickGoodsResult t_pickResult;
        t_pickResult.UniqueID = lootGoods->UniqueID;

        for(vector<STR_Goods>::iterator iter = it->second.begin(); iter != it->second.end(); iter++)
        {
            if(iter->Count == GOODSMAXCOUNT) //位置已存满，跳过
            {
                continue;
            }
            else if(iter->Count + lootGoods->Count <= GOODSMAXCOUNT) //这个位置可以存放下
            {
                iter->Count += lootGoods->Count;

                STR_PackGoods t_packGoods(&(*iter));
                conn->Write_all (&t_packGoods, sizeof(STR_PackGoods));

                 t_post->PushUpdateGoods(roleid, &(*iter), PostUpdate); //将新买的物品添加到list
                 t_pickResult.Count += lootGoods->Count;
                 t_pickResult.Result = PICK_SUCCESS;
                 conn->Write_all(&t_pickResult, sizeof(STR_PackPickGoodsResult));

                 Server::GetInstance()->GetGameTask()->UpdateCollectGoodsTaskProcess(conn, lootGoods->TypeID);
                 return 1;
            }
            else //这个位置存放不下，存满当前位置，继续查找新位置
            {
                lootGoods->Count -= GOODSMAXCOUNT - iter->Count;
                t_pickResult.Count += GOODSMAXCOUNT - iter->Count;
                iter->Count = GOODSMAXCOUNT;

                STR_PackGoods t_packGoods(&(*iter));
                conn->Write_all (&t_packGoods, sizeof(STR_PackGoods));

                t_post->PushUpdateGoods(roleid, &(*iter), PostUpdate); //
            }
        }
        if(lootGoods->Count != 0) //开辟新位置存放剩余的物品
        {
            hf_uint8 empty_pos = OperationGoods::GetEmptyPos(conn);
            if(empty_pos == 0) //没有空位置
            {
                t_pickResult.Result = PICKPART_BAGFULL;
                conn->Write_all(&t_pickResult, sizeof(STR_PackPickGoodsResult));
                return 0;
            }

            t_pickResult.Count += lootGoods->Count;
            t_pickResult.Result = PICK_SUCCESS;
            conn->Write_all(&t_pickResult, sizeof(STR_PackPickGoodsResult));

            STR_Goods t_goods;
            t_goods.GoodsID = lootGoods->GoodsID;
            t_goods.TypeID = lootGoods->TypeID;
            t_goods.Count = lootGoods->Count;
            t_goods.Position = empty_pos;

           (*playerBagGoods)[t_goods.GoodsID].push_back(t_goods);

//            cout << "insert goods" << t_goods.GoodsID << "," << t_goods.TypeID << "," << t_goods.Count << "," << t_goods.Position << endl;
             t_post->PushUpdateGoods(roleid, &t_goods, PostInsert); //将新买的物品添加到list
             STR_PackGoods t_packGoods(&t_goods);
             conn->Write_all (&t_packGoods, sizeof(STR_PackGoods));

             Server::GetInstance()->GetGameTask()->UpdateCollectGoodsTaskProcess(conn, lootGoods->TypeID);
             return 1;
        }
    }
    else  //物品没有位置
    {
        hf_uint8 empty_pos = OperationGoods::GetEmptyPos(conn);
        if(empty_pos == 0) //没有空位置
        {
            STR_PackPickGoodsResult t_pickResult(lootGoods->UniqueID, 0, PICK_BAGFULL);
            conn->Write_all(&t_pickResult, sizeof(STR_PackPickGoodsResult));
            return 0;
        }

        STR_Goods t_goods;
        t_goods.GoodsID = lootGoods->GoodsID;
        t_goods.TypeID = lootGoods->TypeID;
        t_goods.Count = lootGoods->Count;
        t_goods.Position = empty_pos;

        vector<STR_Goods> t_vec;
        t_vec.push_back(t_goods);
       (*playerBagGoods)[t_goods.GoodsID] = t_vec;

        STR_PackGoods t_packGoods(&t_goods);
        conn->Write_all (&t_packGoods, sizeof(STR_PackGoods));
        t_post->PushUpdateGoods(roleid, &t_goods, PostInsert); //将新买的物品添加到list

        STR_PackPickGoodsResult t_pickResult(lootGoods->UniqueID, lootGoods->Count, PICK_SUCCESS);
        conn->Write_all(&t_pickResult, sizeof(STR_PackPickGoodsResult));

        Server::GetInstance()->GetGameTask()->UpdateCollectGoodsTaskProcess(conn, lootGoods->TypeID);
        return 1;
    }
}

//捡物品
void OperationGoods::PickUpGoods(TCPConnection::Pointer conn, STR_PackPickGoods* pickGoods)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    if((*(*smap)[conn].m_interchage).isInchange == true) //处于交易状态
    {
        Server::GetInstance()->free(pickGoods);
        return;
    }
    umap_lootGoods lootGoods = SessionMgr::Instance()->GetLootGoods();
//    umap_lootGoods lootGoods = (*smap)[conn].m_lootGoods;
    _umap_lootGoods::iterator loot_it = lootGoods->find(pickGoods->UniqueID);
    if(loot_it == lootGoods->end())  //掉落者不存在
    {
        Server::GetInstance()->free(pickGoods);
        return;
    }
    if(loot_it->second.ProtectStatus != (*smap)[conn].m_roleid)
    {
        time_t timep;
        time(&timep);
        if(loot_it->second.ProtectTime > timep)
        {
            Server::GetInstance()->free(pickGoods);
            return;
        }
    }

    if(loot_it->second.TypeID == Money_1)  //掉落的是金钱
    {
        PickUpMoney(conn, &loot_it->second);
        SessionMgr::Instance()->LootGoodsDelete(pickGoods->UniqueID);
    }
    else if(EquTypeMinValue <= loot_it->second.TypeID && loot_it->second.TypeID <= EquTypeMaxValue)  //直接生成装备ID存起来
    {
        if(PickUpEqu(conn, &loot_it->second) == 1)
        {
            SessionMgr::Instance()->LootGoodsDelete(pickGoods->UniqueID);
        }
    }
    else
    {
        if(PickUpcommonGoods(conn, &loot_it->second) == 1)
        {
            SessionMgr::Instance()->LootGoodsDelete(pickGoods->UniqueID);
        }
    }


//    if(pickGoods->LootGoodsID != 0) //捡取掉落物品中的某个物品
//    {
//        for(vector<STR_LootGoods>::iterator vec = loot_it->second.begin(); vec != loot_it->second.end(); )
//        {
//            if(pickGoods->LootGoodsID == vec->LootGoodsID)
//            {
//                if(pickGoods->LootGoodsID == Money_1)  //掉落的是金钱
//                {
//                    PickUpMoney(conn, &(*vec), pickGoods->GoodsFlag);
//                    vector<STR_LootGoods>::iterator _vec = vec;
//                    vec++;
//                    loot_it->second.erase(_vec);
//                    break;
//                }
//                else if(EquTypeMinValue <= vec->LootGoodsID && pickGoods->LootGoodsID <= EquTypeMaxValue)  //直接生成装备ID存起来
//                {
//                    PickUpEqu(conn, &(*vec), pickGoods->GoodsFlag);
//                    vector<STR_LootGoods>::iterator _vec = vec;
//                    vec++;
//                    loot_it->second.erase(_vec);
//                    break;
//                }
//                else
//                {
//                    if(PickUpcommonGoods(conn, &(*vec), pickGoods->GoodsFlag) == 1)
//                    {
//                        vector<STR_LootGoods>::iterator _vec = vec;
//                        vec++;
//                        loot_it->second.erase(_vec);
//                        break;
//                    }
//                }
//            }
//            vec++;
//            if(vec == loot_it->second.end()) //要捡的物品不存在
//            {
//                STR_PackPickGoodsResult t_pickResult(pickGoods->GoodsFlag, pickGoods->LootGoodsID, 0, PICK_GOODNOTEXIST);
//                conn->Write_all(&t_pickResult, sizeof(STR_PackPickGoodsResult));
//                Server::GetInstance()->free(pickGoods);
//                return;
//            }
//        }

//        if(loot_it->second.empty())
//        {
//            lootGoods->erase(loot_it);
//        }
//        Server::GetInstance()->free(pickGoods);
//        return;
//    }

    //PickGoods->LootGoodsID == 0  //捡取掉落的全部物品
//    cout << loot_it->first << " ,not pick:loot_it->second.size = " << loot_it->second.size() << endl;
//    hf_uint8 i = 0;
//    for(vector<STR_LootGoods>::iterator vec = loot_it->second.begin(); vec != loot_it->second.end();)
//    {
//        i++;
//        if(vec->LootGoodsID == Money_1)  //掉落的是金钱
//        {
//            PickUpMoney(conn, &(*vec), pickGoods->GoodsFlag);
//            vector<STR_LootGoods>::iterator _vec = vec;
//            vec = vec++;
//            loot_it->second.erase(_vec);
//        }
//        else if(EquTypeMinValue <= vec->LootGoodsID && vec->LootGoodsID <= EquTypeMaxValue)  //直接生成装备ID存起来
//        {
//            if(PickUpEqu(conn, &(*vec), pickGoods->GoodsFlag) == 1)
//            {
//                vector<STR_LootGoods>::iterator _vec = vec;
//                vec = vec++;
//                loot_it->second.erase(_vec);
//            }
//            else
//            {
//                vec = vec++;
//            }
//        }
//        else
//        {
//            if(PickUpcommonGoods(conn, &(*vec), pickGoods->GoodsFlag) == 1)
//            {
//                vector<STR_LootGoods>::iterator _vec = vec;
//                vec = vec++;
//                loot_it->second.erase(_vec);
//            }
//            else
//            {
//                vec = vec++;
//            }
//        }
//    }

//    cout << " i = " << i << endl;
//    cout << loot_it->first << " ,pick :loot_it->second.size = " << loot_it->second.size() << endl;
//    if(loot_it->second.empty())
//    {
//        lootGoods->erase(loot_it);
//        cout << "erase:" << loot_it->first << endl;
//    }
//    Server::GetInstance()->free(pickGoods);
}

//查询物品价格
void OperationGoods::QueryGoodsPrice()
{
    StringBuilder       sbd;
    sbd << "select typeid,buyprice,sellprice from t_equipmentattr;";
    DiskDBManager *db = Server::GetInstance()->getDiskDB();
    hf_int32 count = db->GetGoodsPrice(m_goodsPrice,sbd.str());
    if ( count < 0 )
    {
        Logger::GetLogger()->Error("Query equipment price error");
        return;
    }

    sbd.Clear();
    sbd << "select goodsid,buyprice,sellprice from t_consumable;";
    count = db->GetGoodsPrice(m_goodsPrice, sbd.str());
    if ( count < 0 )
    {
        Logger::GetLogger()->Error("Query consumable price error");
        return;
    }

    sbd.Clear();
    sbd << "select goodsid,buyprice,sellprice from t_materialprice;";
    count = db->GetGoodsPrice(m_goodsPrice, sbd.str());
    if ( count < 0 )
    {
        Logger::GetLogger()->Error("Query materia price error");
        return;
    }
}

//查询物品信息
void OperationGoods::QueryConsumableAttr()
{
    DiskDBManager *db = Server::GetInstance()->getDiskDB();
    StringBuilder       sbd;
    sbd << "select goodsid,hp,magic,coldtime,stacknumber,persecondhp,persecondmagic,userlevel,continuetime ,type from t_consumable;";
    hf_int32 count = db->GetConsumableAttr(m_consumableAttr, sbd.str());
    if ( count < 0 )
    {
        Logger::GetLogger()->Error("Query consumableattr error");
        return;
    }
}

//查询装备属性
void OperationGoods::QueryEquAttr()
{
    DiskDBManager *db = Server::GetInstance()->getDiskDB();
    StringBuilder       sbd;
    sbd << "select * from t_equipmentAttr;";
    hf_int32 count = db->GetEquAttr(m_equAttr, sbd.str());
    if ( count < 0 )
    {
        Logger::GetLogger()->Error("Query equipment price error");
        return;
    }
}

//丢弃物品
void OperationGoods::RemoveBagGoods(TCPConnection::Pointer conn, STR_PackRemoveBagGoods* removeGoods)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();

    hf_uint32 roleid = (*smap)[conn].m_roleid;
    if(removeGoods->GoodsID >= EquipMentID)
    {
        _umap_roleEqu::iterator equ_it = (*smap)[conn].m_playerEqu->find(removeGoods->GoodsID);
        if(equ_it == (*smap)[conn].m_playerEqu->end())
        {
            Server::GetInstance()->free(removeGoods);
            return;
        }
        if((*smap)[conn].m_goodsPosition[removeGoods->Position] == POS_LOCKED)
        {
            Server::GetInstance()->free(removeGoods);
            return;
        }   
        OperationGoods::ReleasePos(conn, removeGoods->Position);
        equ_it->second.goods.Count = 0;
        STR_PackGoods t_goods(&equ_it->second.goods);
        conn->Write_all(&t_goods, sizeof(STR_PackGoods));

        ReleasePos(conn, removeGoods->Position); //释放位置

        Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods(roleid, &(equ_it->second.goods), PostDelete);
        Server::GetInstance()->GetOperationPostgres()->PushUpdateEquAttr(roleid, &(equ_it->second.equAttr), PostDelete);

        Server::GetInstance()->GetGameTask()->UpdateCollectGoodsTaskProcess(conn, equ_it->second.goods.TypeID);

        STR_PackPlayerPosition t_pos = (*smap)[conn].m_position;
        time_t timep;
        time(&timep);
        //将丢弃的物品加入到掉落结构里
        STR_LootGoods t_lootGoods;
        t_lootGoods.UniqueID = GetLootGoodsID();
        t_lootGoods.GoodsID = equ_it->second.goods.GoodsID;
        t_lootGoods.TypeID = equ_it->second.goods.TypeID;
        t_lootGoods.MapID = t_pos.MapID;
        t_lootGoods.Pos_x = t_pos.Pos_x + 10;
        t_lootGoods.Pos_y = t_pos.Pos_y;
        t_lootGoods.Pos_z = t_pos.Pos_z + 10;
        t_lootGoods.ProtectTime = (hf_uint32)timep + LootProtectTime;
        t_lootGoods.ContinueTime = (hf_uint32)timep + LootContinueTime;
        t_lootGoods.Count = 1;
        t_lootGoods.ProtectStatus = roleid;

        SessionMgr::Instance()->LootGoodsAdd(t_lootGoods);
        SessionMgr::Instance()->LootEquAttrAdd(equ_it->second.equAttr);
        SessionMgr::Instance()->SendLootGoods(&t_lootGoods);

        (*smap)[conn].m_playerEqu->erase(equ_it);
        Server::GetInstance()->free(removeGoods);
        return;
    }

    umap_roleGoods  playerBagGoods = (*smap)[conn].m_playerGoods;
    _umap_roleGoods::iterator it = playerBagGoods->find(removeGoods->GoodsID);
    if(it == playerBagGoods->end())
    {
        Server::GetInstance()->free(removeGoods);
        return;
    }
    for(vector<STR_Goods>::iterator iter = it->second.begin(); iter != it->second.end(); iter++)
    {
        if(removeGoods->Position == iter->Position)  //找到要删除的位置
        {
            if((*smap)[conn].m_goodsPosition[removeGoods->Position] == POS_LOCKED)
            {
                Server::GetInstance()->free(removeGoods);
                return;
            }
            ReleasePos(conn, removeGoods->Position);

            STR_PackPlayerPosition t_pos = (*smap)[conn].m_position;
            time_t timep;
            time(&timep);

            //将丢弃的物品加入到掉落结构里
            STR_LootGoods t_lootGoods;
            t_lootGoods.UniqueID = GetLootGoodsID();
            t_lootGoods.GoodsID = iter->GoodsID;
            t_lootGoods.TypeID = iter->TypeID;
            t_lootGoods.MapID = t_pos.MapID;
            t_lootGoods.Pos_x = t_pos.Pos_x + 10;
            t_lootGoods.Pos_y = t_pos.Pos_y;
            t_lootGoods.Pos_z = t_pos.Pos_z + 10;
            t_lootGoods.ProtectTime = (hf_uint32)timep + LootProtectTime;
            t_lootGoods.ContinueTime = (hf_uint32)timep + LootContinueTime;
            t_lootGoods.Count = iter->Count;
            t_lootGoods.ProtectStatus = roleid;

            iter->Count = 0;
            STR_PackGoods t_goods(&(*iter));
            conn->Write_all(&t_goods, sizeof(STR_PackGoods));

            Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods(roleid, &(*iter), PostDelete);
            Server::GetInstance()->GetGameTask()->UpdateCollectGoodsTaskProcess(conn, iter->TypeID);

            SessionMgr::Instance()->LootGoodsAdd(t_lootGoods);
            SessionMgr::Instance()->SendLootGoods(&t_lootGoods);

            it->second.erase(iter);
            Server::GetInstance()->free(removeGoods);
            return;
        }
    }
}

//移动或分割物品
void OperationGoods::MoveBagGoods(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods)
{
    if(moveGoods->CurrentPos == moveGoods->TargetPos)
    {
        Server::GetInstance()->free(moveGoods);
        return;
    }
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    if((*smap)[conn].m_goodsPosition[moveGoods->CurrentPos] == POS_LOCKED || (*smap)[conn].m_goodsPosition[moveGoods->TargetPos] == POS_LOCKED)  //当前位置或者目标位置商品处于锁定状态，不可以移动
    {
        Server::GetInstance()->free(moveGoods);
        return;
    }

    if((*smap)[conn].m_goodsPosition[moveGoods->TargetPos] == POS_EMPTY) //如果目标位置为空
    {
        MoveBagGoodsToEmptyPos(conn, moveGoods);
        return;
    }

    if(moveGoods->CurrentGoodsID < EquipMentID && moveGoods->TargetGoodsID < EquipMentID) //都是普通物品
    {
        if(moveGoods->CurrentGoodsID == moveGoods->TargetGoodsID)
        {
            MoveBagCommonGoods(conn, moveGoods);
        }
        else
        {
            ExchangeBagCommonGoods(conn, moveGoods);
        }
    }
    else if(moveGoods->CurrentGoodsID >= EquipMentID && moveGoods->TargetGoodsID < EquipMentID)//当前位置是装备
    {
        ExchangeBagEquAndCommonGoods(conn, moveGoods);
    }
    else if(moveGoods->CurrentGoodsID < EquipMentID && moveGoods->TargetGoodsID >= EquipMentID) //目标位置是装备
    {        
        hf_uint32 t_currentGoodsID = moveGoods->CurrentGoodsID;
        hf_uint16 t_currentPos = moveGoods->CurrentPos;
        moveGoods->CurrentGoodsID = moveGoods->TargetGoodsID;
        moveGoods->CurrentPos = moveGoods->TargetPos;
        moveGoods->TargetGoodsID = t_currentGoodsID;
        moveGoods->TargetPos = t_currentPos;
        ExchangeBagEquAndCommonGoods(conn, moveGoods);
    }
    else //都是装备
    {
        ExchangeBagEqu(conn, moveGoods);
    }
}

void OperationGoods::MoveBagGoodsToEmptyPos(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    if(moveGoods->CurrentGoodsID >= EquipMentID) //移动的是装备
    {
        _umap_roleEqu::iterator equ_it = (*smap)[conn].m_playerEqu->find(moveGoods->CurrentGoodsID);
        if(equ_it == (*smap)[conn].m_playerEqu->end())
        {
            Server::GetInstance()->free(moveGoods);
            return;
        }

        hf_char buff[PACKAGE_LEN] = { 0 };
        STR_PackHead t_packHead;
        t_packHead.Len = sizeof(STR_Goods) * 2;
        t_packHead.Flag = FLAG_BagGoods;
        memcpy(buff, &t_packHead, sizeof(STR_PackHead));

        equ_it->second.goods.Count = 0;
        equ_it->second.goods.Source = Source_Bag;
        memcpy(buff + sizeof(STR_PackHead), &equ_it->second.goods, sizeof(STR_Goods));

        Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(equ_it->second.goods), PostDelete);
        equ_it->second.goods.Position = moveGoods->TargetPos;
        equ_it->second.goods.Count= 1;
        memcpy(buff + sizeof(STR_PackHead) + sizeof(STR_Goods), &equ_it->second.goods, sizeof(STR_Goods));
        conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
        OperationGoods::UsePos(conn, moveGoods->TargetPos);
        OperationGoods::ReleasePos(conn, moveGoods->CurrentPos);

        Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(equ_it->second.goods), PostInsert);
        Server::GetInstance()->free(moveGoods);
        return;
    }

    umap_roleGoods  playerBagGoods = (*smap)[conn].m_playerGoods;
    _umap_roleGoods::iterator it = playerBagGoods->find(moveGoods->CurrentGoodsID);
    if(it == playerBagGoods->end())
    {
        Server::GetInstance()->free(moveGoods);
        return;
    }
    for(vector<STR_Goods>::iterator iter = it->second.begin(); iter != it->second.end(); iter++)
    {
        if(moveGoods->CurrentPos == iter->Position)  //找到要移动的位置
        {
            if(moveGoods->Count > iter->Count)
            {
                Server::GetInstance()->free(moveGoods);
                return;
            }
            else if(moveGoods->Count == iter->Count)
            {
                hf_char buff[PACKAGE_LEN] = { 0 };
                STR_PackHead t_packHead;
                t_packHead.Len = sizeof(STR_Goods) * 2;
                t_packHead.Flag = FLAG_BagGoods;
                memcpy(buff, &t_packHead, sizeof(STR_PackHead));

                hf_uint16 t_count = iter->Count;
                iter->Count = 0;
                iter->Source = Source_Bag;
                memcpy(buff + sizeof(STR_PackHead), &(*iter), sizeof(STR_Goods));

                Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*iter), PostDelete);
                iter->Position = moveGoods->TargetPos;
                iter->Count= t_count;
                memcpy(buff + sizeof(STR_PackHead) + sizeof(STR_Goods), &(*iter), sizeof(STR_Goods));
                conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
                OperationGoods::UsePos(conn, moveGoods->TargetPos);
                OperationGoods::ReleasePos(conn, moveGoods->CurrentPos);

                Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*iter), PostInsert);
                Server::GetInstance()->free(moveGoods);
                return;
            }
            else
            {
                hf_char buff[PACKAGE_LEN] = { 0 };
                STR_PackHead t_packHead;
                t_packHead.Len = sizeof(STR_Goods) * 2;
                t_packHead.Flag = FLAG_BagGoods;
                memcpy(buff, &t_packHead, sizeof(STR_PackHead));

                iter->Count -= moveGoods->Count;
                iter->Source = Source_Bag;
                memcpy(buff + sizeof(STR_PackHead), &(*iter), sizeof(STR_Goods));

                Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*iter), PostUpdate);
                STR_Goods t_goods;
                t_goods.Position = moveGoods->TargetPos;
                t_goods.Count = moveGoods->Count;
                t_goods.GoodsID = iter->GoodsID;
                t_goods.TypeID = iter->TypeID;
                t_goods.Source = iter->Source;
                memcpy(buff + sizeof(STR_PackHead) + sizeof(STR_Goods), &t_goods, sizeof(STR_Goods));
                conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
                OperationGoods::UsePos(conn, moveGoods->TargetPos);

                it->second.push_back(t_goods);
                Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &t_goods, PostInsert);
                Server::GetInstance()->free(moveGoods);
                return;
            }
        }
    }
}

//背包两个装备交换位置
void OperationGoods::ExchangeBagEqu(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    if(moveGoods->CurrentGoodsID == moveGoods->TargetGoodsID)
    {
        Server::GetInstance()->free(moveGoods);
        return;
    }
    _umap_roleEqu::iterator current_it = (*smap)[conn].m_playerEqu->find(moveGoods->CurrentGoodsID);
    if(current_it == (*smap)[conn].m_playerEqu->end() || current_it->second.goods.Position != moveGoods->CurrentPos)
    {
        Server::GetInstance()->free(moveGoods);
        return;
    }
    _umap_roleEqu::iterator target_it = (*smap)[conn].m_playerEqu->find(moveGoods->TargetGoodsID);
    if(target_it == (*smap)[conn].m_playerEqu->end() || target_it->second.goods.Position != moveGoods->TargetPos)
    {
        Server::GetInstance()->free(moveGoods);
        return;
    }

    hf_char buff[PACKAGE_LEN] = { 0 };
    STR_PackHead t_packHead;
    t_packHead.Len = sizeof(STR_Goods) * 2;
    t_packHead.Flag = FLAG_BagGoods;
    memcpy(buff, &t_packHead, sizeof(STR_PackHead));

    current_it->second.goods.Position = moveGoods->TargetPos;
    current_it->second.goods.Source = Source_Bag;
    memcpy(buff + sizeof(STR_PackHead), &current_it->second.goods, sizeof(STR_Goods));

    Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &current_it->second.goods, PostUpdate);

    target_it->second.goods.Position = moveGoods->CurrentPos;
    target_it->second.goods.Source = Source_Bag;
    memcpy(buff + sizeof(STR_PackHead) + sizeof(STR_Goods), &target_it->second.goods, sizeof(STR_Goods));

    conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);

    Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &target_it->second.goods, PostUpdate);
    Server::GetInstance()->free(moveGoods);
}

//背包装备和普通物品交换位置
void OperationGoods::ExchangeBagEquAndCommonGoods(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    _umap_roleEqu::iterator equ_it = (*smap)[conn].m_playerEqu->find(moveGoods->CurrentGoodsID);
    if(equ_it == (*smap)[conn].m_playerEqu->end())
    {
        Server::GetInstance()->free(moveGoods);
        return;
    }
    umap_roleGoods  playerBagGoods = (*smap)[conn].m_playerGoods;
    _umap_roleGoods::iterator it = playerBagGoods->find(moveGoods->TargetGoodsID);
    if(it == playerBagGoods->end())
    {
        Server::GetInstance()->free(moveGoods);
        return;
    }
    for(vector<STR_Goods>::iterator iter = it->second.begin(); iter != it->second.end(); iter++)
    {
        if(moveGoods->TargetPos == iter->Position)  //找到要移动的位置
        {
            hf_char buff[PACKAGE_LEN] = { 0 };
            STR_PackHead t_packHead;
            t_packHead.Len = sizeof(STR_Goods) * 2;
            t_packHead.Flag = FLAG_BagGoods;
            memcpy(buff, &t_packHead, sizeof(STR_PackHead));

            equ_it->second.goods.Position = iter->Position;
            equ_it->second.goods.Source = Source_Bag;
            memcpy(buff + sizeof(STR_PackHead), &equ_it->second.goods, sizeof(STR_Goods));
            Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &equ_it->second.goods, PostUpdate);
            iter->Position = moveGoods->CurrentPos;
            iter->Source = Source_Bag;
            memcpy(buff + sizeof(STR_PackHead) + sizeof(STR_Goods), &(*iter), sizeof(STR_Goods));
            conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
            Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*iter), PostUpdate);
            Server::GetInstance()->free(moveGoods);
        }
    }
}

//移动背包两个位置上的普通物品
void OperationGoods::MoveBagCommonGoods(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();

    umap_roleGoods  playerBagGoods = (*smap)[conn].m_playerGoods;
    vector<STR_Goods>::iterator curPos;
    _umap_roleGoods::iterator cur_goodsID = playerBagGoods->find(moveGoods->CurrentGoodsID);
    if(cur_goodsID == playerBagGoods->end())
    {
        Server::GetInstance()->free(moveGoods);
        return;
    }

    for(curPos = cur_goodsID->second.begin(); curPos != cur_goodsID->second.end();)
    {
        if(moveGoods->CurrentPos == curPos->Position )
        {
            if(moveGoods->Count > curPos->Count) //移动数量大于当前数量
            {
                Server::GetInstance()->free(moveGoods);
                return;
            }
            break;
        }
        curPos++;
        if(curPos == cur_goodsID->second.end())
        {
            Server::GetInstance()->free(moveGoods);
            return;
        }
    }

    vector<STR_Goods>::iterator tarPos;
    for(tarPos = cur_goodsID->second.begin(); tarPos != cur_goodsID->second.end();)
    {
        if(tarPos->Position == moveGoods->TargetPos) break;
        tarPos++;
        if(tarPos == cur_goodsID->second.end()) //没找到目标位置
        {
            Server::GetInstance()->free(moveGoods);
            return;
        }
    }

    hf_char buff[PACKAGE_LEN] = { 0 };
    if(curPos->Count == GOODSMAXCOUNT || tarPos->Count == GOODSMAXCOUNT) //有一个位置已经放满
    {
        hf_uint16 pos = curPos->Position;
        curPos->Position = tarPos->Position;
        tarPos->Position = pos;
        memcpy(buff + sizeof(STR_PackHead), &(*curPos), sizeof(STR_Goods));
        memcpy(buff + sizeof(STR_PackHead) + sizeof(STR_Goods), &(*tarPos), sizeof(STR_Goods));
        Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*curPos), PostUpdate);
        Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*tarPos), PostUpdate);
    }
    else
    {
        if(moveGoods->Count == curPos->Count)//全部移动
        {
            if(tarPos->Count + moveGoods->Count > GOODSMAXCOUNT)
            {   //目标位置放不下,将目标位置放满
                curPos->Count = curPos->Count - (GOODSMAXCOUNT - tarPos->Count);
                tarPos->Count = GOODSMAXCOUNT;
                memcpy(buff + sizeof(STR_PackHead), &(*curPos), sizeof(STR_Goods));
                Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*curPos), PostUpdate);
            }
            else
            {
                curPos->Count = 0;
                memcpy(buff + sizeof(STR_PackHead), &(*curPos), sizeof(STR_Goods));
                tarPos->Count += moveGoods->Count;  //将当前位置的物品添加到目标位置
                Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*curPos), PostDelete);

                cur_goodsID->second.erase(curPos);
                OperationGoods::ReleasePos(conn, moveGoods->CurrentPos);
            }
            memcpy(buff + sizeof(STR_PackHead) + sizeof(STR_Goods), &(*tarPos), sizeof(STR_Goods));
            Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*tarPos), PostUpdate);
        }
        else //移动部分
        {
            if(tarPos->Count + moveGoods->Count > GOODSMAXCOUNT)
            { //目标位置放不下，将目标位置放满
                curPos->Count = curPos->Count - (GOODSMAXCOUNT - tarPos->Count);
                tarPos->Count = GOODSMAXCOUNT;
            }
            else
            {
                curPos->Count -= moveGoods->Count;
                tarPos->Count += moveGoods->Count;
            }
            memcpy(buff + sizeof(STR_PackHead), &(*curPos), sizeof(STR_Goods));
            memcpy(buff + sizeof(STR_PackHead) + sizeof(STR_Goods), &(*tarPos), sizeof(STR_Goods));
            Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*curPos), PostUpdate);
            Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*tarPos), PostUpdate);
        }
    }
    STR_PackHead t_packHead;
    t_packHead.Len = sizeof(STR_Goods) * 2;
    t_packHead.Flag = FLAG_BagGoods;
    memcpy(buff, &t_packHead, sizeof(STR_PackHead));
    conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
    Server::GetInstance()->free(moveGoods);
}

void OperationGoods::ExchangeBagCommonGoods(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    umap_roleGoods  playerBagGoods = (*smap)[conn].m_playerGoods;

    vector<STR_Goods>::iterator curPos;
    _umap_roleGoods::iterator cur_goodsID = playerBagGoods->find(moveGoods->CurrentGoodsID);
    if(cur_goodsID == playerBagGoods->end())//没有找到要移动的商品
    {
        Server::GetInstance()->free(moveGoods);
        return;
    }
    for(curPos = cur_goodsID->second.begin(); curPos != cur_goodsID->second.end();)
    { //查找当前位置
        if(moveGoods->CurrentPos == curPos->Position)
        {
            if(moveGoods->Count > curPos->Count) //移动数量大于实际数量
            {
                Server::GetInstance()->free(moveGoods);
                return;
            }
            break;
        }
        curPos++;
        if(curPos == cur_goodsID->second.end())
        {
            Server::GetInstance()->free(moveGoods);
            return;
        }
    }
    _umap_roleGoods::iterator tar_goodsID = playerBagGoods->find(moveGoods->TargetGoodsID);
    if(tar_goodsID == playerBagGoods->end())
    {
        Server::GetInstance()->free(moveGoods);
        return;
    }

    vector<STR_Goods>::iterator tarPos;
    for(tarPos = tar_goodsID->second.begin(); tarPos != tar_goodsID->second.end();)
    { //查找目标位置
        if(moveGoods->TargetPos == tarPos->Position) break;
        tarPos++;
        if(tarPos == tar_goodsID->second.end())
        {
            Server::GetInstance()->free(moveGoods);
            return;
        }
    }

    hf_char buff[PACKAGE_LEN] = { 0 };
    curPos->Position = tarPos->Position;
    tarPos->Position = moveGoods->CurrentPos;

    curPos->Source = Source_Bag;
    tarPos->Source = Source_Bag;
    memcpy(buff + sizeof(STR_PackHead), &(*curPos), sizeof(STR_Goods));
    memcpy(buff + sizeof(STR_PackHead) + sizeof(STR_Goods), &(*tarPos), sizeof(STR_Goods));
    Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*curPos), PostUpdate);
    Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods((*smap)[conn].m_roleid, &(*tarPos), PostUpdate);

    STR_PackHead t_packHead;
    t_packHead.Len = sizeof(STR_Goods) * 2;
    t_packHead.Flag = FLAG_BagGoods;
    memcpy(buff, &t_packHead, sizeof(STR_PackHead));
    conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
    Server::GetInstance()->free(moveGoods);
    return;
}

//购买物品
void OperationGoods::BuyGoods(TCPConnection::Pointer conn, STR_PackBuyGoods* buyGoods)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    if((*(*smap)[conn].m_interchage).isInchange == true)
    {
        Server::GetInstance()->free(buyGoods);
        return;
    }
    umap_roleMoney t_playerMoney = (*smap)[conn].m_playerMoney;
    umap_goodsPrice::iterator price_it = m_goodsPrice->find(buyGoods->GoodsID);
    if(price_it == m_goodsPrice->end())  //购买的商品不存在
    {
        Server::GetInstance()->free(buyGoods);
        return;
    }
    if(price_it->second.BuyPrice == 0)   //不可买物品
    {
        Logger::GetLogger()->Debug("this goods can not buy");
        Server::GetInstance()->free(buyGoods);
        return;
    }

    if((*t_playerMoney)[Money_1].Count < buyGoods->Count * price_it->second.BuyPrice)  //金钱不够
    {
        STR_PackOtherResult t_result;
        t_result.Flag = FLAG_BuyGoods;
        t_result.result = Buy_NotEnoughMoney;
        conn->Write_all(&t_result, sizeof(STR_PackOtherResult));
        Server::GetInstance()->free(buyGoods);
        return;
    }

    if(EquTypeMinValue <= buyGoods->GoodsID && buyGoods->GoodsID <= EquTypeMaxValue)
    {
        BuyEquipment(conn, buyGoods, &(*t_playerMoney)[Money_1], price_it->second.BuyPrice);
    }
    else
    {
        BuyOtherGoods(conn, buyGoods, &(*t_playerMoney)[Money_1], price_it->second.BuyPrice);
    }
    Server::GetInstance()->free(buyGoods);
}

//出售物品
void OperationGoods::SellGoods(TCPConnection::Pointer conn, STR_PackSellGoods* sellGoods)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    if((*(*smap)[conn].m_interchage).isInchange == true)
    {
        Server::GetInstance()->free(sellGoods);
        return;
    }

    umap_roleMoney t_playerMoney = (*smap)[conn].m_playerMoney;
    hf_uint32 roleid = (*smap)[conn].m_roleid;    
    OperationPostgres* t_post = Server::GetInstance()->GetOperationPostgres();

    if(sellGoods->GoodsID >= EquipMentID)   //出售装备
    {
        umap_roleEqu playerEqu = (*smap)[conn].m_playerEqu;
        _umap_roleEqu::iterator equ_it = playerEqu->find(sellGoods->GoodsID);
        if(equ_it == playerEqu->end())
        {
            Server::GetInstance()->free(sellGoods);
            return;
        }
        STR_GoodsPrice* equPrice = &(*m_goodsPrice)[equ_it->second.goods.TypeID]; //装备价格
        equ_it->second.goods.Count = 0;
        STR_PackGoods t_goods(&(equ_it->second.goods));
        conn->Write_all(&t_goods, sizeof(STR_PackGoods));
        t_post->PushUpdateGoods(roleid, &(equ_it->second.goods), PostDelete);

        ReleasePos(conn, t_goods.goods.Position); //释放位置
        //装备属性
        t_post->PushUpdateEquAttr(roleid, &(equ_it->second.equAttr), PostDelete);

        Server::GetInstance()->GetGameTask()->UpdateCollectGoodsTaskProcess(conn, equ_it->second.goods.TypeID);
        playerEqu->erase(equ_it);

        (*t_playerMoney)[Money_1].Count += equPrice->SellPrice;

        STR_PackPlayerMoney t_money(&(*t_playerMoney)[Money_1]);
        conn->Write_all(&t_money, sizeof(STR_PackPlayerMoney));
        t_post->PushUpdateMoney(roleid, &(*t_playerMoney)[Money_1]);  //将金钱变动插入到list

        Server::GetInstance()->free(sellGoods);
        return;
    }

    umap_roleGoods t_playerGoods = (*smap)[conn].m_playerGoods;
    _umap_roleGoods::iterator goods_it = t_playerGoods->find(sellGoods->GoodsID);
    if(goods_it == t_playerGoods->end()) //背包没有这种物品
    {
        Server::GetInstance()->free(sellGoods);
        return;
    }
    for(vector<STR_Goods>::iterator pos_it = goods_it->second.begin(); pos_it != goods_it->second.end();)
    {
        STR_GoodsPrice* goodsPrice = &(*m_goodsPrice)[sellGoods->GoodsID];
        if(pos_it->Position == sellGoods->Position)
        {          
            pos_it->Count = 0;
            STR_PackGoods t_goods(&(*pos_it));
            conn->Write_all(&t_goods, sizeof(STR_PackGoods));

            t_post->PushUpdateGoods(roleid, &(*pos_it), PostDelete);
            ReleasePos(conn, t_goods.goods.Position); //释放位置

            Server::GetInstance()->GetGameTask()->UpdateCollectGoodsTaskProcess(conn, t_goods.goods.TypeID);
            goods_it->second.erase(pos_it);
            if(goods_it->second.begin() == goods_it->second.end())
            {
                t_playerGoods->erase(goods_it);
            }

            (*t_playerMoney)[Money_1].Count += goodsPrice->SellPrice * pos_it->Count;
            STR_PackPlayerMoney t_money(&(*t_playerMoney)[Money_1]);
            conn->Write_all(&t_money, sizeof(STR_PackPlayerMoney));
            t_post->PushUpdateMoney(roleid, &(*t_playerMoney)[Money_1]);  //将金钱变动插入到list
            Server::GetInstance()->free(sellGoods);
            return;
        }
        pos_it++;
        if(pos_it == goods_it->second.end()) //到结尾了没找到这种物品
        {
            Server::GetInstance()->free(sellGoods);
            return;
        }
    }
}

//得到玩家背包中某种/某类物品的数量
hf_uint32  OperationGoods::GetThisGoodsCount(TCPConnection::Pointer conn, hf_uint32 goodsID)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    umap_roleGoods playerGoods = (*smap)[conn].m_playerGoods;
    if(EquTypeMinValue <= goodsID && goodsID <= EquTypeMaxValue)
    {
        umap_roleEqu playerEqu = (*smap)[conn].m_playerEqu;
        if(playerEqu->size() == 0)
        {
            return 0;
        }
        hf_uint32 count = 0;
        for(_umap_roleEqu::iterator it = playerEqu->begin(); it != playerEqu->end(); it++)
        {
            if(it->second.goods.TypeID == goodsID)
            {
                count++;
            }
        }
        return count;
    }
    _umap_roleGoods::iterator it = playerGoods->find(goodsID);
    if(it == playerGoods->end())
    {
        return 0;
    }
    else
    {
        hf_uint32 count = 0;
        for(vector<STR_Goods>::iterator iter = it->second.begin(); iter != it->second.end(); iter++)
        {
            count += iter->Count;
        }
        return count;
    }
}

//给新捡的装备属性 附初值
void OperationGoods::SetEquAttr(STR_EquipmentAttr* equAttr, hf_uint32 typeID)
{
    umap_equAttr::iterator attr_it = m_equAttr->find(typeID);
    if(attr_it != m_equAttr->end())
    {
        memcpy(equAttr, &attr_it->second, sizeof(STR_EquipmentAttr));
//        equAttr->TypeID = attr_it->second.TypeID;
//        equAttr->PhysicalAttack = attr_it->second.PhysicalAttack;
//        equAttr->PhysicalDefense = attr_it->second.PhysicalDefense;
//        equAttr->MagicAttack = attr_it->second.MagicAttack;
//        equAttr->MagicDefense = attr_it->second.MagicDefense;
//        equAttr->AddHp = attr_it->second.HP;
//        equAttr->AddMagic = attr_it->second.Magic;
//        equAttr->Durability = attr_it->second.Durability;
    }
}

//购买装备
void OperationGoods::BuyEquipment(TCPConnection::Pointer conn, STR_PackBuyGoods* buyGoods, STR_PlayerMoney* money, hf_uint32 price)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    STR_PackOtherResult t_result;
    t_result.Flag = FLAG_BuyGoods;

    hf_uint16 emptyPosCount = OperationGoods::GetEmptyPosCount(conn);
    if(buyGoods->Count > emptyPosCount) //背包满了
    {
        t_result.result = Buy_BagFull;
        conn->Write_all(&t_result, sizeof(STR_PackOtherResult));
        return;
    }
    umap_roleEqu playerEqu = (*smap)[conn].m_playerEqu;
    hf_char buff[PACKAGE_LEN] = { 0 };
    hf_char attrbuff[PACKAGE_LEN] = { 0 };

    STR_PlayerEqu t_equ;
    hf_uint32 roleid = (*smap)[conn].m_roleid;
    OperationPostgres* t_post = Server::GetInstance()->GetOperationPostgres();

    SetEquAttr(&t_equ.equAttr, buyGoods->GoodsID);
    t_equ.goods.TypeID = buyGoods->GoodsID;
    t_equ.goods.Count = 1;
    t_equ.goods.Source = Source_Buy;

    for(hf_uint16 i = 0; i < buyGoods->Count; i++)
    {
        t_equ.goods.Position = OperationGoods::GetEmptyPos(conn);
        t_equ.goods.GoodsID = GetEquipmentID();
        t_equ.equAttr.EquID = t_equ.goods.GoodsID;
        (*playerEqu)[t_equ.goods.GoodsID] = t_equ;

        memcpy(buff + sizeof(STR_PackHead) + i * sizeof(STR_Goods), &t_equ.goods, sizeof(STR_Goods));
        t_post->PushUpdateGoods(roleid, &t_equ.goods, PostInsert); //将新买的物品添加到list

        memcpy(attrbuff + sizeof(STR_PackHead) + i * sizeof(STR_EquipmentAttr), &t_equ.equAttr, sizeof(STR_EquipmentAttr)); //装备属性
        t_post->PushUpdateEquAttr(roleid, &t_equ.equAttr, PostInsert); //将新买的物品添加到list
    }
    Server::GetInstance()->GetGameTask()->UpdateCollectGoodsTaskProcess(conn, buyGoods->GoodsID);

    STR_PackHead t_packHead;
    t_packHead.Len = sizeof(STR_Goods)*buyGoods->Count;
    t_packHead.Flag = FLAG_BagGoods;
    memcpy(buff, &t_packHead, sizeof(STR_PackHead));
    conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);

    t_packHead.Len = sizeof(STR_EquipmentAttr)*buyGoods->Count;
    t_packHead.Flag = FLAG_EquGoodsAttr;
    memcpy(attrbuff, &t_packHead, sizeof(STR_PackHead));
    conn->Write_all(attrbuff, sizeof(STR_PackHead) + t_packHead.Len);

    money->Count -= buyGoods->Count * price;
    t_post->PushUpdateMoney(roleid, money);  //将金钱变动插入到list

    STR_PackPlayerMoney t_money(money);
    conn->Write_all(&t_money, sizeof(STR_PackPlayerMoney));
}

//购买其他物品
void OperationGoods::BuyOtherGoods(TCPConnection::Pointer conn, STR_PackBuyGoods* buyGoods, STR_PlayerMoney* money, hf_uint32 price)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    hf_uint16 emptyPosCount = OperationGoods::GetEmptyPosCount(conn);
    hf_uint16 size = buyGoods->Count/GOODSMAXCOUNT + (buyGoods->Count % GOODSMAXCOUNT ? 1 : 0);
    if(size > emptyPosCount)
    {
        STR_PackOtherResult t_result;
        t_result.Flag = FLAG_BuyGoods;
        t_result.result = Buy_BagFull;
        conn->Write_all(&t_result, sizeof(STR_PackOtherResult));
        return;
    }

    STR_Goods t_goods;
    t_goods.Source = Source_Buy;
    t_goods.GoodsID = buyGoods->GoodsID;
    t_goods.TypeID = buyGoods->GoodsID;
    hf_char buff[PACKAGE_LEN] = { 0 };

    hf_uint16 buyCount = buyGoods->Count;
    umap_roleGoods t_playerGoods = (*smap)[conn].m_playerGoods;
    _umap_roleGoods::iterator goods_it = t_playerGoods->find(buyGoods->GoodsID);
    hf_uint32 roleid = (*smap)[conn].m_roleid;

    hf_uint16 i = 0;
    OperationPostgres* t_post = Server::GetInstance()->GetOperationPostgres();
    if(goods_it == t_playerGoods->end())  //背包还没这种物品
    {
        vector<STR_Goods> t_vec;
        while(buyGoods->Count > 0) //开辟新位置放下剩余的物品
        {
            t_goods.Position = OperationGoods::GetEmptyPos(conn);
            if(buyGoods->Count > GOODSMAXCOUNT)
            {
                t_goods.Count = GOODSMAXCOUNT;
                buyGoods->Count -= GOODSMAXCOUNT;
            }
            else
            {
                t_goods.Count = buyGoods->Count;
                buyGoods->Count = 0;
            }
            t_vec.push_back(t_goods);
            memcpy(buff + sizeof(STR_PackHead) + i * sizeof(STR_Goods), &t_goods, sizeof(STR_Goods));
            i++;
            t_post->PushUpdateGoods(roleid, &t_goods, PostInsert); //将新买的物品添加到list
            break;
        }
        Server::GetInstance()->GetGameTask()->UpdateCollectGoodsTaskProcess(conn, buyGoods->GoodsID);
       (*t_playerGoods)[t_goods.GoodsID] = t_vec;
    }
    else
    {
        //先将未放满的格子放满
        for(vector<STR_Goods>::iterator vec = goods_it->second.begin(); vec != goods_it->second.end(); vec++)
        {
            if(vec->Count == GOODSMAXCOUNT)
            {
                continue;
            }
            if(vec->Count + buyGoods->Count <= GOODSMAXCOUNT) //当前位置能放下剩余的物品
            {
                t_goods.Position = vec->Position;
                t_goods.Count = vec->Count + buyGoods->Count;
                vec->Count = t_goods.Count;
                buyGoods->Count = 0;
                memcpy(buff + sizeof(STR_PackHead) + i * sizeof(STR_Goods), &t_goods, sizeof(STR_Goods));
                i++;
                t_post->PushUpdateGoods(roleid, &t_goods, PostUpdate); //将新买的物品添加到list
                break;
            }
            else
            {
                t_goods.Position = vec->Position;
                buyGoods->Count = buyGoods->Count + vec->Count - GOODSMAXCOUNT;
                t_goods.Count = GOODSMAXCOUNT;
                vec->Count = GOODSMAXCOUNT;
                memcpy(buff + sizeof(STR_PackHead) + i * sizeof(STR_Goods), &t_goods, sizeof(STR_Goods));
                i++;
                t_post->PushUpdateGoods(roleid, &t_goods, PostUpdate); //将新买的物品添加到list
                continue;
            }
        }

        while(buyGoods->Count > 0) //开辟新位置放下剩余的物品
        {
            t_goods.Position = OperationGoods::GetEmptyPos(conn);
            if(buyGoods->Count > GOODSMAXCOUNT)
            {
                t_goods.Count = GOODSMAXCOUNT;
                buyGoods->Count -= GOODSMAXCOUNT;
            }
            else
            {
                t_goods.Count = buyGoods->Count;
                buyGoods->Count = 0;
            }
            goods_it->second.push_back(t_goods);
            memcpy(buff + sizeof(STR_PackHead) + i * sizeof(STR_Goods), &t_goods, sizeof(STR_Goods));
            i++;
            t_post->PushUpdateGoods(roleid, &t_goods, PostInsert); //将新买的物品添加到list
        }
    }

    //给客户端更新背包物品变化和金钱
    STR_PackHead t_packHead;
    t_packHead.Len = sizeof(STR_Goods)*i;
    t_packHead.Flag = FLAG_BagGoods;
    memcpy(buff, &t_packHead, sizeof(STR_PackHead));
    conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);

    money->Count -= buyCount * price;
    t_post->PushUpdateMoney(roleid, money);  //将金钱变动插入到list

    memset(buff, 0, PACKAGE_LEN);
    t_packHead.Len = sizeof(STR_PlayerMoney);
    t_packHead.Flag = FLAG_PlayerMoney;

    memcpy(buff, &t_packHead, sizeof(STR_PackHead));
    memcpy(buff + sizeof(STR_PackHead), money, sizeof(STR_Goods));
    conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
}


//test 查询数据库中装备编号的最大值
void OperationGoods::SetEquIDInitialValue()
{
    m_equipmentID = Server::GetInstance()->getDiskDB()->GetEquIDMaxValue();
}

//整理背包
void OperationGoods::ArrangeBagGoods(TCPConnection::Pointer conn)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    OperationPostgres* t_post = Server::GetInstance()->GetOperationPostgres();
    umap_roleEqu playerEqu = (*smap)[conn].m_playerEqu;

    hf_uint32 roleid = (*smap)[conn].m_roleid;
    StringBuilder       sbd;
    sbd << "delete from t_playergoods where roleid = " << roleid << ";";
    Logger::GetLogger()->Debug(sbd.str());
    hf_int32 value = Server::GetInstance()->getDiskDB()->Set(sbd.str());
    if(value == -1)
    {
        Logger::GetLogger()->Debug("delete player goods fail");
    }

    hf_char playerPosition[BAGCAPACITY] = { 0 };
    memcpy(playerPosition, (*smap)[conn].m_goodsPosition, BAGCAPACITY);
    memset((*smap)[conn].m_goodsPosition, 0, BAGCAPACITY);

    hf_uint8 useBagPosition = 1;
    hf_uint8 i = 0;

    STR_PackHead t_packHead;
    t_packHead.Flag = FLAG_BagGoods;
    hf_char buff[PACKAGE_LEN] = { 0 };
    hf_char bagBuff[BAGCAPACITY] = { 0 };
    //整理装备
    for(_umap_roleEqu::iterator equ_it = playerEqu->begin(); equ_it != playerEqu->end(); equ_it++)
    {
        if(bagBuff[equ_it->second.goods.Position] == POS_NONEMPTY)
        {
            continue;
        }

        equ_it->second.goods.Position = useBagPosition;
        UsePos(conn, useBagPosition);
        bagBuff[useBagPosition] = POS_NONEMPTY;
        useBagPosition++;
        t_post->PushUpdateGoods(roleid, &equ_it->second.goods, PostInsert);      
        memcpy(buff + sizeof(STR_PackHead) + i*sizeof(STR_Goods), &equ_it->second.goods, sizeof(STR_Goods));
        i++;
        if(i == (PACKAGE_LEN - sizeof(STR_PackHead))/sizeof(STR_Goods))
        {
            t_packHead.Len = sizeof(STR_Goods)*i;
            memcpy(buff, &t_packHead, sizeof(STR_PackHead));
            conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
            i = 0;
        }

        _umap_roleEqu::iterator second_it = equ_it;
        second_it++;
        for(;second_it != playerEqu->end();second_it++)
        {
            if(second_it->second.goods.TypeID != equ_it->second.goods.TypeID)
            {
                continue;
            }
            second_it->second.goods.Position = useBagPosition;
            UsePos(conn, useBagPosition);
            bagBuff[useBagPosition] = POS_NONEMPTY;
            useBagPosition++;
            t_post->PushUpdateGoods(roleid, &second_it->second.goods, PostInsert);
            memcpy(buff + sizeof(STR_PackHead) + i*sizeof(STR_Goods), &second_it->second.goods, sizeof(STR_Goods));
            i++;
            if(i == (PACKAGE_LEN - sizeof(STR_PackHead))/sizeof(STR_Goods))
            {
                t_packHead.Len = sizeof(STR_Goods)*i;
                memcpy(buff, &t_packHead, sizeof(STR_PackHead));
                conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
                i = 0;
            }
        }
    }

    //整理普通物品
    umap_roleGoods playerGoods = (*smap)[conn].m_playerGoods;
    for(_umap_roleGoods::iterator goods_it = playerGoods->begin(); goods_it != playerGoods->end(); goods_it++)
    {
        vector<STR_Goods>::iterator start_it = goods_it->second.begin();
        vector<STR_Goods>::iterator end_it = goods_it->second.end();
        --end_it;

        hf_bool flag = true;
        while(1)
        {
            if(start_it == end_it)
            {
                start_it->Source = Source_Bag;
                start_it->Position = useBagPosition;
                UsePos(conn, useBagPosition);
                bagBuff[useBagPosition] = POS_NONEMPTY;
                useBagPosition++;
                t_post->PushUpdateGoods(roleid, &(*start_it), PostInsert);
                memcpy(buff + sizeof(STR_PackHead) + i*sizeof(STR_Goods), &(*start_it), sizeof(STR_Goods));
                i++;
                if(i == (PACKAGE_LEN - sizeof(STR_PackHead))/sizeof(STR_Goods))
                {
                    t_packHead.Len = sizeof(STR_Goods)*i;
                    memcpy(buff, &t_packHead, sizeof(STR_PackHead));
                    conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
                    i = 0;
                }
                break;
            }
            //start_it != end_it
            if(flag)
            {
                start_it->Position = useBagPosition;
                UsePos(conn, useBagPosition);
                bagBuff[useBagPosition] = POS_NONEMPTY;
                useBagPosition++;
                start_it->Source = Source_Bag;
            }

            if(start_it->Count + end_it->Count > GOODSMAXCOUNT)
            {
                start_it->Count = GOODSMAXCOUNT;
                end_it->Count = end_it->Count + start_it->Count - GOODSMAXCOUNT;
                t_post->PushUpdateGoods(roleid, &(*start_it), PostInsert);
                memcpy(buff + sizeof(STR_PackHead) + i*sizeof(STR_Goods), &(*start_it), sizeof(STR_Goods));
                i++;
                if(i == (PACKAGE_LEN - sizeof(STR_PackHead))/sizeof(STR_Goods))
                {
                    t_packHead.Len = sizeof(STR_Goods)*i;
                    memcpy(buff, &t_packHead, sizeof(STR_PackHead));
                    conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
                    i = 0;
                }
                start_it++;
                continue;
            }

            //start_it->Count + end_it->Count <= GOODSMAXCOUNT
            start_it->Count += end_it->Count;
            t_post->PushUpdateGoods(roleid, &(*start_it), PostInsert);
            hf_uint16 count = end_it->Count;
            vector<STR_Goods>::iterator _end_it = end_it;
            end_it--;
            goods_it->second.erase(_end_it);
            if(end_it == start_it)
            {
                memcpy(buff + sizeof(STR_PackHead) + i*sizeof(STR_Goods), &(*start_it), sizeof(STR_Goods));
                i++;
                if(i == (PACKAGE_LEN - sizeof(STR_PackHead))/sizeof(STR_Goods))
                {
                    t_packHead.Len = sizeof(STR_Goods)*i;
                    memcpy(buff, &t_packHead, sizeof(STR_PackHead));
                    conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
                    i = 0;
                }
                break;
            }
            if(start_it->Count + count == GOODSMAXCOUNT)
            {
                start_it++;
                flag = true;
            }
            else
            {
                flag = false;
            }
        }
    }

    STR_Goods goods;
    goods.Count = 0;
    for(hf_uint8 k = useBagPosition; k < BAGCAPACITY; k++)
    {
        if(bagBuff[k] == playerPosition[k])
            continue;
        goods.Position = k;
        memcpy(buff + sizeof(STR_PackHead) + i*sizeof(STR_Goods), &goods, sizeof(STR_Goods));
        i++;
    }
    //打印
//    for(hf_uint8 k = 0; k < i; k++)
//    {
//        memcpy(&goods, buff + sizeof(STR_PackHead) + sizeof(STR_Goods)*k, sizeof(STR_Goods));
//        printf("goodsID:%u,Count:%u,Position:%u\n",goods.GoodsID, goods.Count, goods.Position);
//    }
    if(i != 0)
    {      
        t_packHead.Len = sizeof(STR_Goods)*i;
        memcpy(buff, &t_packHead, sizeof(STR_PackHead));
        conn->Write_all(buff, sizeof(STR_PackHead) + t_packHead.Len);
    }
}

//换装
void OperationGoods::WearBodyEqu(TCPConnection::Pointer conn, hf_uint32 equid, hf_uint8 pos)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    umap_roleEqu playerEqu = (*smap)[conn].m_playerEqu;
    _umap_roleEqu::iterator it = playerEqu->find(equid);
    if(it == playerEqu->end())
    {
        return;
    }
    if(it->second.goods.Position == 0) //此装备已经穿在身上
    {
        return;
    }
    STR_RoleInfo*  roleInfo = &(*smap)[conn].m_roleInfo;
    STR_BodyEquipment* bodyEqu = &(*smap)[conn].m_BodyEqu;
    OperationGoods* t_operGoods = Server::GetInstance()->GetOperationGoods();


    STR_PackGoods packGoods(&it->second.goods);
    packGoods.goods.Count = 0;
    conn->Write_all(&packGoods, sizeof(STR_PackGoods));

    it->second.goods.Position = 0;  //将位置置为0，表示穿在身上

    switch(it->second.equAttr.BodyPos)
    {
    case BodyPos_Head:
    {
        if(bodyEqu->Head != 0) //头上穿着装备
        {
            t_operGoods->DeleteEquAttrToRole(roleInfo, bodyEqu->HeadType);
            STR_PlayerEqu* t_playerEqu = &(*playerEqu)[bodyEqu->Head];
            t_playerEqu->goods.Position = it->second.goods.Position;
            t_playerEqu->goods.Source = Source_Bag;
            STR_PackGoods packGoods(&t_playerEqu->goods);
            conn->Write_all(&packGoods, sizeof(STR_PackGoods));
        }
        bodyEqu->Head = equid;
        break;
    }
    case BodyPos_UpperBody:
    {
        if(bodyEqu->UpperBody != 0)
        {
            t_operGoods->DeleteEquAttrToRole(roleInfo, bodyEqu->UpperBodyType);
            STR_PlayerEqu* t_playerEqu = &(*playerEqu)[bodyEqu->UpperBody];
            t_playerEqu->goods.Position = it->second.goods.Position;
            t_playerEqu->goods.Source = Source_Bag;
            STR_PackGoods packGoods(&t_playerEqu->goods);
            conn->Write_all(&packGoods, sizeof(STR_PackGoods));
        }
        bodyEqu->UpperBody = equid;
        break;
    }
    case BodyPos_Pants:
    {
        if(bodyEqu->Pants != 0)
        {
            t_operGoods->DeleteEquAttrToRole(roleInfo, bodyEqu->PantsType);
            STR_PlayerEqu* t_playerEqu = &(*playerEqu)[bodyEqu->Pants];
            t_playerEqu->goods.Position = it->second.goods.Position;
            t_playerEqu->goods.Source = Source_Bag;
            STR_PackGoods packGoods(&t_playerEqu->goods);
            conn->Write_all(&packGoods, sizeof(STR_PackGoods));
        }
        bodyEqu->Pants = equid;
        break;
    }
    case BodyPos_Shoes:
    {
        if(bodyEqu->Shoes != 0)
        {
            t_operGoods->DeleteEquAttrToRole(roleInfo, bodyEqu->ShoesType);
            STR_PlayerEqu* t_playerEqu = &(*playerEqu)[bodyEqu->Shoes];
            t_playerEqu->goods.Position = it->second.goods.Position;
            t_playerEqu->goods.Source = Source_Bag;
            STR_PackGoods packGoods(&t_playerEqu->goods);
            conn->Write_all(&packGoods, sizeof(STR_PackGoods));
        }
        bodyEqu->Shoes = equid;
        break;
    }
    case BodyPos_Belt:
    {
        if(bodyEqu->Belt != 0)
        {
            t_operGoods->DeleteEquAttrToRole(roleInfo, bodyEqu->BeltType);
            STR_PlayerEqu* t_playerEqu = &(*playerEqu)[bodyEqu->Belt];
            t_playerEqu->goods.Position = it->second.goods.Position;
            t_playerEqu->goods.Source = Source_Bag;
            STR_PackGoods packGoods(&t_playerEqu->goods);
            conn->Write_all(&packGoods, sizeof(STR_PackGoods));
        }
        bodyEqu->Belt = equid;
        break;
    }
    case BodyPos_Neaklace:
    {
        if(bodyEqu->Neaklace != 0)
        {
            t_operGoods->DeleteEquAttrToRole(roleInfo, bodyEqu->NeaklaceType);
            STR_PlayerEqu* t_playerEqu = &(*playerEqu)[bodyEqu->Neaklace];
            t_playerEqu->goods.Position = it->second.goods.Position;
            t_playerEqu->goods.Source = Source_Bag;
            STR_PackGoods packGoods(&t_playerEqu->goods);
            conn->Write_all(&packGoods, sizeof(STR_PackGoods));
        }
        bodyEqu->Neaklace = equid;
        break;
    }
    case BodyPos_Bracelet:
    {
        if(bodyEqu->Bracelet != 0)
        {
            t_operGoods->DeleteEquAttrToRole(roleInfo, bodyEqu->BraceletType);
            STR_PlayerEqu* t_playerEqu = &(*playerEqu)[bodyEqu->Bracelet];
            t_playerEqu->goods.Position = it->second.goods.Position;
            t_playerEqu->goods.Source = Source_Bag;
            STR_PackGoods packGoods(&t_playerEqu->goods);
            conn->Write_all(&packGoods, sizeof(STR_PackGoods));
        }
        bodyEqu->Bracelet = equid;
        break;
    }
    case BodyPos_Ring:
    {
        if(pos == 1)
        {
            if(bodyEqu->LeftRing != 0)
            {
                t_operGoods->DeleteEquAttrToRole(roleInfo, bodyEqu->LeftRingType);
                STR_PlayerEqu* t_playerEqu = &(*playerEqu)[bodyEqu->UpperBodyType];
                t_playerEqu->goods.Position = it->second.goods.Position;
                t_playerEqu->goods.Source = Source_Bag;
                STR_PackGoods packGoods(&t_playerEqu->goods);
                conn->Write_all(&packGoods, sizeof(STR_PackGoods));
            }
            bodyEqu->LeftRing = equid;
        }
        else
        {
            if(bodyEqu->RightRing != 0)
            {
                t_operGoods->DeleteEquAttrToRole(roleInfo, bodyEqu->RightRingType);
                STR_PlayerEqu* t_playerEqu = &(*playerEqu)[bodyEqu->RightRing];
                t_playerEqu->goods.Position = it->second.goods.Position;
                t_playerEqu->goods.Source = Source_Bag;
                STR_PackGoods packGoods(&t_playerEqu->goods);
                conn->Write_all(&packGoods, sizeof(STR_PackGoods));
            }
            bodyEqu->RightRing = equid;
        }
        break;
    }
    case BodyPos_Phone:
    {
        if(bodyEqu->Phone != 0)
        {
            t_operGoods->DeleteEquAttrToRole(roleInfo, bodyEqu->PhoneType);
            STR_PlayerEqu* t_playerEqu = &(*playerEqu)[bodyEqu->Phone];
            t_playerEqu->goods.Position = it->second.goods.Position;
            t_playerEqu->goods.Source = Source_Bag;
            STR_PackGoods packGoods(&t_playerEqu->goods);
            conn->Write_all(&packGoods, sizeof(STR_PackGoods));
        }
        bodyEqu->Phone = equid;
        break;
    }
    case BodyPos_Weapon:
    {
        if(bodyEqu->Weapon != 0)
        {
            t_operGoods->DeleteEquAttrToRole(roleInfo, bodyEqu->WeaponType);
            STR_PlayerEqu* t_playerEqu = &(*playerEqu)[bodyEqu->Weapon];
            t_playerEqu->goods.Position = it->second.goods.Position;
            t_playerEqu->goods.Source = Source_Bag;
            STR_PackGoods packGoods(&t_playerEqu->goods);
            conn->Write_all(&packGoods, sizeof(STR_PackGoods));
        }
        bodyEqu->Weapon = equid;
        break;
    }

    default :
        break;
    }

    t_operGoods->AddEquAttrToRole(roleInfo, equid);
    STR_PackRoleInfo packRoleInfo(roleInfo);
    conn->Write_all(&packRoleInfo, sizeof(STR_PackRoleInfo));

    STR_PackBodyEquipment packBody(bodyEqu);
    conn->Write_all(&packBody, sizeof(STR_PackBodyEquipment));
}

void OperationGoods::TakeOffBodyEqu(TCPConnection::Pointer conn, hf_uint32 equid)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    umap_roleEqu playerEqu = (*smap)[conn].m_playerEqu;
    _umap_roleEqu::iterator it = playerEqu->find(equid);
    if(it == playerEqu->end())
    {
        return;
    }
    if(it->second.goods.Position == 0)
    {
        return;
    }

    hf_uint8 posvalue = GetEmptyPos(conn);
    if(posvalue == 0) //没有空位置
    {
        return;
    }

    STR_PlayerEqu* t_playerEqu = &(*playerEqu)[equid];
    t_playerEqu->goods.Position = posvalue;
    STR_PackGoods packGoods(&t_playerEqu->goods);
    conn->Write_all(&packGoods, sizeof(STR_PackGoods));

    STR_RoleInfo*  roleInfo = &(*smap)[conn].m_roleInfo;
    OperationGoods* t_operGoods = Server::GetInstance()->GetOperationGoods();
    t_operGoods->DeleteEquAttrToRole(roleInfo, equid);
    STR_PackRoleInfo packRoleInfo(roleInfo);
    conn->Write_all(&packRoleInfo, sizeof(STR_PackRoleInfo));

    STR_BodyEquipment* bodyEqu = &(*smap)[conn].m_BodyEqu;

    switch(t_playerEqu->equAttr.BodyPos)
    {
    case BodyPos_Head:
    {
        bodyEqu->Head = 0;
        break;
    }
    case BodyPos_UpperBody:
    {
        bodyEqu->UpperBody = 0;
        break;
    }
    case BodyPos_Pants:
    {
        bodyEqu->Pants = 0;
        break;
    }
    case BodyPos_Shoes:
    {
        bodyEqu->Shoes = 0;
        break;
    }
    case BodyPos_Belt:
    {
        bodyEqu->Belt = 0;
        break;
    }
    case BodyPos_Neaklace:
    {
        bodyEqu->Neaklace = 0;
        break;
    }
    case BodyPos_Bracelet:
    {
        bodyEqu->Bracelet = 0;
        break;
    }
    case BodyPos_Ring:
    {
        if(bodyEqu->LeftRing == equid)
        {
            bodyEqu->LeftRing = 0;
        }
        else
        {
            bodyEqu->RightRing = 0;
        }
        break;
    }
    case BodyPos_Phone:
    {
        bodyEqu->Phone = 0;
        break;
    }
    case BodyPos_Weapon:
    {
        bodyEqu->Weapon = 0;
        break;
    }
    default :
        break;
    }

    STR_PackBodyEquipment packBody(bodyEqu);
    conn->Write_all(&packBody, sizeof(STR_PackBodyEquipment));
}

//角色属性加上装备属性
void OperationGoods::AddEquAttrToRole(STR_RoleInfo* roleinfo, hf_uint32 equTypeid)
{
    STR_EquipmentAttr* equAttr = &(*m_equAttr)[equTypeid];

    roleinfo->Crit_Rate += equAttr->Crit_Rate;
    roleinfo->Dodge_Rate += equAttr->Dodge_Rate;
    roleinfo->Hit_Rate += equAttr->Hit_Rate;
    roleinfo->Resist_Rate += equAttr->Resist_Rate;
    roleinfo->Caster_Speed += equAttr->Caster_Speed;
    roleinfo->Move_Speed += equAttr->Move_Speed;
    roleinfo->Hurt_Speed += equAttr->Hurt_Speed;
    roleinfo->RecoveryLife_Percentage += equAttr->RecoveryLife_Percentage;
    roleinfo->RecoveryLife_value += equAttr->RecoveryLife_value;
    roleinfo->RecoveryMagic_Percentage += equAttr->RecoveryMagic_Percentage;
    roleinfo->RecoveryMagic_value += equAttr->RecoveryMagic_value;
    roleinfo->MagicHurt_Reduction += equAttr->MagicHurt_Reduction;
    roleinfo->PhysicalHurt_Reduction += equAttr->PhysicalHurt_Reduction;
    roleinfo->CritHurt += equAttr->CritHurt;
    roleinfo->CritHurt_Reduction += equAttr->CritHurt_Reduction;

    roleinfo->MaxHP += equAttr->HP;
    roleinfo->HP += equAttr->HP;
    roleinfo->MaxMagic += equAttr->Magic;
    roleinfo->Magic += equAttr->Magic;
    roleinfo->PhysicalDefense += equAttr->PhysicalDefense;
    roleinfo->MagicDefense += equAttr->MagicDefense;
    roleinfo->PhysicalAttack += equAttr->PhysicalAttack;
    roleinfo->MagicAttack += equAttr->MagicAttack;
    roleinfo->Rigorous += equAttr->Rigorous;
    roleinfo->Will += equAttr->Will;
    roleinfo->Wise += equAttr->Wise;
    roleinfo->Mentality += equAttr->Mentality;
    roleinfo->Physical_fitness += equAttr->Physical_fitness;

}


//角色属性去掉此装备属性
void OperationGoods::DeleteEquAttrToRole(STR_RoleInfo* roleInfo, hf_uint32 equTypeid)
{
    STR_EquipmentAttr* equAttr = &(*m_equAttr)[equTypeid];

    roleInfo->Crit_Rate -= equAttr->Crit_Rate;
    roleInfo->Dodge_Rate -= equAttr->Dodge_Rate;
    roleInfo->Hit_Rate -= equAttr->Hit_Rate;
    roleInfo->Resist_Rate -= equAttr->Resist_Rate;
    roleInfo->Caster_Speed -= equAttr->Caster_Speed;
    roleInfo->Move_Speed -= equAttr->Move_Speed;
    roleInfo->Hurt_Speed -= equAttr->Hurt_Speed;
    roleInfo->RecoveryLife_Percentage -= equAttr->RecoveryLife_Percentage;
    roleInfo->RecoveryLife_value -= equAttr->RecoveryLife_value;
    roleInfo->RecoveryMagic_Percentage -= equAttr->RecoveryMagic_Percentage;
    roleInfo->RecoveryMagic_value -= equAttr->RecoveryMagic_value;
    roleInfo->MagicHurt_Reduction -= equAttr->MagicHurt_Reduction;
    roleInfo->PhysicalHurt_Reduction -= equAttr->PhysicalHurt_Reduction;
    roleInfo->CritHurt -= equAttr->CritHurt;
    roleInfo->CritHurt_Reduction -= equAttr->CritHurt_Reduction;

    roleInfo->MaxHP -= equAttr->HP;
    roleInfo->HP -= equAttr->HP;
    roleInfo->MaxMagic -= equAttr->Magic;
    roleInfo->Magic -= equAttr->Magic;
    roleInfo->PhysicalDefense -= equAttr->PhysicalDefense;
    roleInfo->MagicDefense -= equAttr->MagicDefense;
    roleInfo->PhysicalAttack -= equAttr->PhysicalAttack;
    roleInfo->MagicAttack -= equAttr->MagicAttack;
    roleInfo->Rigorous -= equAttr->Rigorous;
    roleInfo->Will -= equAttr->Will;
    roleInfo->Wise -= equAttr->Wise;
    roleInfo->Mentality -= equAttr->Mentality;
    roleInfo->Physical_fitness -= equAttr->Physical_fitness;
}


//使用背包物品恢复属性
void OperationGoods::UseBagGoods(TCPConnection::Pointer conn, hf_uint32 goodsid, hf_uint8 pos)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    umap_roleGoods t_playerGoods = (*smap)[conn].m_playerGoods;

    vector<STR_Goods>* t_goods = &(*t_playerGoods)[goodsid];
    if(t_goods == NULL)//背包没有这种物品
    {
        return;
    }

    hf_double currentTime = Server::GetInstance()->GetGameAttack()->GetCurrentTime();
    hf_double* timep = &(*(*smap)[conn].m_skillTime)[goodsid];
    if(*timep > currentTime) //冷却时间还没到，不可以使用
    {
        return;
    }

    STR_Consumable* t_consum = &(*m_consumableAttr)[goodsid];
    STR_RoleInfo*  t_roleInfo = &(*smap)[conn].m_roleInfo;
    if(t_roleInfo->HP == 0) //角色死亡，不可以使用消耗品
    {
        return;
    }
    switch(t_consum->Type)
    {
    case MomentMagic:
    {
        if(t_roleInfo->Magic == t_roleInfo->MaxMagic) //魔法值是满的
        {
            return;
        }
        if(t_roleInfo->Magic + t_consum->Magic < t_roleInfo->MaxMagic)
            t_roleInfo->Magic += t_consum->Magic;
        else
            t_roleInfo->Magic = t_roleInfo->MaxMagic;
        break;
    }
    case MomentHP:
    {
        if(t_roleInfo->HP == t_roleInfo->MaxHP) //血量是满的
        {
            return;
        }
        if(t_roleInfo->HP + t_consum->HP < t_roleInfo->MaxHP)
            t_roleInfo->HP += t_consum->HP;
        else
            t_roleInfo->HP = t_roleInfo->MaxHP;
        break;
    }
    case SecondMagic:
    {
        STR_RecoveryMagic t_magic(currentTime + 1, t_consum->PersecondMagic, t_consum->ContinueTime - 1);
        SessionMgr::Instance()->RecoveryMagicAdd(conn, t_magic);
        if(t_roleInfo->Magic == t_roleInfo->MaxMagic) //魔法值是满的
        {
            return;
        }
        if(t_roleInfo->Magic + t_consum->Magic < t_roleInfo->MaxMagic)
            t_roleInfo->Magic += t_consum->Magic;
        else
            t_roleInfo->Magic = t_roleInfo->MaxMagic;
        break;
    }
    case SecondHP:
    {
        STR_RecoveryHP t_hp(currentTime + 1, t_consum->PersecondHP, t_consum->ContinueTime - 1 );
        printf("t_hp:%u\n", t_hp.HP);
        SessionMgr::Instance()->RecoveryHPAdd(conn, t_hp);
        if(t_roleInfo->HP == t_roleInfo->MaxHP) //血量是满的
        {
            return;
        }
        if(t_roleInfo->HP + t_consum->HP < t_roleInfo->MaxHP)
            t_roleInfo->HP += t_consum->HP;
        else
            t_roleInfo->HP = t_roleInfo->MaxHP;
        break;
    }
    case MomentHPMagic:
    {
        if(t_roleInfo->HP == t_roleInfo->MaxHP && t_roleInfo->Magic == t_roleInfo->MaxMagic) //血量和魔法值都是满的，不可以使用
        {
            return;
        }
        if(t_roleInfo->HP < t_roleInfo->MaxHP) //血量不满
        {
            if(t_roleInfo->HP + t_consum->HP < t_roleInfo->MaxHP)
                t_roleInfo->HP += t_consum->HP;
            else
                t_roleInfo->HP = t_roleInfo->MaxHP;
        }
        if(t_roleInfo->Magic < t_roleInfo->MaxMagic) //魔法值不满
        {
            if(t_roleInfo->Magic + t_consum->Magic < t_roleInfo->MaxMagic)
                t_roleInfo->Magic += t_consum->Magic;
            else
                t_roleInfo->Magic = t_roleInfo->MaxMagic;
        }
        break;
    }
    case SecondHPMagic:
    {
        STR_RecoveryHPMagic t_hpMagic(currentTime + 1, t_consum->PersecondHP, t_consum->PersecondMagic, t_consum->ContinueTime - 1 );
        SessionMgr::Instance()->RecoveryHPMagicAdd(conn, t_hpMagic);
        if(t_roleInfo->HP == t_roleInfo->MaxHP && t_roleInfo->Magic == t_roleInfo->MaxMagic) //血量和魔法值都是满的，不可以使用
        {
            return;
        }
        if(t_roleInfo->HP < t_roleInfo->MaxHP) //血量不满
        {
            if(t_roleInfo->HP + t_consum->HP < t_roleInfo->MaxHP)
                t_roleInfo->HP += t_consum->HP;
            else
                t_roleInfo->HP = t_roleInfo->MaxHP;
        }
        if(t_roleInfo->Magic < t_roleInfo->MaxMagic) //魔法值不满
        {
            if(t_roleInfo->Magic + t_consum->Magic < t_roleInfo->MaxMagic)
                t_roleInfo->Magic += t_consum->Magic;
            else
                t_roleInfo->Magic = t_roleInfo->MaxMagic;
        }
        break;
    }
    default:
        return;
    }

    *timep = currentTime + t_consum->ColdTime;
    hf_uint32 roleid = (*smap)[conn].m_roleid;
    STR_RoleAttribute t_roleAttr(roleid, t_roleInfo->HP, t_roleInfo->Magic);
    conn->Write_all(&t_roleAttr, sizeof(STR_RoleAttribute));

    Session* sess = &(*smap)[conn];
    sess->SendHPToViewRole(&t_roleAttr);

    vector<STR_Goods>::iterator it = t_goods->begin();
    if(pos != 0)
    {
        for(;it != t_goods->end(); it++)
        {
            if(it->Position == pos)
            {
                break;
            }
            if(it == t_goods->end())
                return;
        }
    }
    it->Count -= 1;
    STR_PackGoods t_packGoods(&(*it));
    conn->Write_all(&t_packGoods, sizeof(STR_PackGoods));
    Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods(roleid, &(*it), PostUpdate);
    if(it->Count == 0)
    {
        Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods(roleid, &(*it), PostDelete);
        ReleasePos(conn, it->Position);
        t_goods->erase(it);
    }
    else
    {
        Server::GetInstance()->GetOperationPostgres()->PushUpdateGoods(roleid, &(*it), PostUpdate);
    }

    if(t_goods->size() == 0)
    {
        t_playerGoods->erase(goodsid);
    }
}
