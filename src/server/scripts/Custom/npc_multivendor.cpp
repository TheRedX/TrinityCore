/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "GameEventMgr.h"
#include "Player.h"
#include "WorldSession.h"

class Vendor
{
private:
    int m_GUID;
    GossipOptionIcon m_Icon;
    std::string m_Message;
public:
    Vendor() {}
    Vendor(int guid, GossipOptionIcon icon, std::string message)
    {
        m_GUID = guid;
        m_Icon = icon;
        m_Message = message;
    }
    
    Vendor(int guid, GossipOptionIcon icon, std::string message, VendorItemData* item_list)
    {
        m_GUID = guid;
        m_Icon = icon;
        m_Message = message;
        for(int i = 0; i < item_list->GetItemCount(); i++)
            items.AddItem(item_list->m_items[i]->item, 0, 0, item_list->m_items[i]->ExtendedCost);
    }

    VendorItemData items;

    int getGuid() { return m_GUID; }
    void setGuid(int guid) { m_GUID = guid; }
    GossipOptionIcon getIcon() { return m_Icon; }
    void setIcon(GossipOptionIcon icon) { m_Icon = icon; }
    std::string getMessage() { return m_Message; }
    void setMessage(std::string message) { m_Message = message; }
};

class ItemList
{
public:
    ItemList() { /* Called while looping through vendors to find ours */ }

    ItemList(Vendor* myVendor)
    {
        m_vendor = Vendor(myVendor->getGuid(), myVendor->getIcon(), myVendor->getMessage());
    }

    ItemList(Vendor* myVendor, VendorItemData* item_list)
    {
        m_vendor = Vendor(myVendor->getGuid(), myVendor->getIcon(), myVendor->getMessage());
        for(int i = 0; i < item_list->GetItemCount(); i++)
            m_vendor.items.AddItem(item_list->m_items[i]->item, 0, 0, item_list->m_items[i]->ExtendedCost);
    }

    void AddItem(uint32 item_id, uint32 extended_cost = 0) { m_vendor.items.AddItem(item_id, 0, 0, extended_cost); }
    void RemoveItem(uint32 item_id) { m_vendor.items.RemoveItem(item_id); }
    uint8 GetItemCount() { return m_vendor.items.GetItemCount(); }
    Vendor GetVendor() { return m_vendor; }
private:
    Vendor m_vendor;
};

class SmsgListInventory
{
private:
    uint32 m_opcode;
    uint64 m_vendorGuid;
    uint8  m_itemCount;
    uint8  m_status;
    WorldSession* m_session;
    VendorItemData m_vendorItemData;
    VendorItemData m_completeVendorItemData;
    std::vector<ItemList> m_vendors;

    struct packetItem
    {
        uint32 m_slot;
        uint32 m_item;
        uint32 m_displayId;
        int32  m_inStock;
        uint32 m_price;
        uint32 m_durability;
        uint32 m_buyCount;
        uint32 m_extendedCost;
    };

