#include "./../OperationPostgres/operationpostgres.h"
#include "./../GameTask/gametask.h"
#include "./../memManage/diskdbmanager.h"
#include "./../Game/getdefinevalue.h"
#include "./../GameTask/gametask.h"
#include "./../Monster/monster.h"
#include "./../Game/session.hpp"
#include "gameattack.h"
#include "./../server.h"
#include "./../PlayerLogin/playerlogin.h"
#include "./../OperationGoods/operationgoods.h"


GameAttack::GameAttack()
    :m_attackRole(new _umap_roleAttackAim)
    ,m_attackMonster(new _umap_roleAttackAim)    
    ,m_attackPoint(new _umap_roleAttackPoint)
    ,m_skillInfo(new umap_skillInfo)
{

}

GameAttack::~GameAttack()
{
    delete m_skillInfo;
}

//攻击点
void GameAttack::AttackPoint(TCPConnection::Pointer conn, STR_PackUserAttackPoint* t_attack)
{
    Server* srv = Server::GetInstance();


    srv->free(t_attack);
}

//攻击目标
void GameAttack::AttackAim(TCPConnection::Pointer conn, STR_PackUserAttackAim* t_attack)
{
    Server* srv = Server::GetInstance();
    if(t_attack->SkillID >= 5001)  //技能攻击
    {
        umap_skillInfo::iterator it = m_skillInfo->find(t_attack->SkillID);
        if(it == m_skillInfo->end())
        {
            Logger::GetLogger()->Debug("not this skill");
            srv->free(t_attack);
            return;
        }
        hf_double timep = GetCurrentTime();

        STR_PackSkillResult t_skillResult;
        t_skillResult.SkillID = t_attack->SkillID;
        if(SkillCoolTime(conn, timep, t_attack->SkillID) == 0)
        {
           t_skillResult.result = SKILL_NOTCOOL;
           conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
           srv->free(t_attack);
           return;
        }
        if(it->second.SkillRangeID == 2)  //自己为圆心
        {
            AimItselfCircle(conn, &it->second, timep);
        }
        else if(it->second.SkillRangeID == 1) //目标
        {
            if(t_attack->AimID > 100000000)
            {
                AimRole(conn,&it->second, timep, t_attack->AimID);
            }
            else if(30000000 <= t_attack->AimID && t_attack->AimID < 40000000)
            {
                AimMonster(conn,&it->second, timep, t_attack->AimID);
            }
        }
        else if(it->second.SkillRangeID == 3) //目标为圆心
        {
            if(t_attack->AimID > 100000000)
            {

            }
            else if(30000000 <= t_attack->AimID && t_attack->AimID < 40000000)
            {
                AimMonsterCircle(conn,&it->second, timep,t_attack->AimID);

            }
        }
    }
    else //普通攻击
    {
        if(t_attack->AimID > 100000000) //攻击玩家
        {
            CommonAttackRole(conn, t_attack);
        }
        else if(30000000 <= t_attack->AimID && t_attack->AimID < 40000000)
        {
            CommonAttackMonster(conn, t_attack);
        }
    }

    srv->free(t_attack);
}

//普通攻击角色
void GameAttack::CommonAttackRole(TCPConnection::Pointer conn, STR_PackUserAttackAim* t_attack)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    STR_PackDamageData t_damageData;

    hf_double timep = GetCurrentTime();
    if((*smap)[conn].m_commonAttackTime > timep) //普通攻击间隔时间过短
    {
        return;
    }
    umap_roleSock t_viewRole = (*smap)[conn].m_viewRole;
    _umap_roleSock::iterator it = t_viewRole->find(t_attack->AimID);
    if(it == t_viewRole->end()) //不在可视范围
    {
        return;
    }

    t_damageData.AimID = t_attack->AimID;
    t_damageData.AttackID = (*smap)[conn].m_roleid;
    t_damageData.TypeID = t_attack->SkillID;


    STR_PackPlayerPosition* t_AttacketPos = &(*smap)[conn].m_position;
    STR_PackPlayerPosition* t_AimPos = &(*smap)[it->second].m_position;

    hf_float dx = t_AimPos->Pos_x - t_AttacketPos->Pos_x;
    hf_float dy = t_AimPos->Pos_y - t_AttacketPos->Pos_y;
    hf_float dz = t_AimPos->Pos_z - t_AttacketPos->Pos_z;
    //判断是否在攻击范围内
    if(dx*dx + dy*dy + dz*dz > PlayerAttackView*PlayerAttackView)
    {
        t_damageData.Flag = NOT_ATTACKVIEW;  //不在攻击范围
        conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
        return;
    }


    //判断方向是否可攻击
    if(dx*cos(t_AttacketPos->Direct) + dz*sin(t_AttacketPos->Direct) < 0)
    {
        t_damageData.Flag = OPPOSITE_DIRECT;
        conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
        return;
    }

    STR_RoleInfo* t_AttacketInfo = &((*smap)[conn].m_roleInfo);
    STR_RoleInfo* t_AimInfo = &(*smap)[it->second].m_roleInfo;

    (*smap)[conn].m_commonAttackTime = timep + HurtSpeed;
    hf_float t_probHit = t_AttacketInfo->Hit_Rate*1;
    if(t_probHit*100 < rand()%100) //未命中
    {
        t_damageData.Flag = NOT_HIT;
        conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
        return;
    }
    if(t_AimInfo->Dodge_Rate*100 >= rand()%100) //闪避
    {
        t_damageData.Flag = Dodge;
        conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
        it->second->Write_all(&t_damageData, sizeof(STR_PackDamageData));
        return;
    }

    hf_uint8 t_level = (*smap)[it->second].m_roleExp.Level;
    hf_uint32 reductionValue = GetDamage_reduction(t_level);
    if(t_attack->SkillID == PhyAttackSkillID) //物理攻击
    {
        if(reductionValue >= t_AimInfo->PhysicalDefense)
            t_damageData.Damage = t_AttacketInfo->PhysicalAttack;
        else
            t_damageData.Damage = t_AttacketInfo->PhysicalAttack* reductionValue/t_AimInfo->PhysicalDefense;
    }
    else if(t_attack->SkillID == MagicAttackSkillID)//魔法攻击
    {
        if(reductionValue >= t_AimInfo->MagicDefense)
            t_damageData.Damage = t_AttacketInfo->MagicAttack;
        else
            t_damageData.Damage = t_AttacketInfo->MagicAttack* reductionValue/t_AimInfo->MagicDefense;
    }

    if(t_AttacketInfo->Crit_Rate*100 > rand()%100)//暴击
    {
        t_damageData.Flag = CRIT_HIT;
        t_damageData.Damage *= 1.5;
    }
    else //未暴击
    {
        t_damageData.Flag = HIT;
    }

    if(t_AimInfo->Resist_Rate*100 >= rand()%100) //抵挡
    {
        t_damageData.Flag = RESIST;
    }

    //发送产生的伤害
    conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
    it->second->Write_all(&t_damageData, sizeof(STR_PackDamageData));
    //减血量
    if(t_AimInfo->HP > t_damageData.Damage)
    {
        t_AimInfo->HP -= t_damageData.Damage;
    }
    else
    {
        t_AimInfo->HP = 0;
    }
    //给被攻击者可视范围内的玩家发送血量信息

    hf_uint32 roleid = (*smap)[it->second].m_roleid;
    STR_RoleAttribute t_roleAttr(roleid, t_AimInfo->HP);
    printf("RoleID:%d,HP%d \n",t_roleAttr.RoleID,t_roleAttr.HP);
    it->second->Write_all(&t_roleAttr, sizeof(STR_RoleAttribute));
    Server::GetInstance()->GetGameAttack()->SendRoleHpToViewRole(it->second, &t_roleAttr);


}

