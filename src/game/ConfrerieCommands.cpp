/*
 * Copyright (C) 2009-2010 La Confr�rie <http://la-confrerie.fr>
 *
 * Commandes cr��es sp�cialement pour le serveur de La Confr�rie.
 * Pour toute autre utilisation, vous devez contactez l'auteur de
 * ces commandes.
 * Vous 'navez pas le droit de partager ou distribuer ce code.
 *
 */

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "World.h"
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

/* Requ�te SQL
    CREATE TABLE `denied_items` (
      `itemId` int(11) unsigned NOT NULL,
      `comment` varchar(255),
      PRIMARY KEY  (`itemId`)
    ) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_general_ci;
    INSERT INTO `mangos_string` (`entry`, `content_default`) VALUES 
        (12030, 'Vous ne poss�dez aucun Gage de recherche de Halaa !'),
        (12031, 'Erreur lors de l\'ex�cution de la commande, contactez un Administrateur.'),
        (12032, 'L\'objet d\'ID %u n\'est pas autoris� � �tre ajout� !'),
        (12033, 'L\'objet d\'ID %u a bien �t� ajout� en �change d\'un Gage de recherche de Halaa !');
*/
/* Textes
    LANG_COMMAND_CONF_ADDITEM_NO_REQ_ITEM   "Vous ne poss�dez aucun Gage de recherche de Halaa !"
    LANG_COMMAND_CONF_ADDITEM_SQL_ERROR     "Erreur lors de l'ex�cution de la commande, contactez un Administrateur."
    LANG_COMMAND_CONF_ADDITEM_ITEM_DENIED   "L'objet d'ID %u n'est pas autoris� � �tre ajout� !"
    LANG_COMMAND_CONF_ADDITEM_ITEM_ADDED    "L'objet d'ID %u a bien �t� ajout� en �change d'un Gage de recherche de Halaa !"
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

    // Gage de recherche de Halaa poss�d� ?
    if (pl->HasItemCount(requiredItem, 1, false))
    {
        // On v�rifie que l'objet est autoris�.
        QueryResult *result = ConfrerieDatabase.PQuery("SELECT COUNT(itemId) FROM denied_items WHERE itemId = %u", itemId);
        if (result)
        {
            Field *fields=result->Fetch();
            uint32 itemCount = fields[0].GetUInt32();
            delete result;
            if (itemCount == 0) // Si il n'est pas dans la DB, c'est qu'il est autoris�.
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
                // ...puis on ajoute l'objet demand�.
                pl->StoreNewItem( dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId));
                PSendSysMessage(LANG_COMMAND_CONF_ADDITEM_ITEM_ADDED, itemId);
                return true;
            }
            else // Objet non autoris�.
            {
                PSendSysMessage(LANG_COMMAND_CONF_ADDITEM_ITEM_DENIED, itemId);
                SetSentErrorMessage(true);
                return false;
            }
        }
        else // Aucun r�sultat lors de la requ�te (table inexistante ?)
        {
            PSendSysMessage(LANG_COMMAND_CONF_ADDITEM_SQL_ERROR);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else // Gage de recherche de Halaa non poss�d�.
    {
        PSendSysMessage(LANG_COMMAND_CONF_ADDITEM_NO_REQ_ITEM);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}
