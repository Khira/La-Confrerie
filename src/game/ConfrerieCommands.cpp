/*
 * Copyright (C) 2009-2010 La Confrérie <http://la-confrerie.fr>
 *
 * Commandes créées spécialement pour le serveur de La Confrérie.
 * Pour toute autre utilisation, vous devez contactez l'auteur de
 * ces commandes.
 * Vous 'navez pas le droit de partager ou distribuer ce code.
 *
 */

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Opcodes.h"
#include "Chat.h"
#include "ObjectAccessor.h"
#include "Language.h"
#include "AccountMgr.h"
#include "SystemConfig.h"
#include "revision.h"
#include "revision_nr.h"
#include "Util.h"
#include "SpellAuras.h"
#include "SpellMgr.h"

enum PlayerSellerConfFlags
{
    PLAYER_SELLER_CONF_FLAG_WEAPONS     = 0x00000001,
    PLAYER_SELLER_CONF_FLAG_ARMOR       = 0x00000002,
    PLAYER_SELLER_CONF_FLAG_JEWELERY    = 0x00000004,
    PLAYER_SELLER_CONF_FLAG_OTHER       = 0x00000008,
    PLAYER_SELLER_CONF_FLAG_GUILD_SELL  = 0x00000010,
};

bool ChatHandler::HandleConfrerieMorphOnCommand(char* /*args*/)
{
    if(!m_session->GetPlayer()->isAlive())
    {
        SetSentErrorMessage(true);
        return false;
    }

    Player* character = m_session->GetPlayer();

    QueryResult *result = ConfrerieDatabase.PQuery("SELECT displayid_m, displayid_w, scale, speed, aura1, aura2, aura3, spell1, spell2, spell3 FROM player_race WHERE entry=(SELECT morph FROM player_race_relation WHERE guid='%u')", character->GetGUID());
    if (!result)
    {
        SendSysMessage(LANG_COMMAND_CONF_MORPH_RACE_NOT_POSSIBLE);
        SetSentErrorMessage(true);
        return false;
    }
    Field* fields = result->Fetch();
    float Scale = fields[2].GetFloat();
    float Speed = fields[3].GetFloat();

    for (int j = 0; j < 3; ++j)
    {
        if (!(character->HasSpell(fields[j + 7].GetUInt32())) && fields[j + 7].GetUInt32() != 0)
            character->learnSpell(fields[j + 7].GetUInt32(),false);

        if (fields[j + 4].GetUInt32() != 0)
        {
            uint32 spellID = fields[j + 4].GetUInt32();

            SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellID);
            if (!spellInfo)
                return false;

            SpellAuraHolder *holder = CreateSpellAuraHolder(spellInfo, character, character);

            for(uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                uint8 eff = spellInfo->Effect[i];
                if (eff>=TOTAL_SPELL_EFFECTS)
                    continue;
                if (IsAreaAuraEffect(eff)           || eff == SPELL_EFFECT_APPLY_AURA  || eff == SPELL_EFFECT_PERSISTENT_AREA_AURA)
                {
                    Aura *aur = CreateAura(spellInfo, SpellEffectIndex(i), NULL, holder, character);
                    holder->AddAura(aur, SpellEffectIndex(i));
                }
            }
            character->AddSpellAuraHolder(holder);
        }
    }

    character->UpdateSpeed(MOVE_RUN, true, Speed);
    character->UpdateSpeed(MOVE_FLIGHT, true, Speed);
    character->SetFloatValue(OBJECT_FIELD_SCALE_X, Scale);
    if (character->getGender() == GENDER_MALE)
        character->SetDisplayId(fields[0].GetUInt32());
    else
        character->SetDisplayId(fields[1].GetUInt32());

    delete result;
    return true;
}

