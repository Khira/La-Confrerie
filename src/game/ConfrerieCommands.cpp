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
