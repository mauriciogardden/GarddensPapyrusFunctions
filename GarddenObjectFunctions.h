#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

RE::TESObjectREFR* PlaceStaticOnGround(RE::TESObjectREFR* center, RE::TESForm* form, float minRadius, float maxRadius,
                                       bool avoidWater, bool requireOutOfSight, float zOffset, float minClearDistance,
                                       RE::BGSListForm* validSurfaceList);

RE::TESObjectREFR* PlaceRefAtMe(RE::TESObjectREFR* target, RE::TESObjectREFR* templateRef);

std::int32_t GiveLeveledLoot(RE::Actor* actor, RE::TESLevItem* lootList);