bool ChatHandler::HandleConfrerieMorphOffCommand(char* /*args*/)
{
    if(!m_session->GetPlayer()->isAlive())
    {
        SetSentErrorMessage(true);
        return false;
    }

    Player* character = m_session->GetPlayer();

    QueryResult *resultRace = ConfrerieDatabase.PQuery("SELECT aura1, aura2, aura3, spell1, spell2, spell3 FROM player_race WHERE entry=(SELECT morph FROM player_race_relation WHERE guid='%u')", character->GetGUID());
    if (resultRace)
    {
        Field* fieldsRace = resultRace->Fetch();
        for (int i = 0; i < 3; ++i)
        {
            if(fieldsRace[i + 0].GetUInt32())
                character->RemoveAurasDueToSpell(fieldsRace[i + 0].GetUInt32());

            if (character->HasSpell(fieldsRace[i + 3].GetUInt32()) && fieldsRace[i + 3].GetUInt32()!=0)
                character->removeSpell(fieldsRace[i + 3].GetUInt32(),false,false);
        }
    }
    delete resultRace;

    character->DeMorph();
    character->UpdateSpeed(MOVE_RUN, true, 1.0f);
    character->UpdateSpeed(MOVE_FLIGHT, true, 1.0f);
    character->SetFloatValue(OBJECT_FIELD_SCALE_X, 1.0f);
    m_session->SendNotification(LANG_COMMAND_CONF_MORPH_RACE_HUMAN);
    return true;
}

bool ChatHandler::HandleConfrerieMorphSetCommand(char* args)
{
    if(!*args)
        return false;

    if(!m_session->GetPlayer()->isAlive())
        return false;

    Player *target = getSelectedPlayer();
    if (!target)
    {
        target = m_session->GetPlayer();
        if (!target)
            return false;
    }

    uint32 idMorph = atoi(args);
    if (idMorph == 0)
        PSendSysMessage(LANG_COMMAND_CONF_MORPH_RACE_PL_DEL, target->GetName());
    else
        PSendSysMessage(LANG_COMMAND_CONF_MORPH_RACE_SET, target->GetName());
    ConfrerieDatabase.PExecute("DELETE FROM player_race_relation WHERE guid = '%u'", target->GetGUID());
    if (idMorph != 0)
        ConfrerieDatabase.PExecute("INSERT INTO player_race_relation (morph, guid) VALUES ('%u', '%u')", idMorph, target->GetGUID());

    return true;
}

bool ChatHandler::HandleConfrerieMorphGetCommand(char* args)
{
    Player* target;
    uint64 target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
        return false;

    QueryResult* result = ConfrerieDatabase.PQuery("SELECT pr.entry, pr.comments FROM player_race pr, player_race_relation prr WHERE pr.entry = prr.morph AND prr.guid = '%u'", target->GetGUID());
    if (!result)
    {
        PSendSysMessage(LANG_COMMAND_CONF_MORPH_RACE_NO_MORPH, target_name.c_str());
        return true;
    }
    
    Field *fields = result->Fetch();
    PSendSysMessage(LANG_COMMAND_CONF_MORPH_RACE_GET, target_name.c_str(), fields[0].GetUInt32(), fields[1].GetString());

    delete result;
    return true;
}

bool ChatHandler::HandleConfrerieMorphCreateCommand(char* args)
{
    char* nameMorph = ExtractQuotedArg(&args);
    if (!nameMorph)
        return false;

    QueryResult *result = ConfrerieDatabase.Query("SELECT MAX(entry) FROM player_race");
    if (!result)
        return false;

    Field* fields = result->Fetch();
    uint32 maxRaceEntry = fields[0].GetUInt32();
    std::string nameMorphStr = nameMorph;

    ConfrerieDatabase.PExecute("INSERT INTO player_race (entry, comments) VALUES ('%u', '%s')", maxRaceEntry + 1, nameMorphStr.c_str());
    PSendSysMessage(LANG_COMMAND_CONF_MORPH_RACE_ADD, nameMorphStr.c_str(), maxRaceEntry + 1);
    delete result;
    return true;
}