//普通攻击怪物
void GameAttack::CommonAttackMonster(TCPConnection::Pointer conn, STR_PackUserAttackAim* t_attack)
{
    umap_monsterAttackInfo* t_monsterAttack = Server::GetInstance()->GetMonster()->GetMonsterAttack();
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();

    hf_double currentTime = GetCurrentTime();
    if((*smap)[conn].m_commonAttackTime > currentTime) //普通攻击间隔时间过短
    {
        return;
    }

    STR_PackDamageData t_damageData;
    umap_playerViewMonster t_viewMonster = (*smap)[conn].m_viewMonster;
    cout << "t_playerViewMonster->size()" << t_viewMonster->size() << endl;
    _umap_playerViewMonster::iterator it = t_viewMonster->find(t_attack->AimID);
    if(it == t_viewMonster->end())
    {
        return;
    }

    t_damageData.AimID = t_attack->AimID;
    t_damageData.AttackID = (*smap)[conn].m_roleid;
    t_damageData.TypeID = PhyAttackSkillID;


    umap_monsterInfo u_monsterInfo = Server::GetInstance()->GetMonster()->GetMonsterBasic();
    STR_MonsterInfo* t_monsterBasicInfo = &(*u_monsterInfo)[it->first];
    STR_PackPlayerPosition* t_AttacketPosition = &((*smap)[conn].m_position);

    STR_PosDis t_posDis;
    hf_uint8 res = Server::GetInstance()->GetMonster()->JudgeDisAndDirect(t_AttacketPosition, t_monsterBasicInfo, currentTime, &t_posDis);
//    return;
    //判断是否在攻击范围内
    if(res == 1)
    {
        t_damageData.Flag = NOT_ATTACKVIEW;  //不在攻击范围
        conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
        return;
    }

    else if(res == 2) //判断方向是否可攻击
    {
        t_damageData.Flag = OPPOSITE_DIRECT;
//        printf("方向相反，攻击不上\n");
        conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
        return;
    }

    (*smap)[conn].m_commonAttackTime = currentTime + HurtSpeed;

    STR_RoleInfo* t_AttacketInfo = &((*smap)[conn].m_roleInfo);
    hf_float t_probHit = t_AttacketInfo->Hit_Rate*1;

    if(t_monsterBasicInfo->monsterStatus)
        t_probHit = 0;
    if(t_probHit*100 < rand()%100) //未命中
    {
        t_damageData.Flag = NOT_HIT;
        conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
        return;
    }

    //查找怪物攻击属性
    STR_MonsterAttackInfo* t_monsterAttackInfo = &(*t_monsterAttack)[t_monsterBasicInfo->monster.MonsterTypeID];

    hf_uint32 reductionValue = GetDamage_reduction(t_monsterBasicInfo->monster.Level);
    if(t_attack->SkillID == PhyAttackSkillID) //物理攻击
    {
        if(reductionValue >= t_monsterAttackInfo->PhysicalDefense)
            t_damageData.Damage = t_AttacketInfo->PhysicalAttack;
        else
            t_damageData.Damage = t_AttacketInfo->PhysicalAttack* reductionValue/t_monsterAttackInfo->PhysicalDefense;
    }
    else if(t_attack->SkillID == MagicAttackSkillID)//魔法攻击
    {
        if(reductionValue >= t_monsterAttackInfo->MagicDefense)
            t_damageData.Damage = t_AttacketInfo->MagicAttack;
        else
            t_damageData.Damage = t_AttacketInfo->MagicAttack* reductionValue/t_monsterAttackInfo->MagicDefense;
    }

    if(t_AttacketInfo->Crit_Rate*100 >= rand()%100)//暴击
    {
        t_damageData.Flag = CRIT_HIT;
        t_damageData.Damage *= 1.5;
    }
    else //未暴击
    {
        t_damageData.Flag = HIT;
    }
    DamageDealWith(conn, &t_damageData, t_monsterBasicInfo, &t_posDis);
}

//发送玩家血量给周围玩家
void GameAttack::SendRoleHpToViewRole(TCPConnection::Pointer conn, STR_RoleAttribute* roleAttr)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    umap_roleSock t_viewRole = (*smap)[conn].m_viewRole;
    for(_umap_roleSock::iterator it = t_viewRole->begin(); it != t_viewRole->end(); it++)
    {
        it->second->Write_all(roleAttr, sizeof(STR_RoleAttribute));
    }
}

//计算伤害
hf_uint32 GameAttack::CalMonsterDamage(hf_uint8 monsterLevel, STR_PackSkillInfo* skillInfo, STR_RoleInfo* roleInfo, STR_MonsterAttackInfo* monster, hf_uint8* type)
{
    hf_uint32 reductionValue = GetDamage_reduction(monsterLevel);
    hf_uint32 damage = 0;

    if(skillInfo->PhysicalHurt > 0 && skillInfo->MagicHurt > 0)
    {
        *type = PhyMagAttackSkillID;
        if(reductionValue >= monster->PhysicalDefense)
            damage = (skillInfo->PhysicalHurt + skillInfo->PhyPlus*roleInfo->PhysicalAttack);
        else
            damage = (skillInfo->PhysicalHurt + skillInfo->PhyPlus*roleInfo->PhysicalAttack)*reductionValue/monster->PhysicalDefense;

        if(reductionValue >= monster->MagicDefense)
            damage += (skillInfo->MagicHurt + skillInfo->MagPlus*roleInfo->MagicAttack);
        else
            damage += (skillInfo->MagicHurt + skillInfo->MagPlus*roleInfo->MagicAttack)*reductionValue/monster->MagicDefense;
    }
    else if(skillInfo->PhysicalHurt > 0 && skillInfo->MagicHurt == 0)
    {
        *type = PhyAttackSkillID;
        if(reductionValue >= monster->PhysicalDefense)
            damage = (skillInfo->PhysicalHurt + skillInfo->PhyPlus*roleInfo->PhysicalAttack);
        else
            damage = (skillInfo->PhysicalHurt + skillInfo->PhyPlus*roleInfo->PhysicalAttack)*reductionValue/monster->PhysicalDefense;
    }
    else if(skillInfo->PhysicalHurt == 0 && skillInfo->MagicHurt > 0)
    {
        *type = MagicAttackSkillID;
        if(reductionValue >= monster->MagicDefense)
            damage = (skillInfo->MagicHurt + skillInfo->MagPlus*roleInfo->MagicAttack);
        else
            damage = (skillInfo->MagicHurt + skillInfo->MagPlus*roleInfo->MagicAttack)*reductionValue/monster->MagicDefense;
    }   
    return damage;
}

//计算技能对玩家产生的伤害
hf_uint32 GameAttack::CalRoleDamage(hf_uint8 roleLevel, STR_PackSkillInfo* skillInfo, STR_RoleInfo* attackInfo, STR_RoleInfo* aimInfo, hf_uint8* type)
{
    hf_uint32 reductionValue = GetDamage_reduction(roleLevel);
    hf_uint32 damage = 0;

    if(skillInfo->PhysicalHurt > 0 && skillInfo->MagicHurt > 0)
    {
        *type = PhyMagAttackSkillID;
        if(reductionValue >= aimInfo->PhysicalDefense)
            damage = (skillInfo->PhysicalHurt + skillInfo->PhyPlus*attackInfo->PhysicalAttack);
        else
            damage = (skillInfo->PhysicalHurt + skillInfo->PhyPlus*attackInfo->PhysicalAttack)*reductionValue/aimInfo->PhysicalDefense;

        if(reductionValue >= aimInfo->MagicDefense)
            damage += (skillInfo->MagicHurt + skillInfo->MagPlus*attackInfo->MagicAttack);
        else
            damage += (skillInfo->MagicHurt + skillInfo->MagPlus*attackInfo->MagicAttack)*reductionValue/aimInfo->MagicDefense;
    }
    else if(skillInfo->PhysicalHurt > 0 && skillInfo->MagicHurt == 0)
    {
        *type = PhyAttackSkillID;
        if(reductionValue >= aimInfo->PhysicalDefense)
            damage = (skillInfo->PhysicalHurt + skillInfo->PhyPlus*attackInfo->PhysicalAttack);
        else
            damage = (skillInfo->PhysicalHurt + skillInfo->PhyPlus*attackInfo->PhysicalAttack)*reductionValue/aimInfo->PhysicalDefense;
    }
    else if(skillInfo->PhysicalHurt == 0 && skillInfo->MagicHurt > 0)
    {
        *type = MagicAttackSkillID;
        if(reductionValue >= aimInfo->MagicDefense)
            damage = (skillInfo->MagicHurt + skillInfo->MagPlus*attackInfo->MagicAttack);
        else
            damage = (skillInfo->MagicHurt + skillInfo->MagPlus*attackInfo->MagicAttack)*reductionValue/aimInfo->MagicDefense;
    }
    return damage;
}