    bool SkipItem(const ItemTemplate* item_template, Player* player, int slot)
    {
        /* Calculate relative slot position */
        if(m_vendors[0].GetItemCount() > 0)
        {
            if( (slot + 1) > (m_vendors[0].GetItemCount()))
            {
                int totalItemsChecked = 0;
                /* For each vendor */
                for(int i = 0; i < m_vendors.size(); i++)
                {
                    if((totalItemsChecked + m_vendors[i].GetItemCount()) < (slot + 1))
                    {
                        totalItemsChecked += m_vendors[i].GetItemCount();
                        continue;
                    }

                    slot -= totalItemsChecked;
                    break;
                }
            }
        }

        /* Account for duplicate items across vendors */
        if(VendorItem* vendor_item = m_vendorItemData.GetItem(slot))
        {
            if(vendor_item->item != item_template->ItemId)
                return true;
        }
        else
            return true; // the item doesn't even exist why would we send it

        /* GM's are exceptions */
        if(player->isGameMaster())
            return false;

        /* If the item is class specific and Bind on Pickup */
        if (!(item_template->AllowableClass & player->getClassMask()) && item_template->Bonding == BIND_WHEN_PICKED_UP)
            return true;
        
        /* If the item is faction specific and player is wrong faction */
        if ((item_template->Flags2 & ITEM_FLAGS_EXTRA_HORDE_ONLY && player->GetTeam() == ALLIANCE) || 
            (item_template->Flags2 == ITEM_FLAGS_EXTRA_ALLIANCE_ONLY && player->GetTeam() == HORDE))
            return true;

        /* Anything else */
        return false;
    }

public: SmsgListInventory(uint64 vendor_guid, WorldSession* player_session, VendorItemData* vendor_item_data, std::vector<ItemList> vendors)
        {
            m_opcode = SMSG_LIST_INVENTORY;
            m_vendorGuid = vendor_guid;
            m_itemCount = 0;
            for(int i = 0; i < vendors.size(); i++)
                m_itemCount += vendors[i].GetItemCount();

            m_status = 0;
            m_session = player_session;
            for(int i = 0; i < vendors.size(); i++)
                for(int i2 = 0; i2 < vendors[i].GetItemCount(); i2++)
                    m_completeVendorItemData.AddItem(vendors[i].GetVendor().items.m_items[i2]->item, 0, 0, vendors[i].GetVendor().items.m_items[i2]->ExtendedCost);

            for(int i = 0; i < vendor_item_data->GetItemCount(); i++)
                m_vendorItemData.AddItem(vendor_item_data->m_items[i]->item, 0, 0, vendor_item_data->m_items[i]->ExtendedCost);

            for(int i = 0; i < vendors.size(); i++)
                m_vendors.push_back(vendors[i]);
        }

        void Send(Creature* originalVendor, Player* player)
        {
            uint8 item_count = 0;
            std::vector<packetItem> items_to_send;
            ItemList item_list_to_send;
            bool haveUpdatedVendor = false;
            if(const VendorItemData* vendor_item_data = originalVendor->GetVendorItems())
                if(vendor_item_data->GetItemCount() > 0)
                    haveUpdatedVendor = true;

            for(int slot = 0; slot < m_itemCount; slot++)
                if(VendorItem const* item = m_completeVendorItemData.GetItem(slot))
                    if(ItemTemplate const* item_template = sObjectMgr->GetItemTemplate(item->item))
                    {
                        if(!haveUpdatedVendor)
                            sObjectMgr->AddVendorItem(originalVendor->GetEntry(), item->item, 0, 0, item->ExtendedCost, false);

                        if(SkipItem(item_template, player, slot))
                            continue;

                        item_list_to_send.AddItem(item->item, item->ExtendedCost);

                        item_count++;

                        int32 price = item->IsGoldRequired(item_template) ? uint32(item_template->BuyPrice) : 0;

                        packetItem packet;
                        packet.m_slot = slot + 1;
                        packet.m_item = item->item;
                        packet.m_displayId = item_template->DisplayInfoID;
                        packet.m_inStock = 0xFFFFFFFF;
                        packet.m_price = price;
                        packet.m_durability = item_template->MaxDurability;
                        packet.m_buyCount = item_template->BuyCount;
                        packet.m_extendedCost = item->ExtendedCost;
                        items_to_send.push_back(packet);
                    }

            WorldPacket packet(m_opcode, 8 + 1 + (m_itemCount > 0 ? m_itemCount * 8 * 4 : 1));
            packet << m_vendorGuid;
            packet << item_count;
            for(int i = 0; i < items_to_send.size(); i++)
            {
                packet << items_to_send[i].m_slot;
                packet << items_to_send[i].m_item;
                packet << items_to_send[i].m_displayId;
                packet << items_to_send[i].m_inStock;
                packet << items_to_send[i].m_price;
                packet << items_to_send[i].m_durability;
                packet << items_to_send[i].m_buyCount;
                packet << items_to_send[i].m_extendedCost;
            }
            m_session->SendPacket(&packet);
        }
};

class npc_multivendor : public CreatureScript
{
public:
    npc_multivendor() : CreatureScript("npc_multivendor"){ }