bool ChatHandler::HandleConfrerieMorphEditCommand(char* args)
{
    int idMorph;
    if (!ExtractInt32(&args, idMorph))
        return false;

    char* argEdit = ExtractQuotedArg(&args);
    if (!argEdit)
        return false;

    std::string argEditStr = argEdit;

    if (argEditStr == "displayid_m")
    {
        int param;
        if (!ExtractInt32(&args, param))
            return false;
        if (!ConfrerieDatabase.PExecute("UPDATE player_race SET displayid_m = '%u' WHERE entry = '%u'", param, idMorph))
            return false;
    }
    else if (argEditStr == "displayid_w")
    {
        int param;
        if (!ExtractInt32(&args, param))
            return false;
        if (!ConfrerieDatabase.PExecute("UPDATE player_race SET displayid_w = '%u' WHERE entry = '%u'", param, idMorph))
            return false;
    }
    else if (argEditStr == "scale")
    {
        float param;
        if (!ExtractFloat(&args, param))
            return false;
        if (!ConfrerieDatabase.PExecute("UPDATE player_race SET scale = '%f' WHERE entry = '%u'", param, idMorph))
            return false;
    }
    else if (argEditStr == "speed")
    {
        float param;
        if (!ExtractFloat(&args, param))
            return false;
        if (!ConfrerieDatabase.PExecute("UPDATE player_race SET speed = '%f' WHERE entry = '%u'", param, idMorph))
            return false;
    }
    else if (argEditStr == "aura1")
    {
        int param;
        if (!ExtractInt32(&args, param))
            return false;
        if (!ConfrerieDatabase.PExecute("UPDATE player_race SET aura1 = '%u' WHERE entry = '%u'", param, idMorph))
            return false;
    }
    else if (argEditStr == "aura2")
    {
        int param;
        if (!ExtractInt32(&args, param))
            return false;
        if (!ConfrerieDatabase.PExecute("UPDATE player_race SET aura2 = '%u' WHERE entry = '%u'", param, idMorph))
            return false;
    }
    else if (argEditStr == "aura3")
    {
        int param;
        if (!ExtractInt32(&args, param))
            return false;
        if (!ConfrerieDatabase.PExecute("UPDATE player_race SET aura3 = '%u' WHERE entry = '%u'", param, idMorph))
            return false;
    }
    else if (argEditStr == "spell1")
    {
        int param;
        if (!ExtractInt32(&args, param))
            return false;
        if (!ConfrerieDatabase.PExecute("UPDATE player_race SET spell1 = '%u' WHERE entry = '%u'", param, idMorph))
            return false;
    }
    else if (argEditStr == "spell2")
    {
        int param;
        if (!ExtractInt32(&args, param))
            return false;
        if (!ConfrerieDatabase.PExecute("UPDATE player_race SET spell2 = '%u' WHERE entry = '%u'", param, idMorph))
            return false;
    }
    else if (argEditStr == "spell3")
    {
        int param;
        if (!ExtractInt32(&args, param))
            return false;
        if (!ConfrerieDatabase.PExecute("UPDATE player_race SET spell3 = '%u' WHERE entry = '%u'", param, idMorph))
            return false;
    }
    else if (argEditStr == "comments")
    {
        char* param = ExtractQuotedArg(&args);
        if (!param)
            return false;
        std::string paramStr = param;
        if (!ConfrerieDatabase.PExecute("UPDATE player_race SET comments = '%s' WHERE entry = '%u'", paramStr.c_str(), idMorph))
            return false;
    }
    else
        return false;

    PSendSysMessage(LANG_COMMAND_CONF_MORPH_RACE_EDIT, argEditStr.c_str(), idMorph);
    return true;
}

bool ChatHandler::HandleConfrerieMorphDeleteCommand(char* args)
{
    uint32 idMorph = atoi(args);

    ConfrerieDatabase.PExecute("DELETE FROM player_race WHERE entry = '%u'", idMorph);
    ConfrerieDatabase.PExecute("DELETE FROM player_race_relation WHERE morph = '%u'", idMorph);
    PSendSysMessage(LANG_COMMAND_CONF_MORPH_RACE_DEL, idMorph);

    return true;
}