void GameAttack::RoleViewDeleteMonster(hf_uint32 monsterID)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    _umap_viewRole* t_viewRole = &(*(Server::GetInstance()->GetMonster()->GetMonsterViewRole()))[monsterID];  //得到能看到这个怪物的玩家
    umap_roleSock t_roleSock = SessionMgr::Instance()->GetRoleSock();
    for(_umap_viewRole::iterator it = t_viewRole->begin(); it != t_viewRole->end(); it++)
    {
        _umap_roleSock::iterator role_it = t_roleSock->find(it->first);
        if(role_it != t_roleSock->end())
        {
            umap_playerViewMonster   t_viewMonster = (*smap)[role_it->second].m_viewMonster;
            _umap_playerViewMonster::iterator iter = t_viewMonster->find(monsterID);
            if(iter == t_viewMonster->end())
            {
                Logger::GetLogger()->Error("monster view player,player not view monster");
                continue;
            }
            t_viewMonster->erase(iter);
        }
    }
}

//技能处理函数
void GameAttack::DamageDealWith(TCPConnection::Pointer conn, STR_PackDamageData* damage, STR_MonsterInfo* monster, STR_PosDis* posDis)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    //发送产生的伤害
    conn->Write_all(damage, sizeof(STR_PackDamageData));

    STR_PackMonsterAttrbt t_monsterBt;
    t_monsterBt.MonsterID = monster->monster.MonsterID;
    hf_uint32 roleid = (*smap)[conn].m_roleid;
    double timep = GetCurrentTime();
    t_monsterBt.HP = monster->ReduceHp(roleid, damage->Damage, timep);

    umap_monsterViewRole  monsterViewRole = Server::GetInstance()->GetMonster()->GetMonsterViewRole();

//    cout << "hatredRoleid" << monster->hatredRoleid << ",hatred:" << ((*monsterViewRole)[t_monsterBt.MonsterID])[monster->hatredRoleid] << endl;
    //发送怪物当前血量给可视范围内的玩家
    Server::GetInstance()->GetMonster()->SendMonsterHPToViewRole(&t_monsterBt);
    if(t_monsterBt.HP == 0)
    {
        //怪物死亡，发送奖励经验，玩家经验，查找掉落物品
        MonsterDeath(conn, monster);
        //从玩家可视范围内的怪物列表中删除该怪物
        RoleViewDeleteMonster(t_monsterBt.MonsterID);
        //删除该怪物可视范围内的玩家
        (*monsterViewRole)[t_monsterBt.MonsterID].clear();
//        monsterViewRole->erase(t_monsterBt.MonsterID);
        return;
    }

    Logger::GetLogger()->Debug("attack before monsterid:%u hatredroleid:%u\n",monster->monster.MonsterID, monster->hatredRoleid);

    ((*monsterViewRole)[t_monsterBt.MonsterID])[roleid] += damage->Damage;
    hf_uint32 t_hatredValue = ((*monsterViewRole)[t_monsterBt.MonsterID])[roleid];

    printf("roleid:%d,hatredvalue:%d\n", roleid, t_hatredValue);\
    if(monster->hatredRoleid != roleid)
    {
        if((monster->hatredRoleid != 0 && t_hatredValue > ((*monsterViewRole)[t_monsterBt.MonsterID])[monster->hatredRoleid]) || monster->hatredRoleid == 0)
        {
            if(monster->aimTime - timep > 0.002) //时间大于0.002秒时，重新确定时间和位置点
            {
                monster->ChangeAimTimeAndPos(roleid, timep, posDis);
                Server::GetInstance()->GetMonster()->SendMonsterToViewRole(&monster->monster);
            }
            else
            {
                Logger::GetLogger()->Debug("wating time less than 0.002 second\n");
                monster->ChangeHatredRoleid(roleid);
            }
        }        
    }
    Logger::GetLogger()->Debug("attack later monsterid:%u hatredroleid:%u\n",monster->monster.MonsterID, monster->hatredRoleid);
}


//怪物死亡处理函数
void GameAttack::MonsterDeath(TCPConnection::Pointer conn, STR_MonsterInfo* monster)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    Server* srv = Server::GetInstance();
    //查找此任务是否为任务进度里要打的怪，如果是，更新任务进度
    srv->GetGameTask()->UpdateAttackMonsterTaskProcess(conn, monster->monster.MonsterTypeID);

    //前15级怪物死亡不掉经验
//     if(monster->monster.Level >= 15)
//     {
        STR_PackRewardExperience t_RewardExp;
        t_RewardExp.ID = monster->monster.MonsterID;
        t_RewardExp.Experience = GetRewardExperience(monster->monster.Level);
        Logger::GetLogger()->Debug("monster reward:monsterID:%u,experiense:%u",t_RewardExp.ID, t_RewardExp.Experience);
        conn->Write_all(&t_RewardExp, sizeof(STR_PackRewardExperience));

        STR_PackRoleExperience* t_RoleExp = &(*smap)[conn].m_roleExp;

        //玩家升级
        if(t_RoleExp->CurrentExp + t_RewardExp.Experience >= t_RoleExp->UpgradeExp)
        {            
            t_RoleExp->Level += 1;
            Server::GetInstance()->GetOperationPostgres()->PushUpdateLevel((*smap)[conn].m_roleid, t_RoleExp->Level);
            hf_uint8 t_profession = (*smap)[conn].m_RoleBaseInfo.Profession;
            STR_RoleInfo* t_roleInfo = &(*smap)[conn].m_roleInfo;

            //更新玩家属性
            Server::GetInstance()->GetPlayerLogin()->UpdateJobAttr(t_profession, t_RoleExp->Level, t_roleInfo);

            srv->GetGameTask()->UpdateAttackUpgradeTaskProcess(conn, t_RoleExp->Level);
            t_RoleExp->CurrentExp = t_RoleExp->CurrentExp + t_RewardExp.Experience - t_RoleExp->UpgradeExp;
            t_RoleExp->UpgradeExp = GetUpgradeExprience(t_RoleExp->Level);
        }
        else
        {
            t_RoleExp->CurrentExp = t_RoleExp->CurrentExp + t_RewardExp.Experience;
        }
        Server::GetInstance()->GetOperationPostgres()->PushUpdateExp((*smap)[conn].m_roleid, t_RoleExp->CurrentExp);
        conn->Write_all(t_RoleExp, sizeof(STR_PackRoleExperience));
//      }


    STR_LootGoods t_LootGoods;
    t_LootGoods.Pos_x = monster->monster.Current_x - 2;
    t_LootGoods.Pos_y = monster->monster.Current_y + 4;
    t_LootGoods.Pos_z = monster->monster.Current_z;
    t_LootGoods.MapID = monster->monster.MapID;
    time_t timep;
    time(&timep);

    t_LootGoods.ProtectTime = (hf_uint32)timep + LootProtectTime;
    t_LootGoods.ProtectStatus = (*smap)[conn].m_roleid;
    t_LootGoods.ContinueTime = (hf_uint32)timep + LootContinueTime;
//    umap_lootGoods lootGoods = (*smap)[conn].m_lootGoods;
    printf("protectTime:%u,    ContinueTime:%u\n",t_LootGoods.ProtectTime, t_LootGoods.ContinueTime);
    t_LootGoods.Count = GetRewardMoney(monster->monster.Level);;

    //暂时只取奖励的金钱，后面会加一些计算公式，计算后奖励的金钱可能为0

