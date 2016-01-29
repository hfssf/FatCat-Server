#ifndef OPERATIONGOODS_H
#define OPERATIONGOODS_H

#include "./../Game/postgresqlstruct.h"
#include "./../NetWork/tcpconnection.h"
#include "./../Game/cmdtypes.h"

class OperationGoods
{
public:
    OperationGoods();
    ~OperationGoods();


    //捡物品
    void PickUpGoods(TCPConnection::Pointer conn, STR_PackPickGoods* t_pickGoods);

    void PickUpMoney(TCPConnection::Pointer conn, STR_LootGoods* lootGoods, hf_uint32 dropID);
    hf_uint8 PickUpEqu(TCPConnection::Pointer conn, STR_LootGoods* lootGoods, hf_uint32 dropID);
    hf_uint8 PickUpcommonGoods(TCPConnection::Pointer conn, STR_LootGoods* lootGoods, hf_uint32 dropID);

    //查询物品价格
    void QueryGoodsPrice();
    //查询消耗品信息
    void QueryConsumableAttr();
    //查询装备属性
    void QueryEquAttr();
    //查询材料属性

    //丢弃物品
    void RemoveBagGoods(TCPConnection::Pointer conn, STR_PackRemoveBagGoods* removeGoods);
    //移动或分割物品
    void MoveBagGoods(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods);
    //移动或分割物品到空格子
    void MoveBagGoodsToEmptyPos(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods);
    //背包两个装备交换位置
    void ExchangeBagEqu(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods);
    //背包装备和普通物品交换位置
    void ExchangeBagEquAndCommonGoods(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods);
    //移动背包两个位置上的同一普通物品
    void MoveBagCommonGoods(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods);
    //交换背包两个位置上的不同普通物品
    void ExchangeBagCommonGoods(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods);
    //交换物品
//    void ExchangeBagGoods(TCPConnection::Pointer conn, STR_PackMoveBagGoods* moveGoods);

    //购买物品
    void BuyGoods(TCPConnection::Pointer conn, STR_PackBuyGoods* buyGoods);
    //购买装备
    void BuyEquipment(TCPConnection::Pointer conn, STR_PackBuyGoods* buyGoods, STR_PlayerMoney* money, hf_uint32 price);
    //购买其他物品
    void BuyOtherGoods(TCPConnection::Pointer conn, STR_PackBuyGoods* buyGoods, STR_PlayerMoney* money, hf_uint32 price);
    //出售物品
    void SellGoods(TCPConnection::Pointer conn, STR_PackSellGoods* moveGoods);
    //得到新的装备编号

    //得到玩家背包中某种/某类物品的数量
    static hf_uint32  GetThisGoodsCount(TCPConnection::Pointer conn, hf_uint32 goodsID);

    static hf_uint32  GetEquipmentID();

    static void UsePos(TCPConnection::Pointer conn, hf_uint16 pos);      //使用位置
    static void ReleasePos(TCPConnection::Pointer conn, hf_uint16 pos);  //释放位置
    static hf_uint8 GetEmptyPosCount(TCPConnection::Pointer conn);       //得到空位置总数
    static hf_uint8 GetEmptyPos(TCPConnection::Pointer conn);            //查找空位置

    static hf_uint8 JudgeEmptyPos(TCPConnection::Pointer conn, hf_uint8 count); //判断空位值能否放下

    //给新捡的装备属性 附初值
    void SetEquAttr(STR_EquipmentAttr* equAttr, hf_uint32 typeID);

    void SetEquIDInitialValue();

    //整理背包
    void ArrangeBagGoods(TCPConnection::Pointer conn);

    //换装
    void WearBodyEqu(TCPConnection::Pointer conn, hf_uint32 equid, hf_uint8 pos);
    void TakeOffBodyEqu(TCPConnection::Pointer conn, hf_uint32 equid);

    //角色属性加上此装备属性
    void AddEquAttrToRole(STR_RoleInfo* roleinfo, hf_uint32 equTypeid);

    //角色属性去掉此装备属性
    void DeleteEquAttrToRole(STR_RoleInfo* roleinfo, hf_uint32 equTypeid);

    //使用背包物品恢复属性
    void UseBagGoods(TCPConnection::Pointer conn, hf_uint32 goodsid, hf_uint8 pos);


private:
    umap_goodsPrice*    m_goodsPrice;       //物品价格
    umap_consumable*    m_consumableAttr;   //消耗品属性
    umap_equAttr*       m_equAttr;          //装备属性
    static hf_uint32    m_equipmentID;      //装备ID
};

#endif // OPERATIONGOODS_H