    std::vector<ItemList> GetVendorList()
    {
        std::vector<ItemList> itemlists;
        
        /* ONLY EDIT THINGS BELOW THIS COMMENT BLOCK
         * DO NOT EDIT ANYTHING ELSE
         *
         * Example vendor:

        Vendor vendor_1(700000, GOSSIP_ICON_VENDOR, "Look at all the weapons I have");
        ItemList items_1(&vendor_1);
        items_1.AddItem(18582); // Azzinoth
        items_1.AddItem(13262); // Ashbringer
        itemlists.push_back(items_1);
        
        * The first line creates a "Vendor" object named vendor_1 with:
        *     GUID 700000 (this must be different to all your other vendors)
        *     Displays the moneybag icon in the gossip list
        *     Displays the text "Look at all the weapons I have" in the gossip list
        * 
        * The second line creates an instance of the ItemList object named items1.
        *     We pass it a reference to the vendor we created before. The class does the rest.
        * 
        * The third and fourth lines are examples of adding items to our item list (named items_1).
        *     You can give an item an extended_cost by passing a second parameter to the function.
        *     This script does not support time-limited or respawning items
        * 
        * The final line is the most important. This line adds your vendor to the internal list of vendors that will be processed by the script.
        * 
        *
        * END OF EXPLANATION
        * I REPEAT ONCE MORE PLEASE DO NOT TOUCH ANYTHING ELSE IN THE FILE IT IS EXTREMELY DELICATE AND EXTREMELY COMPLEX
        */

        Vendor vendor_1(700000, GOSSIP_ICON_VENDOR, "Starting Gear - Death Knight");
        ItemList items_1(&vendor_1);
        items_1.AddItem(39617); // 
        items_1.AddItem(39618); //
        items_1.AddItem(39619); //
        items_1.AddItem(39620); //
        items_1.AddItem(39621); //
        //
        items_1.AddItem(39623); // 
        items_1.AddItem(39624); //
        items_1.AddItem(39625); //
        items_1.AddItem(39626); //
        items_1.AddItem(39627); //
        itemlists.push_back(items_1);

        Vendor vendor_2(700001, GOSSIP_ICON_VENDOR, "Starting Gear - Warrior");
        ItemList items_2(&vendor_2);
        items_2.AddItem(39605); // 
        items_2.AddItem(39606); // 
        items_2.AddItem(39607); // 
        items_2.AddItem(39608); //
        items_2.AddItem(39609); //
        //
        items_2.AddItem(39610); //
        items_2.AddItem(39611); // 
        items_2.AddItem(39612); //
        items_2.AddItem(39613); // 
        items_2.AddItem(39622); //
        itemlists.push_back(items_2);
        
        Vendor vendor_3(700002, GOSSIP_ICON_VENDOR, "Starting Gear - Paladin");
        ItemList items_3(&vendor_3);
        items_3.AddItem(39633); // 
        items_3.AddItem(39634); // 
        items_3.AddItem(39635); // 
        items_3.AddItem(39636); //
        items_3.AddItem(39637); //
        //
        items_3.AddItem(39638); //
        items_3.AddItem(39639); // 
        items_3.AddItem(39640); //
        items_3.AddItem(39641); // 
        items_3.AddItem(39642); //
        //
        items_3.AddItem(39628); //
        items_3.AddItem(39629); // 
        items_3.AddItem(39630); //
        items_3.AddItem(39631); // 
        items_3.AddItem(39632); //
        itemlists.push_back(items_3);
        
        Vendor vendor_4(700003, GOSSIP_ICON_VENDOR, "Starting Gear - Shaman");
        ItemList items_4(&vendor_4);
        items_4.AddItem(39597); // 
        items_4.AddItem(39601); // 
        items_4.AddItem(39602); // 
        items_4.AddItem(39603); //
        items_4.AddItem(39604); //
        //
        items_4.AddItem(39592); //
        items_4.AddItem(39593); // 
        items_4.AddItem(39594); //
        items_4.AddItem(39595); // 
        items_4.AddItem(39596); //
        //
        items_4.AddItem(39583); //
        items_4.AddItem(39588); // 
        items_4.AddItem(39589); //
        items_4.AddItem(39590); // 
        items_4.AddItem(39591); //
        itemlists.push_back(items_4);
        
        Vendor vendor_5(700004, GOSSIP_ICON_VENDOR, "Starting Gear - Hunter");
        ItemList items_5(&vendor_5);
        items_5.AddItem(39578); // 
        items_5.AddItem(39579); // 
        items_5.AddItem(39580); // 
        items_5.AddItem(39581); //
        items_5.AddItem(39582); //
        itemlists.push_back(items_5);
        
        Vendor vendor_6(700005, GOSSIP_ICON_VENDOR, "Starting Gear - Rogue");
        ItemList items_6(&vendor_6);
        items_6.AddItem(39558); // 
        items_6.AddItem(39560); // 
        items_6.AddItem(39561); // 
        items_6.AddItem(39564); //
        items_6.AddItem(39565); //
        itemlists.push_back(items_6);
        
        Vendor vendor_7(700006, GOSSIP_ICON_VENDOR, "Starting Gear - Druid");
        ItemList items_7(&vendor_7);
        items_7.AddItem(39553); // 
        items_7.AddItem(39554); // 
        items_7.AddItem(39555); // 
        items_7.AddItem(39556); //
        items_7.AddItem(39557); //
        //
        items_7.AddItem(39544); //
        items_7.AddItem(39545); // 
        items_7.AddItem(39546); //
        items_7.AddItem(39547); // 
        items_7.AddItem(39548); //
        //
        items_7.AddItem(39531); //
        items_7.AddItem(39538); // 
        items_7.AddItem(39539); //
        items_7.AddItem(39542); // 
        items_7.AddItem(39543); //
        itemlists.push_back(items_7);
        
        Vendor vendor_8(700007, GOSSIP_ICON_VENDOR, "Starting Gear - Priest");
        ItemList items_8(&vendor_8);
        items_8.AddItem(39521); // 
        items_8.AddItem(39523); // 
        items_8.AddItem(39528); // 
        items_8.AddItem(39529); //
        items_8.AddItem(39530); //
        //
        items_8.AddItem(39514); //
        items_8.AddItem(39515); // 
        items_8.AddItem(39517); //
        items_8.AddItem(39518); // 
        items_8.AddItem(39519); //
        itemlists.push_back(items_8);
        
        Vendor vendor_9(700008, GOSSIP_ICON_VENDOR, "Starting Gear - Warlock");
        ItemList items_9(&vendor_9);
        items_9.AddItem(39496); // 
        items_9.AddItem(39497); // 
        items_9.AddItem(39498); // 
        items_9.AddItem(39499); //
        items_9.AddItem(39500); //
        itemlists.push_back(items_9);
        
        Vendor vendor_10(700009, GOSSIP_ICON_VENDOR, "Starting Gear - Mage");
        ItemList items_10(&vendor_10);
        items_10.AddItem(39491); // 
        items_10.AddItem(39492); // 
        items_10.AddItem(39493); // 
        items_10.AddItem(39494); //
        items_10.AddItem(39495); //
        itemlists.push_back(items_10);

        /* DO NOT EDIT ANYTHING BELOW HERE EITHER
         * THIS IS THE END OF THE EDITABLE SECTION
         * ONLY EDIT THINGS ABOVE THIS COMMENT BLOCK UNLESS YOU _REALLY_ KNOW WHAT YOU'RE DOING
         *
         * Peace out
         * Evilfairy~ */

        return itemlists;
    }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        std::vector<ItemList> vendors = GetVendorList();