//    Logger::GetLogger()->Debug("monster loot money:monsterid:%u,count:%u",monster->monster.MonsterID, t_packLootGoods.Count);
    if(t_LootGoods.Count > 0)
    {
        t_LootGoods.UniqueID = OperationGoods::GetLootGoodsID();
        t_LootGoods.TypeID = Money_1;
        SessionMgr::Instance()->LootGoodsAdd(t_LootGoods);
//        (*lootGoods)[t_packLootGoods.UniqueID] = t_packLootGoods;
        SessionMgr::Instance()->SendLootGoods(&t_LootGoods);
    }
    umap_monsterLoot* t_monsterLoot = Server::GetInstance()->GetMonster()->GetMonsterLoot();
    umap_monsterLoot::iterator iter = t_monsterLoot->find(monster->monster.MonsterTypeID);
    if(iter != t_monsterLoot->end())
    {
        //可能掉落多个物品，分别判断
        for(vector<STR_MonsterLoot>::iterator vec = iter->second.begin(); vec != iter->second.end(); vec++)
        {
            //掉落条件判断

            //掉落可能性判断
            if(vec->LootProbability*100 >= rand()%100)
            {
                t_LootGoods.UniqueID = OperationGoods::GetLootGoodsID();
                if(20001 <= vec->LootGoodsID && vec->LootGoodsID <= 30000)
                {
                    t_LootGoods.GoodsID = OperationGoods::GetEquipmentID();
                }
                else
                {
                    t_LootGoods.GoodsID = vec->LootGoodsID;
                }
                t_LootGoods.TypeID = vec->LootGoodsID;
                t_LootGoods.Count = vec->Count;
//                (*lootGoods)[t_packLootGoods.UniqueID] = t_packLootGoods;
                t_LootGoods.Pos_x += 5;
                SessionMgr::Instance()->LootGoodsAdd(t_LootGoods);
                SessionMgr::Instance()->SendLootGoods(&t_LootGoods);
            }
        }                

//        LootPositionTime t_lootPositionTime;
//        time_t timep;
//        time(&timep);
//        t_lootPositionTime.timep = (hf_uint32)timep;
//        t_lootPositionTime.continueTime = GOODS_CONTINUETIME;

//        memcpy(&t_lootPositionTime.goodsPos, &t_PacklootGoods, sizeof(STR_LootGoodsPos));
//        (*((*smap)[conn].m_lootPosition))[t_PacklootGoods.GoodsFlag] = t_lootPositionTime; //保存掉落物品时间位置
    }
}


//角色技能伤害
void GameAttack::RoleSkillAttack()
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    umap_roleSock t_roleSock = SessionMgr::Instance()->GetRoleSock();
    umap_monsterAttackInfo* t_monsterAttack = Server::GetInstance()->GetMonster()->GetMonsterAttack();
    umap_monsterInfo u_monsterInfo = Server::GetInstance()->GetMonster()->GetMonsterBasic();
    STR_PackDamageData t_damageData;

    //遍历m_attackMonster,m_attackRole,m_attackPoint 根据时间判断，计算伤害发送给玩家
    while(1)
    {
//        if(m_attackMonster->size() == 0)
//        {
//            usleep(1000);
//        }
        hf_double t_currentTime = GetCurrentTime();
        for(_umap_roleAttackAim::iterator it = m_attackMonster->begin(); it != m_attackMonster->end();)
        {
            if(t_currentTime < (it->second).HurtTime) //时间没到
            {
                it++;
                continue;
            }

            _umap_roleSock::iterator iter = t_roleSock->find(it->first);
            if(iter == t_roleSock->end())  //攻击的玩家不在线
            {
                _umap_roleAttackAim::iterator aim = it;
                it++;
                m_attackMonster->erase(aim);
                continue;
            }

            STR_PackSkillAimEffect t_skillEffect(it->second.AimID,it->second.SkillID,it->first);
            iter->second->Write_all(&t_skillEffect, sizeof(STR_PackSkillAimEffect));  //发送施法效果

            umap_playerViewMonster t_viewMonster = (*smap)[iter->second].m_viewMonster;
            STR_PackPlayerPosition* t_pos = &(*smap)[iter->second].m_position;
            STR_PackSkillInfo* t_skillInfo = &((*m_skillInfo)[it->second.SkillID]);
            STR_RoleInfo* t_AttacketInfo = &(*smap)[iter->second].m_roleInfo;
            STR_PackSkillResult t_skillResult;
            t_skillResult.SkillID = t_skillInfo->SkillID;
            if(t_skillInfo->SkillRangeID == 1)  //目标
            {
                _umap_playerViewMonster::iterator monster = t_viewMonster->find(it->second.AimID);
                if(monster == t_viewMonster->end())
                {
                    continue;
                }

                STR_MonsterInfo* t_monsterInfo = &(*u_monsterInfo)[it->second.AimID];

                 STR_MonsterAttackInfo* t_monsterAttackInfo = &(*t_monsterAttack)[t_monsterInfo->monster.MonsterTypeID];
                 hf_float dx = t_monsterInfo->monster.Current_x - t_pos->Pos_x;
                 hf_float dy = t_monsterInfo->monster.Current_y - t_pos->Pos_y;
                 hf_float dz = t_monsterInfo->monster.Current_z - t_pos->Pos_z;
                 hf_float dis = sqrt(dx*dx + dy*dy + dz*dz);
                 if( dis >= t_skillInfo->NearlyDistance && dis <= t_skillInfo->FarDistance)
                 {
                     hf_float t_probHit = t_AttacketInfo->Hit_Rate*1;
                     if(t_monsterInfo->monsterStatus) //怪物处于返回中
                         t_probHit = 0;
                     if(t_probHit*100 < rand()%100) //未命中
                     {
                         t_damageData.TypeID = PhyAttackSkillID;
                         t_damageData.Flag = NOT_HIT;
                         iter->second->Write_all(&t_damageData, sizeof(STR_PackDamageData));
                         continue;
                     }

                     t_skillResult.result = SKILL_SUCCESS;
                     iter->second->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
                     t_damageData.Damage = CalMonsterDamage(t_monsterInfo->monster.Level, t_skillInfo, t_AttacketInfo, t_monsterAttackInfo, &t_damageData.TypeID);

                     if(t_AttacketInfo->Crit_Rate*100 >= rand()%100)//暴击
                     {
                         t_damageData.Flag = CRIT_HIT;
                         t_damageData.Damage *= 1.5;
                     }
                     else //未暴击
                     {
                         t_damageData.Flag = HIT;
                     }

                     cout << "wait Skill" << t_damageData.Damage << endl;
                     STR_PosDis t_posDis(dis, 0 - dx, 0 - dz);
                     DamageDealWith(iter->second, &t_damageData, t_monsterInfo, &t_posDis); //发送计算伤害
                 }
            }
            else if(t_skillInfo->SkillRangeID == 2) //自己为圆心
            {
                for(_umap_playerViewMonster::iterator monster = t_viewMonster->begin(); monster != t_viewMonster->end(); monster++)
                {                  
                     STR_MonsterInfo* t_monsterInfo = &(*u_monsterInfo)[monster->first];
                     STR_MonsterAttackInfo* t_monsterAttackInfo = &(*t_monsterAttack)[t_monsterInfo->monster.MonsterTypeID];
                     hf_float dx = t_monsterInfo->monster.Current_x - t_pos->Pos_x;
                     hf_float dy = t_monsterInfo->monster.Current_y - t_pos->Pos_y;
                     hf_float dz = t_monsterInfo->monster.Current_z - t_pos->Pos_z;
                     hf_float dis = sqrt(dx*dx + dy*dy + dz*dz);
                     if( dis >= t_skillInfo->NearlyDistance && dis <= t_skillInfo->FarDistance)
                     {
                         t_damageData.Damage = CalMonsterDamage(t_monsterInfo->monster.Level, t_skillInfo, t_AttacketInfo, t_monsterAttackInfo, &t_damageData.TypeID);
                         STR_PosDis posDis(dis, 0 - dx, 0 - dz);
                         DamageDealWith(iter->second, &t_damageData, t_monsterInfo, &posDis); //发送计算伤害
                     }
                }
            }
            else if( t_skillInfo->SkillRangeID == 3) //目标为圆心
            {
                _umap_playerViewMonster::iterator monster = t_viewMonster->find(it->second.AimID);
                if(monster == t_viewMonster->end())
                {
                    continue;
                }
                STR_MonsterInfo* t_monsterInfo = &(*u_monsterInfo)[monster->first];
                hf_float dx = t_monsterInfo->monster.Current_x - t_pos->Pos_x;
                hf_float dy = t_monsterInfo->monster.Current_y - t_pos->Pos_y;
                hf_float dz = t_monsterInfo->monster.Current_z - t_pos->Pos_z;
                if( (dx*dx + dy*dy + dz*dz) < t_skillInfo->NearlyDistance * t_skillInfo->NearlyDistance || (dx*dx + dy*dy + dz*dz) > t_skillInfo->FarDistance * t_skillInfo->FarDistance)
                {
                    continue;
                }
                for(_umap_playerViewMonster::iterator monster = t_viewMonster->begin(); monster != t_viewMonster->end(); monster++)
                {
                    if(monster->first == t_monsterInfo->monster.MonsterID)
                    {
                        continue;
                    }

                    STR_MonsterInfo* t_monster = &(*u_monsterInfo)[monster->first];
                    STR_MonsterAttackInfo* t_monsterAttackInfo = &(*t_monsterAttack)[t_monster->monster.MonsterTypeID];

                    hf_float dx = t_monsterInfo->monster.Current_x - t_monster->monster.Current_x;
                    hf_float dy = t_monsterInfo->monster.Current_y - t_monster->monster.Current_x;
                    hf_float dz = t_monsterInfo->monster.Current_z - t_monster->monster.Current_x;
                    hf_float dis = sqrt(dx*dx + dy*dy + dz*dz);
                    if( dis >= t_skillInfo->NearlyDistance && dis <= t_skillInfo->FarDistance)
                    {
                      t_damageData.Damage = CalMonsterDamage(t_monsterInfo->monster.Level, t_skillInfo, t_AttacketInfo, t_monsterAttackInfo, &t_damageData.TypeID);
                      STR_PosDis posDis(dis, 0 - dx, 0 - dz);
                      DamageDealWith(iter->second, &t_damageData, t_monster, &posDis); //发送计算伤害
                    }
                }
            }
            _umap_roleAttackAim::iterator aim = it;
            it++;
            m_attackMonster->erase(aim);
        }
//        for(_umap_roleAttackAim::iterator it = m_attackRole->begin(); it != m_attackRole->end(); it++)
//        {  //攻击角色

//        }

//        for(_umap_roleAttackPoint::iterator it = m_attackPoint->begin(); it != m_attackPoint->end(); it++)
//        { //攻击点

//        }


        RoleRecoveryHP(smap, t_currentTime);
        RoleRecoveryMagic(smap, t_currentTime);
        RoleRecoveryHPMagic(smap, t_currentTime);

        usleep(1000);
    }
}