bool ChatHandler::HandleConfrerieMorphListCommand(char* args)
{
    char* cFilter = ExtractLiteralArg(&args);
    std::string filter = cFilter ? cFilter : "";
    LoginDatabase.escape_string(filter);

    QueryResult* result;

    if(filter.empty())
    {
        result = ConfrerieDatabase.Query("SELECT entry, displayid_m, displayid_w, scale, speed, aura1, aura2, aura3,"
            " spell1, spell2, spell3, comments FROM player_race WHERE entry <> 0 GROUP BY entry");
    }
    else
    {
        result = ConfrerieDatabase.PQuery("SELECT entry, displayid_m, displayid_w, scale, speed, aura1, aura2, aura3,"
            " spell1, spell2, spell3, comments FROM player_race WHERE comments "_LIKE_" "_CONCAT3_("'%%'","'%s'","'%%'") " GROUP BY entry", filter.c_str());
    }

    if (!result)
    {
        PSendSysMessage(LANG_COMMAND_CONF_MORPH_RACE_NO_RESULT);
        return false;
    }

    SendSysMessage("= = = = = = = = = = = = = = = = = = = =");
    do
    {
        Field *fields = result->Fetch();
        SendSysMessage("------------------------------------");
        PSendSysMessage(LANG_COMMAND_CONF_MORPH_LIST_DESC_P1, fields[0].GetUInt32(), fields[11].GetString(), fields[1].GetUInt32(),
            fields[2].GetUInt32());
        PSendSysMessage(LANG_COMMAND_CONF_MORPH_LIST_DESC_P2, fields[4].GetFloat(), fields[3].GetFloat(), fields[8].GetUInt32(),
            fields[9].GetUInt32(), fields[10].GetUInt32(), fields[5].GetUInt32(), fields[6].GetUInt32(), fields[7].GetUInt32());
    }while( result->NextRow() );
    SendSysMessage("------------------------------------");
    SendSysMessage("= = = = = = = = = = = = = = = = = = = =");

    delete result;
    return true;
}