        /* DO NOT EDIT ANYTHING BELOW THIS LINE */
        for(int i = 0; i < vendors.size(); i++) // icon message sender guid
            player->ADD_GOSSIP_ITEM(vendors[i].GetVendor().getIcon(), vendors[i].GetVendor().getMessage(), GOSSIP_SENDER_MAIN, vendors[i].GetVendor().getGuid());

        player->TalkedToCreature(creature->GetEntry(), creature->GetGUID());
        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();
        player->CLOSE_GOSSIP_MENU();
        
        SendInventoryCustom(player, creature, action);
        
        return true;
    }

    void SendInventoryCustom(Player* player, Creature* vendor, int guid)
    {
        /* Remove Feign Death effects */
        if (player->HasUnitState(UNIT_STATE_DIED))
            player->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

        /* Stop NPC moving */
        if (vendor->isMoving())
            vendor->StopMoving();

        std::vector<ItemList> vendors = GetVendorList();
        ItemList myVendor;
        for(int i = 0; i < vendors.size(); i++)
        {
            if(vendors[i].GetVendor().getGuid() == guid)
            {
                myVendor = ItemList(&vendors[i].GetVendor(), &vendors[i].GetVendor().items);
                break;
            }
        }

        SmsgListInventory inventory_packet(vendor->GetGUID(), player->GetSession(), &myVendor.GetVendor().items, vendors);
        inventory_packet.Send(vendor, player);
    }
};

void AddSC_npc_multivendor()
{
    new npc_multivendor;
}