void GameAttack::RoleRecoveryHP(SessionMgr::SessionPointer smap, hf_double currentTime)
{
//    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    umap_recoveryHP t_recoveryHP = SessionMgr::Instance()->GetRecoveryHP();
    for(_umap_recoveryHP::iterator HP_it = t_recoveryHP->begin(); HP_it != t_recoveryHP->end();)
    {
//        cout << "recoveryHP start" << endl;
//        printf("timep:%lf,currentTime:%lf\n", HP_it->second.Timep, currentTime);
        if(HP_it->second.Timep >= currentTime) //时间没到
        {
            HP_it++;
            continue;
        }
        Session* sess = &(*smap)[HP_it->first];
        if(sess == NULL) //玩家退出游戏
        {
            _umap_recoveryHP::iterator _HP_it = HP_it;
            HP_it++;
            SessionMgr::Instance()->RecoveryHPDelete(_HP_it->first);
            continue;
        }
        STR_RoleInfo* t_roleInfo = &(*smap)[HP_it->first].m_roleInfo;
        if(t_roleInfo->HP == t_roleInfo->MaxHP)
        {
            HP_it->second.Count -= 1;
            if(HP_it->second.Count >= 1)
            {
                HP_it->second.Timep = currentTime + 1;
                HP_it++;
            }
            else   //作用时间结束了
            {
                _umap_recoveryHP::iterator _HP_it = HP_it;
                HP_it++;
                SessionMgr::Instance()->RecoveryHPDelete(_HP_it->first);
            }
            continue;
        }
        if(t_roleInfo->HP == 0) //玩家已经死亡
        {
            _umap_recoveryHP::iterator _HP_it = HP_it;
            HP_it++;
            SessionMgr::Instance()->RecoveryHPDelete(_HP_it->first);
            continue;
        }

        if(t_roleInfo->HP + HP_it->second.HP < t_roleInfo->MaxHP)
            t_roleInfo->HP += HP_it->second.HP;
        else
            t_roleInfo->HP = t_roleInfo->MaxHP;

        hf_uint32 roleid = (*smap)[HP_it->first].m_roleid;
        STR_RoleAttribute t_roleAttr(roleid, t_roleInfo->HP, t_roleInfo->Magic);
        HP_it->first->Write_all(&t_roleAttr, sizeof(STR_RoleAttribute));
        cout << "roleid:" << t_roleAttr.RoleID << ",HP:" << t_roleAttr.HP << ",magic:" << t_roleAttr.Magic << endl;
        sess->SendHPToViewRole(&t_roleAttr);

        HP_it->second.Count -= 1;
        if(HP_it->second.Count >= 1)
        {
            HP_it->second.Timep = currentTime + 1;
            HP_it++;
        }
        else   //作用时间结束了
        {
            _umap_recoveryHP::iterator _HP_it = HP_it;
            HP_it++;
            SessionMgr::Instance()->RecoveryHPDelete(_HP_it->first);
        }
    }
}

void GameAttack::RoleRecoveryMagic(SessionMgr::SessionPointer smap, hf_double currentTime)
{
//    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    umap_recoveryMagic t_recoveryMagic = SessionMgr::Instance()->GetRecoveryMagic();
    for(_umap_recoveryMagic::iterator Magic_it = t_recoveryMagic->begin(); Magic_it != t_recoveryMagic->end();)
    {
        if(Magic_it->second.Timep >= currentTime) //时间没到
        {
            Magic_it++;
            continue;
        }
        Session* sess = &(*smap)[Magic_it->first];
        if(sess == NULL) //玩家退出游戏
        {
            _umap_recoveryMagic::iterator _Magic_it = Magic_it;
            Magic_it++;
            SessionMgr::Instance()->RecoveryHPDelete(_Magic_it->first);
            continue;
        }
        STR_RoleInfo* t_roleInfo = &(*smap)[Magic_it->first].m_roleInfo;

        if(t_roleInfo->HP == 0) //玩家已经死亡
        {
            _umap_recoveryMagic::iterator _Magic_it = Magic_it;
            Magic_it++;
            SessionMgr::Instance()->RecoveryHPDelete(_Magic_it->first);
            continue;
        }

        if(t_roleInfo->Magic == t_roleInfo->MaxMagic)
        {
            Magic_it->second.Count -= 1;
            if(Magic_it->second.Count >= 1)
            {
                Magic_it->second.Timep = currentTime + 1;
                Magic_it++;
            }
            else
            {
                _umap_recoveryMagic::iterator _Magic_it = Magic_it;
                Magic_it++;
                SessionMgr::Instance()->RecoveryHPDelete(_Magic_it->first);
            }
            continue;
        }


        if(t_roleInfo->Magic + Magic_it->second.Magic < t_roleInfo->MaxMagic)
            t_roleInfo->Magic += Magic_it->second.Magic;
        else
            t_roleInfo->Magic = t_roleInfo->MaxMagic;

        hf_uint32 roleid = (*smap)[Magic_it->first].m_roleid;
        STR_RoleAttribute t_roleAttr(roleid, t_roleInfo->HP, t_roleInfo->Magic);
        Magic_it->first->Write_all(&t_roleAttr, sizeof(STR_RoleAttribute));
        sess->SendHPToViewRole(&t_roleAttr);

        Magic_it->second.Count -= 1;
        if(Magic_it->second.Count >= 1)
        {
            Magic_it->second.Timep = currentTime + 1;
            Magic_it++;
        }
        else
        {
            _umap_recoveryMagic::iterator _Magic_it = Magic_it;
            Magic_it++;
            SessionMgr::Instance()->RecoveryHPDelete(_Magic_it->first);
        }
    }
}