/* Requête SQL
    CREATE TABLE `denied_items` (
      `itemId` int(11) unsigned NOT NULL,
      `comment` varchar(255),
      PRIMARY KEY  (`itemId`)
    ) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_general_ci;
    INSERT INTO `mangos_string` (`entry`, `content_default`) VALUES 
        (12030, 'Vous ne possédez aucun Gage de recherche de Halaa !'),
        (12031, 'Erreur lors de l\'exécution de la commande, contactez un Administrateur.'),
        (12032, 'L\'objet d\'ID %u n\'est pas autorisé à être ajouté !'),
        (12033, 'L\'objet d\'ID %u a bien été ajouté en échange d\'un Gage de recherche de Halaa !');
*/
/* Textes
    LANG_COMMAND_CONF_ADDITEM_NO_REQ_ITEM   "Vous ne possédez aucun Gage de recherche de Halaa !"
    LANG_COMMAND_CONF_ADDITEM_SQL_ERROR     "Erreur lors de l'exécution de la commande, contactez un Administrateur."
    LANG_COMMAND_CONF_ADDITEM_ITEM_DENIED   "L'objet d'ID %u n'est pas autorisé à être ajouté !"
    LANG_COMMAND_CONF_ADDITEM_ITEM_ADDED    "L'objet d'ID %u a bien été ajouté en échange d'un Gage de recherche de Halaa !"
*/
bool ChatHandler::HandleConfrerieAddItemCommand(char* args)
{
    if(!m_session->GetPlayer()->isAlive())
    {
        SetSentErrorMessage(true);
        return false;
    }

    if (!*args)
        return false;

    char* cId = ExtractKeyFromLink(&args, "Hitem");
    if(!cId)
        return false;

    uint32 itemId = 0;
    if (!ExtractUInt32(&cId, itemId)) // [name] manual form
    {
        std::string itemName = cId;
        WorldDatabase.escape_string(itemName);
        QueryResult *result = WorldDatabase.PQuery("SELECT entry FROM item_template WHERE name = '%s'", itemName.c_str());
        if (!result)
        {
            PSendSysMessage(LANG_COMMAND_COULDNOTFIND, cId);
            SetSentErrorMessage(true);
            return false;
        }
        itemId = result->Fetch()->GetUInt16();
        delete result;
    }

    int32 count = 1;
    uint32 requiredItem = 26044;

    Player* pl = m_session->GetPlayer();

    ItemPrototype const *pProto = sObjectMgr.GetItemPrototype(itemId);
    if(!pProto)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, itemId);
        SetSentErrorMessage(true);
        return false;
    }

    // Gage de recherche de Halaa possédé ?
    if (pl->HasItemCount(requiredItem, 1, false))
    {
        // On vérifie que l'objet est autorisé.
        QueryResult *result = ConfrerieDatabase.PQuery("SELECT COUNT(itemId) FROM denied_items WHERE itemId = %u", itemId);
        if (result)
        {
            Field *fields=result->Fetch();
            uint32 itemCount = fields[0].GetUInt32();
            delete result;
            if (itemCount == 0) // Si il n'est pas dans la DB, c'est qu'il est autorisé.
            {
                uint32 noSpaceForCount = 0;
                ItemPosCountVec dest;
                uint8 msg = pl->CanStoreNewItem( NULL_BAG, NULL_SLOT, dest, itemId, count, &noSpaceForCount );
                if( msg != EQUIP_ERR_OK )
                    count -= noSpaceForCount;

                if( count == 0 || dest.empty())
                {
                    PSendSysMessage(LANG_ITEM_CANNOT_CREATE, itemId, noSpaceForCount );
                    SetSentErrorMessage(true);
                    return false;
                }

                // On supprime l'objet requis...
                pl->DestroyItemCount(requiredItem, 1, true, false);
                // ...puis on ajoute l'objet demandé.
                pl->StoreNewItem( dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId));
                PSendSysMessage(LANG_COMMAND_CONF_ADDITEM_ITEM_ADDED, itemId);
                return true;
            }
            else // Objet non autorisé.
            {
                PSendSysMessage(LANG_COMMAND_CONF_ADDITEM_ITEM_DENIED, itemId);
                SetSentErrorMessage(true);
                return false;
            }
        }
        else // Aucun résultat lors de la requête (table inexistante ?)
        {
            PSendSysMessage(LANG_COMMAND_CONF_ADDITEM_SQL_ERROR);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else // Gage de recherche de Halaa non possédé.
    {
        PSendSysMessage(LANG_COMMAND_CONF_ADDITEM_NO_REQ_ITEM);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandleSellerAddItemCommand(char* args)
{
    if(!m_session->GetPlayer()->isAlive())
    {
        SetSentErrorMessage(true);
        return false;
    }

    char* cId = ExtractKeyFromLink(&args, "Hitem");
    if(!cId)
        return false;

    uint32 itemId = 0;
    if (!ExtractUInt32(&cId, itemId))                       // [name] manual form
    {
        std::string itemName = cId;
        WorldDatabase.escape_string(itemName);
        QueryResult *result = WorldDatabase.PQuery("SELECT entry FROM item_template WHERE name = '%s'", itemName.c_str());
        if (!result)
        {
            PSendSysMessage(LANG_COMMAND_COULDNOTFIND, cId);
            SetSentErrorMessage(true);
            return false;
        }
        itemId = result->Fetch()->GetUInt16();
        delete result;
    }

    int32 count = 1;
    Player* pl = m_session->GetPlayer();
    Player* plTarget = getSelectedPlayer();
    if(!plTarget)
        plTarget = pl;
        
    // Selection des permissions du vendeur
    QueryResult *resultSeller = ConfrerieDatabase.PQuery("SELECT vendorTypeFlag, itemLevelMax, itemQualityMax, itemRequierd FROM player_seller WHERE pguid='%u'", pl->GetGUID());
    if (!resultSeller) 
    {
        SendSysMessage(LANG_COMMAND_SELLER_ADD_NOSELLER);
        return true;
    }
    Field* fieldsSeller = resultSeller->Fetch();
    uint32 vendorTypeFlag       = fieldsSeller[0].GetUInt32();
    uint32 level_item_max       = fieldsSeller[1].GetUInt32();
    uint32 qualitee_item_max    = fieldsSeller[2].GetUInt32();
    uint32 idItemRequierd       = fieldsSeller[3].GetUInt32();
    uint32 money = pl->GetMoney();
    delete resultSeller;

    ItemPrototype const *pProto = sObjectMgr.GetItemPrototype(itemId);
    if(!pProto)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, itemId);
        return true;
    }

    if (pProto->Quality > qualitee_item_max)
    {
        SendSysMessage(LANG_COMMAND_SELLER_ADD_NOQUALITY);
        return true;
    }

    if (pProto->ItemLevel > level_item_max)
    {
        SendSysMessage(LANG_COMMAND_SELLER_ADD_NOLEVEL);
        return true;
    }

    // Selection du nombre de 'jetons'
    if (!pl->HasItemCount(idItemRequierd, 1, false))
    {
        SendSysMessage(LANG_COMMAND_SELLER_ADD_NOJETON);
        return true;
    }
    
    uint32 itemPrix = 0;
    switch (pProto->InventoryType)
    {
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 10:
    case 16:
    case 20:
        if (vendorTypeFlag & PLAYER_SELLER_CONF_FLAG_ARMOR)
            itemPrix = 10000 * pProto->Quality * pProto->ItemLevel / 25;
        else 
        {
            PSendSysMessage(LANG_COMMAND_SELLER_ADD_DENIED, itemId);
            SetSentErrorMessage(true);
            return false; 
        }
        break;
    case 13:
    case 14:
    case 17:
    case 21:
        if (vendorTypeFlag & PLAYER_SELLER_CONF_FLAG_WEAPONS)
            itemPrix = 10000 * pProto->Quality * pProto->ItemLevel / 25;
        else 
        {
            PSendSysMessage(LANG_COMMAND_SELLER_ADD_DENIED, itemId);
            SetSentErrorMessage(true);
            return false; 
        }
        break;
    case 2:
    case 9:
    case 11:
    case 12:
    case 22:
    case 23:
    case 28:
        if (vendorTypeFlag & PLAYER_SELLER_CONF_FLAG_JEWELERY)
            itemPrix = 10000 * pProto->Quality * pProto->ItemLevel / 50;
        else {
            PSendSysMessage(LANG_COMMAND_SELLER_ADD_DENIED, itemId);
            SetSentErrorMessage(true);
            return false; }
        break;
    default:
        if (((pProto->Class == 15 && pProto->SubClass == 5) || (pProto->Class == 15 && pProto->SubClass == 2)) && (vendorTypeFlag & PLAYER_SELLER_CONF_FLAG_OTHER)) 
            itemPrix = 10000 * pProto->Quality * pProto->ItemLevel; 
        else 
        {
            PSendSysMessage(LANG_COMMAND_SELLER_ADD_DENIED, itemId);
            return true; 
        }
        break;
    }

    if (plTarget == pl)
    {
        int loc_idx = GetSessionDbLocaleIndex();
        if ( loc_idx >= 0 )
        {
            ItemLocale const *il = sObjectMgr.GetItemLocale(pProto->ItemId);
            if (il)
            {
                if (il->Name.size() > loc_idx && !il->Name[loc_idx].empty())
                {
                    std::string name = il->Name[loc_idx];
                    uint32 gold = itemPrix /GOLD;
                    uint32 silv = (itemPrix % GOLD) / SILVER;
                    uint32 copp = (itemPrix % GOLD) % SILVER;
                    PSendSysMessage(LANG_COMMAND_SELLER_ADD_PRICE, itemId, itemId, name.c_str(), gold, silv, copp);
                    // %d - |cffffffff|Hitem:%d:0:0:0:0:0:0:0:0|h[%s]|h|r - Prix : %uPO %uPA %uPC.
                }
            }
        }
        return true;
    }

    if(pl->GetGuildId() != 0)
    {
        if(pl->GetGuildId() == plTarget->GetGuildId() && !(vendorTypeFlag & PLAYER_SELLER_CONF_FLAG_GUILD_SELL)) // Non autorise a vendre a sa guilde.
        {
            PSendSysMessage(LANG_COMMAND_SELLER_ADD_NOGUILD);
            return true;
        }
    }
        
    if (money < itemPrix) 
    {
        PSendSysMessage(LANG_COMMAND_SELLER_ADD_NOMONEY, itemPrix);
        return true;
    }

    //Adding items
    uint32 noSpaceForCount = 0;
    ItemPosCountVec dest;
    uint8 msg = plTarget->CanStoreNewItem( NULL_BAG, NULL_SLOT, dest, itemId, count, &noSpaceForCount );
    if( msg != EQUIP_ERR_OK )
        count -= noSpaceForCount;

    if( count == 0 || dest.empty())
    {
        PSendSysMessage(LANG_ITEM_CANNOT_CREATE, itemId, noSpaceForCount );
        SetSentErrorMessage(true);
        return false;
    }

    Item* item = plTarget->StoreNewItem( dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId));
    if(count > 0 && item)
    {
        pl->DestroyItemCount(idItemRequierd, 1, true, false);
        int32 newmoney = int32(money) - int32(itemPrix);
        pl->SetMoney( newmoney );
        pl->SendNewItem(item,count,false,true);
        if(pl!=plTarget)
            plTarget->SendNewItem(item,count,true,false);
    }

    if(noSpaceForCount > 0)
        PSendSysMessage(LANG_ITEM_CANNOT_CREATE, itemId, noSpaceForCount);

    return true;
}

