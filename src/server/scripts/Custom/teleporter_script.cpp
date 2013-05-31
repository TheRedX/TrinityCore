/*
Teleporter Script for Trinity
Coded By Lhorath
Coding Inspired From Rochet2 Teleporter
*/

//   Link Types//
/*
{#,	#,	2,	"|cff4169E1 Non-Icon Links|r",	0,	0,	0,	0,	0,	0,	0,	0},
{#,	#,	2,	"|cff4169E1|TInterface\\icons\\Achievement_Leader_King_Varian_Wrynn:30|t Icon & Color Links|r",	0,	0,	0,	0,	0,	0,	0,	0},
{#,	#,	2,	"|TInterface\\icons\\Achievement_Leader_King_Varian_Wrynn:30|t Icon Links (NoColor)|t",	0,	0,	0,	0,	0,	0,	0,	0},
{#,	#,	2,	"Non-Formated Links Links",	0,	0,	0,	0,	0,	0,	0,	0},
*/

// Color List //
/*
ORANGE			"|cffFFA500"
DARKORANGE		"|cffFF8C00"
RED				"|cffFF0000"
LIGHTRED		"|cffD63931"
ROYALBLUE		"|cff4169E1"
LIGHTBLUE		"|cffADD8E6"
YELLOW			"|cffFFFF00"
GREEN			"|cff008000"
PURPLE			"|cffDA70D6"
WHITE			"|cffffffff"
SUBWHITE		"|cffbbbbbb"
*/
	
#include "ScriptPCH.h"

// Teleport data

struct Eric
{
	uint32 menu_id, next_menu_id;
	uint8 icon;
	std::string name;
	uint32 cost;
	uint8 level, faction; // 0 Both, 1 Horde, 2 Ally
	uint32 map;
	float x, y, z, o;
};
typedef struct Eric Rochet2;

//	{menu_id, next_menu_id, icon, name, cost, level, faction, map, x, y, z, o}
Rochet2 TeLe [] = // key = i & Key
{
	{1,	2,	2,	"|cffFFFF00|TInterface\\icons\\Achievement_Dungeon_Classicraider:30|t Level 1 Dungeons|r",	0,	0,	0,	0,	0,	0,	0,	0},
	{1,	3,	2,	"|cffFFFF00|TInterface\\icons\\Achievement_Dungeon_Icecrown_Frostmourne:30|t Level 2 Dungeons|r",	0,	0,	0,	0,	0,	0,	0,	0},
	{1,	4,	2,	"|cffFFFF00|TInterface\\icons\\Achievement_Boss_Galdarah:30|t Level 3 Dungeons|r",	0,	0,	0,	0,	0,	0,	0,	0},
	{1,	5,	2,	"|cffFFFF00|TInterface\\icons\\Achievement_Boss_Infinitecorruptor:30|t Level 4 Dungeons|r",	0,	0,	0,	0,	0,	0,	0,	0},
	{1,	6,	2,	"|cffFFFF00|TInterface\\icons\\Achievement_Boss_Kiljaedan:30|t Level 5 Dungeons[Locked]|r",	0,	0,	0,	0,	0,	0,	0,	0},
	{1,	7,	2,	"|cffFFFF00|TInterface\\icons\\Temp:30|t Starting Zone|r",	0,	0,	0,	0,	0,	0,	0,	0},
	
	{2,	1,	7,	"|cffffffffBack..|r",	0,	0,	0,	0,	0,	0,	0,	0},

	{3,	1,	7,	"|cffffffffBack..|r",	0,	0,	0,	0,	0,	0,	0,	0},
	
	{4,	1,	7,	"|cffffffffBack..|r",	0,	0,	0,	0,	0,	0,	0,	0},
	
	{5,	1,	7,	"|cffffffffBack..|r",	0,	0,	0,	0,	0,	0,	0,	0},
	
	{6,	1,	7,	"|cffffffffBack..|r",	0,	0,	0,	0,	0,	0,	0,	0},
	
	{7,	0,	2,	"Starting Zone",	0,	60,	0,	530,	-250.550f,	1064.48f,	54.3103f,	4.72309f},
	{7,	1,	7,	"|cffffffffBack..|r",	0,	0,	0,	0,	0,	0,	0,	0},
	
//	{10,	0,	2,	"Teleport",	0,	0,	0,	map,	xf,	yf,	zf,	of},
//	{10,	1,	7,	"Back..",	0,	0,	0,	0,	0,	0,	0,	0},
};