//角色延时恢复血量，魔法值
void GameAttack::RoleRecoveryHPMagic(SessionMgr::SessionPointer smap, hf_double currentTime)
{
//    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    umap_recoveryHPMagic t_recoveryHPMagic = SessionMgr::Instance()->GetRecoveryHPMagic();
    for(_umap_recoveryHPMagic::iterator HPMagic_it = t_recoveryHPMagic->begin(); HPMagic_it != t_recoveryHPMagic->end();)
    {
        if(HPMagic_it->second.Timep >= currentTime) //时间没到
        {
            HPMagic_it++;
            continue;
        }
        Session* sess = &(*smap)[HPMagic_it->first];
        if(sess == NULL) //玩家退出游戏
        {
            _umap_recoveryHPMagic::iterator _HPMagic_it = HPMagic_it;
            HPMagic_it++;
            SessionMgr::Instance()->RecoveryHPDelete(_HPMagic_it->first);
            continue;
        }
        STR_RoleInfo* t_roleInfo = &(*smap)[HPMagic_it->first].m_roleInfo;

        if(t_roleInfo->HP == 0) //玩家已经死亡
        {
            _umap_recoveryHPMagic::iterator _HPMagic_it = HPMagic_it;
            HPMagic_it++;
            SessionMgr::Instance()->RecoveryHPDelete(_HPMagic_it->first);
            continue;
        }

        if(t_roleInfo->HP == t_roleInfo->MaxHP && t_roleInfo->Magic == t_roleInfo->MaxMagic) //血量和魔法值都是满的，不可以使用
        {
            HPMagic_it->second.Count -= 1;
            if(HPMagic_it->second.Count >= 1)
            {
                HPMagic_it->second.Timep = currentTime + 1;
                HPMagic_it++;
            }
            else
            {
                _umap_recoveryHPMagic::iterator _HPMagic_it = HPMagic_it;
                HPMagic_it++;
                SessionMgr::Instance()->RecoveryHPDelete(_HPMagic_it->first);
            }
            continue;
        }

        if(t_roleInfo->HP < t_roleInfo->MaxHP) //血量不满
        {
            if(t_roleInfo->HP + HPMagic_it->second.HP < t_roleInfo->MaxHP)
                t_roleInfo->HP += HPMagic_it->second.HP;
            else
                t_roleInfo->HP = t_roleInfo->MaxHP;
        }
        if(t_roleInfo->Magic < t_roleInfo->MaxMagic) //魔法值不满
        {
            if(t_roleInfo->Magic + HPMagic_it->second.Magic < t_roleInfo->MaxMagic)
                t_roleInfo->Magic += HPMagic_it->second.Magic;
            else
                t_roleInfo->Magic = t_roleInfo->MaxMagic;
        }

        hf_uint32 roleid = (*smap)[HPMagic_it->first].m_roleid;
        STR_RoleAttribute t_roleAttr(roleid, t_roleInfo->HP, t_roleInfo->Magic);
        HPMagic_it->first->Write_all(&t_roleAttr, sizeof(STR_RoleAttribute));
        sess->SendHPToViewRole(&t_roleAttr);

        HPMagic_it->second.Count -= 1;
        if(HPMagic_it->second.Count >= 1)
        {
            HPMagic_it->second.Timep = currentTime + 1;
            HPMagic_it++;
        }
        else
        {
            _umap_recoveryHPMagic::iterator _HPMagic_it = HPMagic_it;
            HPMagic_it++;
            SessionMgr::Instance()->RecoveryHPDelete(_HPMagic_it->first);
        }
    }
}

//查询所有技能信息
void GameAttack::QuerySkillInfo()
{
    DiskDBManager *db = Server::GetInstance()->getDiskDB();

    if ( db->GetSkillInfo(m_skillInfo) < 0 )
    {
        Logger::GetLogger()->Error("Query TaskDialogue error");
        return;
    }
}

//发送玩家可以使用的技能
void GameAttack::SendPlayerSkill(TCPConnection::Pointer conn)
{
    hf_char buff[1024] = { 0 };
    STR_PlayerSkill t_skill;
    STR_PackHead t_packHead;
    t_packHead.Flag = FLAG_CanUsedSkill;
    hf_uint32 i = 0;
    for(umap_skillInfo::iterator it = m_skillInfo->begin(); it != m_skillInfo->end(); it++)
    {
        t_skill.SkillID = it->second.SkillID;
        t_skill.CoolTime = it->second.CoolTime;
        t_skill.CastingTime = it->second.CastingTime;
        t_skill.LeadTime = it->second.LeadTime;
        memcpy(buff + sizeof(STR_PackHead) + sizeof(STR_PlayerSkill)*i, &t_skill, sizeof(STR_PlayerSkill));
        i++;
        if(i == (1024 - sizeof(STR_PackHead))/sizeof(STR_PlayerSkill) + 1)
        {
            t_packHead.Len = sizeof(STR_PlayerSkill) * i;
            memcpy(buff, &t_packHead, sizeof(STR_PackHead));
            conn->Write_all(buff, t_packHead.Len + sizeof(STR_PackHead));
            i = 0;
        }
    }
    if(i != 0)
    {
        t_packHead.Len = sizeof(STR_PlayerSkill) * i;
        memcpy(buff, &t_packHead, sizeof(STR_PackHead));
        conn->Write_all(buff, t_packHead.Len + sizeof(STR_PackHead));
    }
}

//判断技能是否过了冷却时间
hf_uint8 GameAttack::SkillCoolTime(TCPConnection::Pointer conn, hf_double timep, hf_uint32 skillID)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    hf_double t_skillUseTime = (*smap)[conn].m_skillUseTime;
    umap_skillTime t_skillTime = (*smap)[conn].m_skillTime;

    _umap_skillTime::iterator iter = t_skillTime->find(skillID);
    if(iter != t_skillTime->end()) //这个技能使用过
    {
        if(t_skillUseTime > timep || (*smap)[conn].m_publicCoolTime > timep || iter->second > timep)
            return 0;
    }
    else
    {       
        if(t_skillUseTime > timep || (*smap)[conn].m_publicCoolTime > timep)//没过技能冷却时间或没过公共冷却时间
            return 0;
    }
    return 1;
}

//以自己为圆心
void GameAttack::AimItselfCircle(TCPConnection::Pointer conn, STR_PackSkillInfo* skillInfo, hf_double timep)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    Server* srv = Server::GetInstance();
//    umap_monsterBasicInfo t_viewMonster = (*smap)[conn].m_viewMonster;
//    //取攻击者角色的属性信息
//    STR_RoleInfo* t_AttacketInfo = &((*smap)[conn].m_roleInfo);
//    umap_monsterAttackInfo* t_monsterAttack = srv->GetMonster()->GetMonsterAttack();

//    STR_PackPlayerPosition* t_pos = &(*smap)[conn].m_position;
//    STR_PackSkillResult t_skillResult;
//    STR_PackDamageData t_damageData;

//    (*smap)[conn].m_publicCoolTime = timep + PUBLIC_COOLTIME; //保存玩家使用技能的公共冷却时间
//    (*smap)[conn].m_skillUseTime = timep + skillInfo->CoolTime + skillInfo->CastingTime;  //保存技能的冷却时间
//    t_skillResult.result = SKILL_SUCCESS;
//    conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));

//    if(skillInfo->CastingTime > 0)   //延时类技能
//    {
//        RoleAttackAim t_attackAim;
//        t_attackAim.AimID = 0;
//        t_attackAim.HurtTime = timep + skillInfo->CastingTime;
//        t_attackAim.SkillID = skillInfo->SkillID;

//        (*m_attackMonster)[(*smap)[conn].m_roleid] = t_attackAim;
//        (*smap)[conn].m_skillUseTime = t_attackAim.HurtTime + skillInfo->CoolTime;
//        return;
//    }
//    STR_PackSkillAimEffect t_skillEffect(t_damageData.AimID,skillInfo->SkillID,t_damageData.AttackID);
//    conn->Write_all(&t_skillEffect, sizeof(STR_PackSkillAimEffect));  //发送施法效果

//    for(_umap_monsterBasicInfo::iterator it = t_viewMonster->begin(); it != t_viewMonster->end(); it++)
//    {
//         STR_MonsterAttackInfo* t_monsterAttackInfo = &(*t_monsterAttack)[it->first];

//         hf_float dx = it->second.Current_x - t_pos->Pos_x;
//         hf_float dy = it->second.Current_y - t_pos->Pos_y;
//         hf_float dz = it->second.Current_z - t_pos->Pos_z;

//         if( (dx*dx + dy*dy + dz*dz) < skillInfo->NearlyDistance * skillInfo->NearlyDistance ||
//                 (dx*dx + dy*dy + dz*dz) > skillInfo->FarDistance * skillInfo->FarDistance) //不在攻击范围
//         {
//             t_skillResult.result = NOT_ATTACKVIEW;
//             conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
//             return;
//         }
//        if(skillInfo->CastingTime == 0)//无延时类技能
//        {
//            t_damageData.Damage = CalMonsterDamage(skillInfo, t_AttacketInfo, t_monsterAttackInfo, &t_damageData.TypeID);
//            DamageDealWith(conn, &t_damageData, it->first); //发送计算伤害
//        }
//    }
}