bool ChatHandler::HandleSellerAddItemLvlCommand(char* args)
{
    Player* plTarget = getSelectedPlayer();
    if (!plTarget)
        return false;

    int newItemLvl;
    if (!ExtractInt32(&args, newItemLvl))
        return false;

    QueryResult *result = ConfrerieDatabase.PQuery("SELECT `vendorTypeFlag`, `itemQualityMax`, `itemRequierd` "
        "FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
    if (result)
    {
        Field *fields=result->Fetch();
        uint32 vendorTypeFlag   = fields[0].GetUInt32();
        uint32 itemQualityMax   = fields[1].GetUInt32();
        uint32 itemRequierd     = fields[2].GetUInt32();
        delete result;
        ConfrerieDatabase.PExecute("DELETE FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
        ConfrerieDatabase.PExecute("INSERT INTO `player_seller` (`pguid`, `vendorTypeFlag`, `itemLevelMax`, `itemQualityMax`, `itemRequierd`) "
            "VALUES (%u, %u, %u, %u, %u)", plTarget->GetGUID(), vendorTypeFlag, newItemLvl, itemQualityMax, itemRequierd);
    }
    else
    {
        ConfrerieDatabase.PExecute("DELETE FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
        ConfrerieDatabase.PExecute("INSERT INTO `player_seller` (`pguid`, `vendorTypeFlag`, `itemLevelMax`, `itemQualityMax`, `itemRequierd`) "
            "VALUES (%u, %u, %u, %u, %u)", plTarget->GetGUID(), 0, newItemLvl, 0, 0);
    }

    PSendSysMessage("Mise à jour du ItemLevelMax du joueur [%u] %s par : %u.", plTarget->GetGUID(), plTarget->GetName(), newItemLvl);
    return true;
}

bool ChatHandler::HandleSellerAddItemQuaCommand(char* args)
{
    Player* plTarget = getSelectedPlayer();
    if (!plTarget)
        return false;

    int newItemQua;
    if (!ExtractInt32(&args, newItemQua))
        return false;

    QueryResult *result = ConfrerieDatabase.PQuery("SELECT `vendorTypeFlag`, `itemLevelMax`, `itemRequierd` "
        "FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
    if (result)
    {
        Field *fields=result->Fetch();
        uint32 vendorTypeFlag   = fields[0].GetUInt32();
        uint32 itemLevelMax     = fields[1].GetUInt32();
        uint32 itemRequierd     = fields[2].GetUInt32();
        delete result;
        ConfrerieDatabase.PExecute("DELETE FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
        ConfrerieDatabase.PExecute("INSERT INTO `player_seller` (`pguid`, `vendorTypeFlag`, `itemLevelMax`, `itemQualityMax`, `itemRequierd`) "
            "VALUES (%u, %u, %u, %u, %u)", plTarget->GetGUID(), vendorTypeFlag, itemLevelMax, newItemQua, itemRequierd);
    }
    else
    {
        ConfrerieDatabase.PExecute("DELETE FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
        ConfrerieDatabase.PExecute("INSERT INTO `player_seller` (`pguid`, `vendorTypeFlag`, `itemLevelMax`, `itemQualityMax`, `itemRequierd`) "
            "VALUES (%u, %u, %u, %u, %u)", plTarget->GetGUID(), 0, 0, newItemQua, 0);
    }

    PSendSysMessage("Mise à jour du ItemQualityMax du joueur [%u] %s par : %u.", plTarget->GetGUID(), plTarget->GetName(), newItemQua);
    return true;
}

bool ChatHandler::HandleSellerAddItemReqCommand(char* args)
{
    Player* plTarget = getSelectedPlayer();
    if (!plTarget)
        return false;

    int newItemReq;
    if (!ExtractInt32(&args, newItemReq))
        return false;

    QueryResult *result = ConfrerieDatabase.PQuery("SELECT `vendorTypeFlag`, `itemLevelMax`, `itemQualityMax` "
        "FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
    if (result)
    {
        Field *fields=result->Fetch();
        uint32 vendorTypeFlag   = fields[0].GetUInt32();
        uint32 itemLevelMax     = fields[1].GetUInt32();
        uint32 itemQualityMax   = fields[2].GetUInt32();
        delete result;
        ConfrerieDatabase.PExecute("DELETE FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
        ConfrerieDatabase.PExecute("INSERT INTO `player_seller` (`pguid`, `vendorTypeFlag`, `itemLevelMax`, `itemQualityMax`, `itemRequierd`) "
            "VALUES (%u, %u, %u, %u, %u)", plTarget->GetGUID(), vendorTypeFlag, itemLevelMax, itemQualityMax, newItemReq);
    }
    else
    {
        ConfrerieDatabase.PExecute("DELETE FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
        ConfrerieDatabase.PExecute("INSERT INTO `player_seller` (`pguid`, `vendorTypeFlag`, `itemLevelMax`, `itemQualityMax`, `itemRequierd`) "
            "VALUES (%u, %u, %u, %u, %u)", plTarget->GetGUID(), 0, 0, 0, newItemReq);
    }

    PSendSysMessage("Mise à jour du ItemRequierd du joueur [%u] %s par : %u.", plTarget->GetGUID(), plTarget->GetName(), newItemReq);
    return true;
}

bool ChatHandler::HandleSellerAddItemFlagsCommand(char* args)
{
    Player* plTarget = getSelectedPlayer();
    if (!plTarget)
        return false;

    int newVendorFlag;
    if (!ExtractInt32(&args, newVendorFlag))
        return false;

    QueryResult *result = ConfrerieDatabase.PQuery("SELECT `itemLevelMax`, `itemQualityMax`, `itemRequierd` "
        "FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
    if (result)
    {
        Field *fields=result->Fetch();
        uint32 itemLevelMax     = fields[0].GetUInt32();
        uint32 itemQualityMax   = fields[1].GetUInt32();
        uint32 itemRequierd     = fields[2].GetUInt32();
        delete result;
        ConfrerieDatabase.PExecute("DELETE FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
        ConfrerieDatabase.PExecute("INSERT INTO `player_seller` (`pguid`, `vendorTypeFlag`, `itemLevelMax`, `itemQualityMax`, `itemRequierd`) "
            "VALUES (%u, %u, %u, %u, %u)", plTarget->GetGUID(), newVendorFlag, itemLevelMax, itemQualityMax, itemRequierd);
    }
    else
    {
        ConfrerieDatabase.PExecute("DELETE FROM `player_seller` WHERE `pguid` = %u", plTarget->GetGUID());
        ConfrerieDatabase.PExecute("INSERT INTO `player_seller` (`pguid`, `vendorTypeFlag`, `itemLevelMax`, `itemQualityMax`, `itemRequierd`) "
            "VALUES (%u, %u, %u, %u, %u)", plTarget->GetGUID(), newVendorFlag, 0, 0, 0);
    }

    PSendSysMessage("Mise à jour du newVendorFlag du joueur [%u] %s par : %u.", plTarget->GetGUID(), plTarget->GetName(), newVendorFlag);
    return true;
}

bool ChatHandler::HandleConfrerieItemNumberCommand(char* args)
{
    if (!*args)
        return false;

    char* cId = ExtractKeyFromLink(&args, "Hitem");
    if(!cId)
        return false;

    uint32 itemId = 0;
    if (!ExtractUInt32(&cId, itemId))
    {
        std::string itemName = cId;
        WorldDatabase.escape_string(itemName);
        QueryResult *result = WorldDatabase.PQuery("SELECT entry FROM item_template WHERE name = '%s'", itemName.c_str());
        if (!result)
        {
            PSendSysMessage(LANG_COMMAND_COULDNOTFIND, cId);
            SetSentErrorMessage(true);
            return false;
        }
        itemId = result->Fetch()->GetUInt16();
        delete result;
    }

    ItemPrototype const *pProto = sObjectMgr.GetItemPrototype(itemId);
    if(!pProto)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, itemId);
        SetSentErrorMessage(true);
        return false;
    }

    Player* target = getSelectedPlayer();
    if(!target)
        target = m_session->GetPlayer();

    uint32 itemCount = target->GetItemCount(itemId);

    PSendSysMessage("%s possède %u exemplaire(s) de l'objet <%u> [%s]", target->GetName(), itemCount, itemId, pProto->Name1);
    return true;
}