// TextIDs from npc_text
enum eEnums
{
	TEXT_MAIN_H		=	400000,			//Horde main menu text
	TEXT_MAIN_A		=	400001,			//Alliance main menu text
	TEXT_DUNGEON	=	400002,			//Dungeon teleport menu texts
	TEXT_RAID		=	400003,			//Raid teleport menu text
	TEXT_AREA		=	400004,			//Area teleport location menu texts
	
	TELEPORT_COUNT	=	sizeof TeLe/sizeof(*TeLe),
};

#define ARE_YOU_SURE	"|cffffffffGetting a Move on to|r "
#define ERROR_COMBAT	"|cffff0000You are in combat !|r"

bool Custom_FactCheck (uint32 Fact, unsigned char Key)
{
	bool Show = false;
	switch (TeLe[Key].faction)
	{
	case 0:
		Show = true;
		break;
	case 1:
		if (Fact == HORDE)
			Show = true;
		break;
	case 2:
		if (Fact == ALLIANCE)
			Show = true;
		break;
	}
	return (Show);
}

uint32 Custom_GetText (unsigned int menu, Player* pPlayer)
{
	uint32 TEXT = TEXT_AREA;
	switch (menu)
	{
	case 0:
		switch (pPlayer->GetTeam())
		{
		case ALLIANCE:
			TEXT = TEXT_MAIN_A;
			break;
		case HORDE:
			TEXT = TEXT_MAIN_H;
			break;
		}
	case 1:
	case 2:
	case 3:
		TEXT = TEXT_DUNGEON;
		break;
	case 4:
		TEXT = TEXT_RAID;
		break;
	}
	return (TEXT);
}

void Custom_GetMenu (Player* pPlayer, Creature* pCreature, uint32 Key)
{
	bool ENDMENU = false;
	for(uint32 i = 0; i < TELEPORT_COUNT; i++)
	{
		if (ENDMENU && TeLe[i].menu_id != Key)
			break;
		if (TeLe[i].menu_id == Key && pPlayer->getLevel() >= TeLe[i].level && Custom_FactCheck(pPlayer->GetTeam(), i))
		{
			if (TeLe[i].next_menu_id != 0)
				pPlayer->ADD_GOSSIP_ITEM_EXTENDED(TeLe[i].icon, TeLe[i].name, GOSSIP_SENDER_MAIN, i, "", TeLe[i].cost, false);
			else
				pPlayer->ADD_GOSSIP_ITEM_EXTENDED(TeLe[i].icon, TeLe[i].name, GOSSIP_SENDER_MAIN, i, ARE_YOU_SURE+TeLe[i].name, TeLe[i].cost, false);
			ENDMENU = true;
		}
	}
	pPlayer->PlayerTalkClass->SendGossipMenu(Custom_GetText(Key, pPlayer), pCreature->GetGUID());
}

class tele_script_mortal : public CreatureScript
{
	public:
	tele_script_mortal()
	: CreatureScript("tele_script_mortal")
	{
	}

	bool OnGossipHello(Player* pPlayer, Creature* pCreature)
	{
		Custom_GetMenu(pPlayer, pCreature, 1);
		return true;
	}

	bool OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
	{
		pPlayer->PlayerTalkClass->ClearMenus(); // clear the menu

		pPlayer->ModifyMoney(-1 * TeLe[uiAction].cost); // take cash
		uint32 Key = TeLe[uiAction].next_menu_id;
		if (Key == 0) // if no next_menu_id, then teleport to coords
		{
			if (!pPlayer->isInCombat())
			{
				pPlayer->CLOSE_GOSSIP_MENU();
				pPlayer->TeleportTo(TeLe[uiAction].map, TeLe[uiAction].x, TeLe[uiAction].y, TeLe[uiAction].z, TeLe[uiAction].o);
				return true;
			}
			pPlayer->GetSession()->SendAreaTriggerMessage(ERROR_COMBAT);
			Key = TeLe[uiAction].menu_id;
		}

		Custom_GetMenu(pPlayer, pCreature, Key);
		return true;
	}
};

void AddSC_tele_script_mortal()
{
    new tele_script_mortal();
}