//怪物为目标
void GameAttack::AimMonster(TCPConnection::Pointer conn, STR_PackSkillInfo* skillInfo, double timep, hf_uint32 AimID)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    STR_PackPlayerPosition* t_pos = &(*smap)[conn].m_position;
    STR_PackSkillResult t_skillResult;
    t_skillResult.SkillID = skillInfo->SkillID;
    STR_PackDamageData t_damageData;

    _umap_playerViewMonster::iterator it = (*smap)[conn].m_viewMonster->find(AimID); //查找可范围内是否有这个怪物
    if(it != (*smap)[conn].m_viewMonster->end())
    {
        t_damageData.AimID = AimID;
        t_damageData.AttackID = (*smap)[conn].m_roleid;

        umap_monsterInfo u_monsterInfo = Server::GetInstance()->GetMonster()->GetMonsterBasic();
        STR_MonsterInfo* t_monsterInfo = &(*u_monsterInfo)[AimID];

//        hf_float dx = t_monsterInfo->monster.Current_x - t_pos->Pos_x;
//        hf_float dy = t_monsterInfo->monster.Current_y - t_pos->Pos_y;
//        hf_float dz = t_monsterInfo->monster.Current_z - t_pos->Pos_z;


        STR_PosDis t_posDis;
        hf_uint8 res = Server::GetInstance()->GetMonster()->JudgeSkillDisAndDirect(t_pos, t_monsterInfo, timep, &t_posDis, skillInfo);
    //    return;
        //判断是否在攻击范围内
        if(res == 1)
        {
            t_skillResult.result = NOT_ATTACKVIEW;
            conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
            return;
        }

        else if(res == 2) //判断方向是否可攻击
        {
            t_skillResult.result = OPPOSITE_DIRECT;
            conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
            return;
        }


//        hf_float dis = sqrt(dx*dx + dy*dy + dz*dz);
//        if( dis < skillInfo->NearlyDistance || dis > skillInfo->FarDistance)    //不在攻击范围
//        {
//            t_skillResult.result = NOT_ATTACKVIEW;
//            conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
//            return;
//        }
//        if(dx*cos(t_pos->Direct) + dz*sin(t_pos->Direct) < 0)  //不在攻击角度
//        {
//            t_skillResult.result = OPPOSITE_DIRECT;
//            conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
//            return;
//        }

        (*smap)[conn].m_publicCoolTime = timep + PUBLIC_COOLTIME; //保存玩家使用技能的公共冷却时间
        (*smap)[conn].m_skillUseTime = timep + skillInfo->CoolTime + skillInfo->CastingTime;  //保存技能的冷却时间

        //取攻击者角色的属性信息
        STR_RoleInfo* t_AttacketInfo = &((*smap)[conn].m_roleInfo);
        umap_monsterAttackInfo* t_monsterAttack = Server::GetInstance()->GetMonster()->GetMonsterAttack();
        STR_MonsterAttackInfo* t_monsterAttackInfo = &(*t_monsterAttack)[t_monsterInfo->monster.MonsterTypeID];

        if(skillInfo->CastingTime == 0)//无延时类技能
        {
            hf_float t_probHit = t_AttacketInfo->Hit_Rate*1;
            if(t_monsterInfo->monsterStatus) //怪物处于返回中
                t_probHit = 0;
            if(t_probHit*100 < rand()%100) //未命中
            {
                t_damageData.TypeID = PhyAttackSkillID;
                t_damageData.Flag = NOT_HIT;
                conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
                return;
            }

            t_skillResult.result = SKILL_SUCCESS;
            conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
            t_damageData.Damage = CalMonsterDamage(t_monsterInfo->monster.Level, skillInfo, t_AttacketInfo, t_monsterAttackInfo, &t_damageData.TypeID);

            if(t_AttacketInfo->Crit_Rate*100 >= rand()%100)//暴击
            {
                t_damageData.Flag = CRIT_HIT;
                t_damageData.Damage *= 1.5;
            }
            else //未暴击
            {
                t_damageData.Flag = HIT;
            }

            cout << "damage:" << t_damageData.Damage << endl;
            STR_PackSkillAimEffect t_skillEffect(t_damageData.AimID,skillInfo->SkillID,t_damageData.AttackID);
            conn->Write_all(&t_skillEffect, sizeof(STR_PackSkillAimEffect));  //发送施法效果
            Server::GetInstance()->GetMonster()->SendSkillEffectToMonsterViewRole(&t_skillEffect);

//            STR_PosDis posDis(dis, 0 - dx, 0 - dz);
            DamageDealWith(conn, &t_damageData, t_monsterInfo, &t_posDis); //发送计算伤害
        }
        else   //延时类技能
        {
            RoleAttackAim t_attackAim;
            t_attackAim.AimID = AimID;
            t_attackAim.HurtTime = (hf_uint32)timep + skillInfo->CastingTime;
            t_attackAim.SkillID = skillInfo->SkillID;

            (*m_attackMonster)[(*smap)[conn].m_roleid] = t_attackAim;
            (*smap)[conn].m_skillUseTime = t_attackAim.HurtTime + skillInfo->CoolTime;
        }
    }
}

void GameAttack::AimRole(TCPConnection::Pointer conn, STR_PackSkillInfo* skillInfo, hf_double timep, hf_uint32 AimID)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();

    umap_roleSock t_viewRole = (*smap)[conn].m_viewRole;
    _umap_roleSock::iterator it = t_viewRole->find(AimID);
    if(it == t_viewRole->end()) //不在可视范围
    {
        return;
    }

    STR_RoleInfo* t_AimInfo = &((*smap)[it->second].m_roleInfo);
    if(t_AimInfo->HP == 0)
    {
        return;
    }
    STR_RoleInfo* t_AttacketInfo = &((*smap)[conn].m_roleInfo);

    STR_PackDamageData t_damageData;
    t_damageData.AimID = AimID;
    t_damageData.AttackID = (*smap)[conn].m_roleid;

    STR_PackSkillResult t_skillResult;
    t_skillResult.SkillID = skillInfo->SkillID;
    STR_PackPlayerPosition* t_AttacketPos = &(*smap)[conn].m_position;
    STR_PackPlayerPosition* t_AimPos = &(*smap)[it->second].m_position;

    hf_float dx = t_AimPos->Pos_x - t_AttacketPos->Pos_x;
    hf_float dy = t_AimPos->Pos_y - t_AttacketPos->Pos_y;
    hf_float dz = t_AimPos->Pos_z - t_AttacketPos->Pos_z;
    //判断是否在攻击范围内
    if(dx*dx + dy*dy + dz*dz > PlayerAttackView*PlayerAttackView)
    {
        t_skillResult.result = NOT_ATTACKVIEW;
        conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
        return;
    }

    //判断方向是否可攻击
    if(dx*cos(t_AttacketPos->Direct) + dz*sin(t_AttacketPos->Direct) < 0)
    {
        t_skillResult.result = OPPOSITE_DIRECT;
        conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
        return;
    }

    (*smap)[conn].m_publicCoolTime = timep + PUBLIC_COOLTIME; //保存玩家使用技能的公共冷却时间
    (*smap)[conn].m_skillUseTime = timep + skillInfo->CoolTime + skillInfo->CastingTime;  //保存技能的冷却时间


    if(skillInfo->CastingTime == 0)//无延时类技能
    {
        hf_float t_probHit = t_AttacketInfo->Hit_Rate*1;
        if(t_probHit*100 < rand()%100) //未命中
        {
            t_damageData.TypeID = PhyAttackSkillID;
            t_damageData.Flag = NOT_HIT;
            conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
            return;
        }

        if(t_AimInfo->Dodge_Rate*100 >= rand()%100) //闪避
        {
            t_damageData.TypeID = PhyAttackSkillID;
            t_damageData.Flag = Dodge;
            conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
            it->second->Write_all(&t_damageData, sizeof(STR_PackDamageData));
            return;
        }
        t_skillResult.result = SKILL_SUCCESS;
        conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));

        t_damageData.Damage = CalRoleDamage((*smap)[it->second].m_roleExp.Level, skillInfo, t_AttacketInfo, t_AimInfo, &t_damageData.TypeID);

        if(t_AttacketInfo->Crit_Rate*100 >= rand()%100)//暴击
        {
            t_damageData.Flag = CRIT_HIT;
            t_damageData.Damage *= 1.5;
        }
        else //未暴击
        {
            t_damageData.Flag = HIT;
        }

        if(t_AimInfo->Resist_Rate*100 >= rand()%100) //抵挡
        {
            t_damageData.Flag = RESIST;
        }


        cout << "damage:" << t_damageData.Damage << endl;
        STR_PackSkillAimEffect t_skillEffect(t_damageData.AimID,skillInfo->SkillID,t_damageData.AttackID);
        Logger::GetLogger()->Debug("aimdid:%u,roleid:%u,skillid:%u",t_skillEffect.AimID,t_skillEffect.RoleID,t_skillEffect.SkillID);

        it->second->Write_all(&t_skillEffect, sizeof(STR_PackSkillAimEffect));  //发送施法效果
        ((*smap)[it->second]).SendSkillEffectToViewRole(&t_skillEffect);

        conn->Write_all(&t_damageData, sizeof(STR_PackDamageData));
        it->second->Write_all(&t_damageData, sizeof(STR_PackDamageData));
        //发送玩家血量
        if(t_AimInfo->HP > t_damageData.Damage)
        {
            t_AimInfo->HP -= t_damageData.Damage;
        }
        else
        {
            t_AimInfo->HP = 0;
        }

        //给被攻击者可视范围内的玩家发送血量信息
        hf_uint32 roleid = (*smap)[it->second].m_roleid;
        STR_RoleAttribute t_roleAttr(roleid, t_AimInfo->HP);
        printf("RoleID:%d,HP%d \n",t_roleAttr.RoleID,t_roleAttr.HP);

        it->second->Write_all(&t_roleAttr, sizeof(STR_RoleAttribute));
        ((*smap)[it->second]).SendHPToViewRole(&t_roleAttr);
    }
    else   //延时类技能
    {
//        RoleAttackAim t_attackAim;
//        t_attackAim.AimID = AimID;
//        t_attackAim.HurtTime = (hf_uint32)timep + skillInfo->CastingTime;
//        t_attackAim.SkillID = skillInfo->SkillID;

//        (*m_attackMonster)[(*smap)[conn].m_roleid] = t_attackAim;
//        (*smap)[conn].m_skillUseTime = t_attackAim.HurtTime + skillInfo->CoolTime;
    }
}


