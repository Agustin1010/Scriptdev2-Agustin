/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ScriptData
SDName: boss_anzu
SD%Complete: 70
SDComment: Timers need improvement; Intro event NYI.
SDCategory: Auchindoun, Sethekk Halls
EndScriptData */

#include "precompiled.h"
#include "sethekk_halls.h"

enum
{
    SAY_BANISH          = -1556018,
    SAY_WHISPER_MAGIC   = -1556019,
    EMOTE_BIRD_STONE    = -1556020,

    SPELL_FLESH_RIP     = 40199,
    SPELL_SCREECH       = 40184,
    SPELL_SPELL_BOMB    = 40303,
    SPELL_CYCLONE       = 40321,
    SPELL_BANISH_SELF   = 42354,

    NPC_BROOD_OF_ANZU   = 23132,

    // Helper birds
    NPC_HAWK_SPIRIT     = 23134,                // casts 40237
    NPC_FALCON_SPIRIT   = 23135,                // casts 40241
    NPC_EAGLE_SPIRIT    = 23136,                // casts 40240

    MAX_BROODS          = 5,
};

static const uint32 aSpiritsEntries[3] = {NPC_FALCON_SPIRIT, NPC_HAWK_SPIRIT, NPC_EAGLE_SPIRIT};

struct MANGOS_DLL_DECL boss_anzuAI : public ScriptedAI
{
    boss_anzuAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_sethekk_halls*)pCreature->GetInstanceData();
        Reset();
    }

    instance_sethekk_halls* m_pInstance;

    uint32 m_uiFleshRipTimer;
    uint32 m_uiScreechTimer;
    uint32 m_uiSpellBombTimer;
    uint32 m_uiCycloneTimer;
    uint8 m_uiBanishPhase;

    GUIDList m_lBirdsGuidList;

    void Reset()
    {
        m_uiFleshRipTimer   = urand(9000, 13000);
        m_uiScreechTimer    = 30000;
        m_uiSpellBombTimer  = 22000;
        m_uiCycloneTimer    = 5000;
        m_uiBanishPhase     = 2;
    }

    void Aggro(Unit* pWho)
    {
        // Note: this should be moved to the intro event when implemented!
        DoSummonBirdHelpers();

        if (m_pInstance)
            m_pInstance->SetData(TYPE_ANZU, IN_PROGRESS);
    }

    void JustDied(Unit* pKiller)
    {
        DespawnBirdHelpers();

        if (m_pInstance)
            m_pInstance->SetData(TYPE_ANZU, DONE);
    }

    void JustReachedHome()
    {
        DespawnBirdHelpers();
        m_creature->ForcedDespawn();

        if (m_pInstance)
            m_pInstance->SetData(TYPE_ANZU, FAIL);
    }

    void JustSummoned(Creature* pSummoned)
    {
        if (pSummoned->GetEntry() == NPC_BROOD_OF_ANZU)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                pSummoned->AI()->AttackStart(pTarget);
        }
        else
        {
            DoScriptText(EMOTE_BIRD_STONE, pSummoned);
            m_lBirdsGuidList.push_back(pSummoned->GetObjectGuid());
        }
    }

    void DoSummonBroodsOfAnzu()
    {
        if (!m_pInstance)
            return;

        // Note: the birds should fly around the room for about 10 seconds before starting to attack the players
        float fX, fY, fZ;
        if (GameObject* pClaw = m_pInstance->GetSingleGameObjectFromStorage(GO_RAVENS_CLAW))
        {
            for (uint8 i = 0; i < MAX_BROODS; ++i)
            {
                m_creature->GetRandomPoint(pClaw->GetPositionX(), pClaw->GetPositionY(), pClaw->GetPositionZ(), 7.0f, fX, fY, fZ);
                m_creature->SummonCreature(NPC_BROOD_OF_ANZU, fX, fY, fZ, 0, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 30000);
            }
        }
    }

    void DoSummonBirdHelpers()
    {
        float fX, fY, fZ, fAng;
        for (uint8 i = 0; i < 3; ++i)
        {
            fAng = 2*M_PI_F/3*i;
            m_creature->GetNearPoint(m_creature, fX, fY, fZ, 0, 15.0f, fAng);
            m_creature->SummonCreature(aSpiritsEntries[i], fX, fY, fZ, fAng + M_PI_F, TEMPSUMMON_CORPSE_DESPAWN, 0);
        }
    }

    void DespawnBirdHelpers()
    {
        for (GUIDList::const_iterator itr = m_lBirdsGuidList.begin(); itr != m_lBirdsGuidList.end(); ++itr)
        {
            if (Creature* pTemp = m_creature->GetMap()->GetCreature(*itr))
                pTemp->ForcedDespawn();
        }
    }

    void UpdateAI(const uint32 uiDiff)
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // Banish at 66% and 33%; Boss can still use spells while banished
        if (m_creature->GetHealthPercent() < 33.3f * m_uiBanishPhase)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_BANISH_SELF) == CAST_OK)
            {
                DoScriptText(SAY_BANISH, m_creature);
                DoSummonBroodsOfAnzu();
                --m_uiBanishPhase;
            }
        }

        if (m_uiFleshRipTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_FLESH_RIP) == CAST_OK)
                m_uiFleshRipTimer = urand(17000, 21000);
        }
        else
            m_uiFleshRipTimer -= uiDiff;

        if (m_uiScreechTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SCREECH) == CAST_OK)
                m_uiScreechTimer = 30000;
        }
        else
            m_uiScreechTimer -= uiDiff;

        if (m_uiSpellBombTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_SPELL_BOMB) == CAST_OK)
                {
                    DoScriptText(SAY_WHISPER_MAGIC, m_creature, pTarget);
                    m_uiSpellBombTimer = urand(25000, 30000);
                }
            }
        }
        else
            m_uiSpellBombTimer -= uiDiff;

        if (m_uiCycloneTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_CYCLONE) == CAST_OK)
                    m_uiCycloneTimer = 21000;
            }
        }
        else
            m_uiCycloneTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_anzu(Creature* pCreature)
{
    return new boss_anzuAI(pCreature);
}

void AddSC_boss_anzu()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_anzu";
    pNewScript->GetAI = &GetAI_boss_anzu;
    pNewScript->RegisterSelf();
}