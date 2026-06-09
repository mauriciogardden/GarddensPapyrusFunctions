#pragma once

#include "RE/A/Actor.h"
#include "RE/A/ActorValueOwner.h"
#include "GarddenCombatFunctions.h"

namespace Hooks {

    void Install();
    void Reset();
    void Stop();

}

namespace GarddenCombat {
    // Função para instalar hooks de combate
    void Install();

    // ponteiros originais para ModActorValue
    extern void(*OriginalDamageActorValue)(RE::ActorValueOwner*, RE::ActorValue, float);
}