//怪物为圆心
void GameAttack::AimMonsterCircle(TCPConnection::Pointer conn, STR_PackSkillInfo* skillInfo, double timep, hf_uint32 AimID)
{
    SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
    umap_playerViewMonster t_viewMonster = (*smap)[conn].m_viewMonster;
    STR_PackPlayerPosition* t_pos = &(*smap)[conn].m_position;
    STR_PackSkillResult t_skillResult;
    STR_PackDamageData t_damageData;

    _umap_playerViewMonster::iterator it = t_viewMonster->find(AimID); //查找可范围内是否有这个怪物
    if(it != (*smap)[conn].m_viewMonster->end())
    {
        umap_monsterInfo u_monsterInfo = Server::GetInstance()->GetMonster()->GetMonsterBasic();

        STR_MonsterInfo* t_monsterInfo = &(*u_monsterInfo)[AimID];
        hf_float dx = t_monsterInfo->monster.Current_x - t_pos->Pos_x;
        hf_float dy = t_monsterInfo->monster.Current_y - t_pos->Pos_y;
        hf_float dz = t_monsterInfo->monster.Current_z - t_pos->Pos_z;

        if( (dx*dx + dy*dy + dz*dz) < skillInfo->NearlyDistance * skillInfo->NearlyDistance ||
                (dx*dx + dy*dy + dz*dz) > skillInfo->FarDistance * skillInfo->FarDistance)    //不在攻击范围
        {
            t_skillResult.result = NOT_ATTACKVIEW;
            conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
            return;
        }
        if(dx*cos(t_pos->Direct) + dz*sin(t_pos->Direct) < 0)  //不在攻击角度
        {
            t_skillResult.result = OPPOSITE_DIRECT;
            conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));
            return;
        }

        (*smap)[conn].m_publicCoolTime = timep + PUBLIC_COOLTIME; //保存玩家使用技能的公共冷却时间
        (*smap)[conn].m_skillUseTime = timep + skillInfo->CoolTime + skillInfo->CastingTime;  //保存技能的冷却时间
        t_skillResult.result = SKILL_SUCCESS;
        conn->Write_all(&t_skillResult, sizeof(STR_PackSkillResult));  //发送施法结果

//        //取攻击者角色的属性信息
        STR_RoleInfo* t_roleInfo = &((*smap)[conn].m_roleInfo);
        umap_monsterAttackInfo* t_monsterAttack = Server::GetInstance()->GetMonster()->GetMonsterAttack();

        if(skillInfo->CastingTime > 0)
        {
            RoleAttackAim t_attackAim;
            t_attackAim.AimID = AimID;
            t_attackAim.HurtTime = timep + skillInfo->CastingTime;
            t_attackAim.SkillID = skillInfo->SkillID;

            (*m_attackMonster)[(*smap)[conn].m_roleid] = t_attackAim;
            (*smap)[conn].m_skillUseTime = t_attackAim.HurtTime + skillInfo->CoolTime;
            return;
        }
        STR_PackSkillAimEffect t_skillEffect(t_damageData.AimID,skillInfo->SkillID,t_damageData.AttackID);
//        conn->Write_all(&t_skillEffect, sizeof(STR_PackSkillAimEffect));  //发送施法效果
        Server::GetInstance()->GetMonster()->SendSkillEffectToMonsterViewRole(&t_skillEffect);

        for(_umap_playerViewMonster::iterator iter = t_viewMonster->begin(); iter != t_viewMonster->end(); iter++)
        {

            if(iter->first == t_monsterInfo->monster.MonsterID)
            {
                continue;
            }            
            STR_MonsterInfo* t_monster = &(*u_monsterInfo)[iter->first];
            STR_MonsterAttackInfo* t_monsterAttackInfo = &(*t_monsterAttack)[t_monster->monster.MonsterTypeID];
            hf_float dx = t_monsterInfo->monster.Current_x - t_monster->monster.Current_x;
            hf_float dy = t_monsterInfo->monster.Current_y - t_monster->monster.Current_x;
            hf_float dz = t_monsterInfo->monster.Current_z - t_monster->monster.Current_x;
            hf_float dis = sqrt(dx*dx + dy*dy + dz*dz);
            if(dis >= skillInfo->NearlyDistance && dis <= skillInfo->FarDistance)
            {
              t_damageData.Damage = CalMonsterDamage(t_monsterInfo->monster.Level, skillInfo, t_roleInfo, t_monsterAttackInfo, &t_damageData.TypeID);
              STR_PosDis posDis(dis, 0 - dx, 0 - dz);
              DamageDealWith(conn, &t_damageData, t_monster, &posDis); //发送计算伤害
            }
        }
    }
}


//清除超过时间的掉落物品
void GameAttack::DeleteOverTimeGoods()
{

    time_t t_time;
    time(&t_time);
    hf_uint32 timep = (hf_uint32)t_time;
    umap_lootGoods lootGoods = SessionMgr::Instance()->GetLootGoods();
    while(1)
    {
        for(_umap_lootGoods::iterator it = lootGoods->begin(); it != lootGoods->end();)
        {
            if(it->second.ContinueTime <= timep)
            {
                hf_uint32 t_uniqueID = it->second.UniqueID;
                it++;
                SessionMgr::Instance()->LootGoodsDelete(t_uniqueID);

//                _umap_lootGoods::iterator _it = it;
//                it++;
//                lootGoods->erase(_it);

            }
            else
            {
                it++;
            }         
        }
        sleep(1);
        timep++;
    }
//        SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();

//        for(SessionMgr::SessionMap::iterator it = smap->begin(); it != smap->end(); it++)
//        {
//            umap_lootPosition  t_lootPosition = it->second.m_lootPosition;
//            umap_lootGoods     t_lootGoods = it->second.m_lootGoods;
//            for(_umap_lootPosition::iterator pos_it = t_lootPosition->begin(); pos_it != t_lootPosition->end();)
//            {
//                if(timep >= pos_it->second.timep + pos_it->second.continueTime)
//                {
//                    t_loot.loot = pos_it->first;
//                    it->first->Write_all(&t_loot, sizeof(LootGoodsOverTime));

//                    _umap_lootGoods::iterator goods_it = t_lootGoods->find(pos_it->first);
//                    if(goods_it != t_lootGoods->end())
//                    {
//                        t_lootGoods->erase(goods_it);
//                    }
//                    _umap_lootPosition::iterator _pos_it = pos_it;
//                    pos_it++;
//                    t_lootPosition->erase(_pos_it);
//                }
//                else
//                {
//                    pos_it++;
//                    continue;
//                }
//            }
//        }
//        sleep(1);
//        timep++;
//    }
}


