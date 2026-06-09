#include <spdlog/sinks/basic_file_sink.h>

#include <cmath>
#include <limits>

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include "RE/E/ExtraPrimitive.h"
#include "RE/E/ExtraDataList.h"

namespace logger = SKSE::log;

float RandomFloat(float min, float max) { return min + (max - min) * ((float)rand() / (float)RAND_MAX); }

RE::TESObjectREFR* PlaceStaticOnGround(RE::TESObjectREFR* center, RE::TESForm* form, float minRadius, float maxRadius,
                                       bool avoidWater, bool requireOutOfSight, float zOffset, float minClearDistance,
                                       RE::BGSListForm* validSurfaceList) {
    logger::info("PlaceOnGround called!");

    //----------------------------------------
    // Validation
    //----------------------------------------

    if (!center) {
        logger::warn("center is null");
        return nullptr;
    }

    if (center->IsDeleted()) {
        logger::warn("center is deleted");
        return nullptr;
    }

    if (!form) {
        logger::warn("form is null");
        return nullptr;
    }

    auto boundObj = form->As<RE::TESBoundObject>();

    if (!boundObj) {
        logger::warn("form is not TESBoundObject");
        return nullptr;
    }

    auto tes = RE::TES::GetSingleton();

    if (!tes) {
        logger::warn("TES singleton is null");
        return nullptr;
    }

    auto cell = center->GetParentCell();

    if (!cell) {
        logger::warn("cell is null");
        return nullptr;
    }

    const auto origin = center->GetPosition();

    //----------------------------------------
    // More attempts for out-of-sight spawning
    //----------------------------------------

    int attempts = requireOutOfSight ? 30 : 10;

    //----------------------------------------
    // Attempts
    //----------------------------------------

    for (int i = 0; i < attempts; i++) {
        //----------------------------------------
        // Camera-aware angle generation
        //----------------------------------------

        float angle = 0.0f;

        if (requireOutOfSight) {
            auto camera = RE::PlayerCamera::GetSingleton();

            if (camera && camera->cameraRoot) {
                auto& rot = camera->cameraRoot->world.rotate;

                RE::NiPoint3 forward(rot.entry[0][1], rot.entry[1][1], rot.entry[2][1]);

                if (forward.SqrLength() > 0.001f) {
                    forward.Unitize();
                }

                //----------------------------------------
                // Behind-camera direction
                //----------------------------------------

                float baseAngle = std::atan2(-forward.x, -forward.y);

                //----------------------------------------
                // Random spread behind camera
                //----------------------------------------

                angle = baseAngle + RandomFloat(-0.8f, 0.8f);

            } else {
                angle = RandomFloat(0.0f, RE::NI_TWO_PI);
            }

        } else {
            angle = RandomFloat(0.0f, RE::NI_TWO_PI);
        }

        //----------------------------------------
        // Radius
        //----------------------------------------

        float radius = RandomFloat(minRadius, maxRadius);

        //----------------------------------------
        // Position
        //----------------------------------------

        float x = origin.x + std::sin(angle) * radius;
        float y = origin.y + std::cos(angle) * radius;

        //----------------------------------------
        // Ground height
        //----------------------------------------

        float groundZ = 0.0f;

        RE::NiPoint3 landQueryPos(x, y, origin.z + 500.0f);

        bool gotLand = tes->GetLandHeight(landQueryPos, groundZ);

        //----------------------------------------
        // Reject terrain if too far below
        //----------------------------------------

        if (gotLand) {
            float verticalDistance = std::abs(origin.z - groundZ);

            if (verticalDistance > maxRadius) {
                logger::info("Terrain too far below ({:.2f}), switching to valid surfaces", verticalDistance);

                gotLand = false;
            }
        }

        //----------------------------------------
        // If no valid terrain found,
        // try valid surface objects
        //----------------------------------------

        if (!gotLand) {
            logger::info("No valid terrain found, searching valid surfaces");

            float bestZ = -999999.0f;
            bool foundSurface = false;

            cell->ForEachReferenceInRange(origin, maxRadius, [&](RE::TESObjectREFR& ref) {
                //----------------------------------------
                // Must have base object
                //----------------------------------------

                auto baseObj = ref.GetBaseObject();

                if (!baseObj) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                //----------------------------------------
                // Check if base object is in formlist
                //----------------------------------------

                bool validSurface = false;

                if (validSurfaceList) {
                    for (auto entry : validSurfaceList->forms) {
                        if (entry == baseObj) {
                            validSurface = true;
                            break;
                        }
                    }
                }

                if (!validSurface) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                //----------------------------------------
                // XY distance to target point
                //----------------------------------------

                float dx = ref.GetPositionX() - x;
                float dy = ref.GetPositionY() - y;

                float distSq = dx * dx + dy * dy;

                //----------------------------------------
                // Keep only nearby candidates
                //----------------------------------------

                if (distSq > (200.0f * 200.0f)) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                //----------------------------------------
                // Use bounds top surface
                //----------------------------------------

                RE::NiPoint3 boundMax = ref.GetBoundMax();

                float topZ = ref.GetPositionZ() + boundMax.z;

                if (topZ > bestZ) {
                    bestZ = topZ;
                    foundSurface = true;
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });

            //----------------------------------------
            // Use valid surface height
            //----------------------------------------

            if (foundSurface) {
                groundZ = bestZ;

                logger::info("Using valid surface height {}", groundZ);

            } else {
                //----------------------------------------
                // Total fallback
                //----------------------------------------

                logger::info("No valid surface found, using origin z");

                groundZ = origin.z;
            }
        }

        //----------------------------------------
        // Out of sight check
        //----------------------------------------

        if (requireOutOfSight) {
            auto camera = RE::PlayerCamera::GetSingleton();

            if (camera && camera->cameraRoot) {
                //----------------------------------------
                // Camera position
                //----------------------------------------

                RE::NiPoint3 cameraPos = camera->cameraRoot->world.translate;

                //----------------------------------------
                // Camera forward
                //----------------------------------------

                auto& rot = camera->cameraRoot->world.rotate;

                RE::NiPoint3 forward(rot.entry[0][1], rot.entry[1][1], rot.entry[2][1]);

                if (forward.SqrLength() > 0.001f) {
                    forward.Unitize();
                }

                //----------------------------------------
                // Direction to spawn point
                //----------------------------------------

                RE::NiPoint3 toPoint(x - cameraPos.x, y - cameraPos.y, groundZ - cameraPos.z);

                if (toPoint.SqrLength() > 0.001f) {
                    toPoint.Unitize();
                }

                //----------------------------------------
                // Dot
                //----------------------------------------

                float dot = forward.Dot(toPoint);

                //----------------------------------------
                // Reject visible positions
                //----------------------------------------

                if (dot > 0.00f) {
                    continue;
                }
            }
        }

        //----------------------------------------
        // Water check
        //----------------------------------------

        if (avoidWater) {
            float waterHeight = 0.0f;

            RE::NiPoint3 waterCheckPos(x, y, groundZ);

            if (cell->GetWaterHeight(waterCheckPos, waterHeight)) {
                if (waterHeight >= groundZ - 20.0f) {
                    continue;
                }
            }
        }

        //----------------------------------------
        // Clearance check
        //----------------------------------------

        bool blocked = false;

        cell->ForEachReferenceInRange(RE::NiPoint3(x, y, groundZ), 256.0f, [&](RE::TESObjectREFR& ref) {
            if (&ref == center) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            //----------------------------------------
            // Ignore irrelevant refs
            //----------------------------------------

            auto formType = ref.GetFormType();

            if (formType == RE::FormType::Projectile || formType == RE::FormType::Hazard ||
                formType == RE::FormType::Light || formType == RE::FormType::IdleMarker ||
                formType == RE::FormType::EffectShader) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            //----------------------------------------
            // Distance check
            //----------------------------------------

            float dx = ref.GetPositionX() - x;
            float dy = ref.GetPositionY() - y;

            float distSq = dx * dx + dy * dy;

            //----------------------------------------
            // Approx radius from bounds
            //----------------------------------------

            RE::NiPoint3 boundMin = ref.GetBoundMin();
            RE::NiPoint3 boundMax = ref.GetBoundMax();

            float sizeX = std::abs(boundMax.x - boundMin.x);
            float sizeY = std::abs(boundMax.y - boundMin.y);

            float approxRadius = std::max(sizeX, sizeY) * 0.5f;

            //----------------------------------------
            // Bound-aware clearance
            //----------------------------------------

            float combinedRadius = minClearDistance + approxRadius;

            if (distSq < (combinedRadius * combinedRadius)) {
                blocked = true;

                return RE::BSContainer::ForEachResult::kStop;
            }

            return RE::BSContainer::ForEachResult::kContinue;
        });

        if (blocked) {
            continue;
        }

        //----------------------------------------
        // Spawn object
        //----------------------------------------

        auto refrHandle = center->PlaceObjectAtMe(boundObj, false);

        if (!refrHandle) {
            logger::warn("PlaceObjectAtMe failed");
            continue;
        }

        auto refr = refrHandle.get();

        if (!refr) {
            logger::warn("Spawned ref invalid");
            continue;
        }

        //----------------------------------------
        // Final position
        //----------------------------------------

        float finalZ = groundZ + zOffset;

        refr->SetPosition(x, y, finalZ);

        //----------------------------------------
        // FORCE WORLD ATTACH (CRÍTICO)
        //----------------------------------------

        refr->MoveTo(refr);

        //----------------------------------------
        // LOAD 3D
        //----------------------------------------

        refr->Load3D(false);
        refr->Update3DPosition(true);

        //----------------------------------------
        // DEFER HAVOK (MESMO FIX DA OUTRA FUNÇÃO)
        //----------------------------------------

        auto task = SKSE::GetTaskInterface();
        task->AddTask([ref = RE::NiPointer<RE::TESObjectREFR>(refr)]() {
            if (!ref) return;

            //logger::info("Initializing Havok (delayed ground spawn)");

            ref->InitHavok();
            ref->SetCollision(true);
            ref->MoveHavok(true);

            ref->Update3DPosition(true);
        });

        //----------------------------------------
        // Success (Havok ainda não inicializado aqui)
        //----------------------------------------

        return refr;
    }

    logger::warn("PlaceStaticOnGround FAILED ALL ATTEMPTS");

    return nullptr;
}



RE::TESObjectREFR* PlaceRefAtMe(RE::TESObjectREFR* target, RE::TESObjectREFR* templateRef) {
    logger::info("PlaceRefAtMe called");

    if (!target || !templateRef) return nullptr;

    auto baseObj = templateRef->GetBaseObject();
    if (!baseObj) return nullptr;

    auto handle = target->PlaceObjectAtMe(baseObj, false);
    if (!handle) return nullptr;

    auto newRef = handle.get();
    if (!newRef) return nullptr;

    logger::info("Spawned FormID {:08X}", newRef->GetFormID());

    //------------------------------------------------
    // POSITION / ROTATION / SCALE
    //------------------------------------------------

    auto pos = target->GetPosition();

    float scale = templateRef->GetScale();
    auto& refData = newRef->GetReferenceRuntimeData();
    refData.refScale = static_cast<std::uint16_t>(scale * 100.0f);

    newRef->SetPosition(pos);
    newRef->data.angle = templateRef->GetAngle();

    //logger::info("Applied scale: {}", scale);

    //------------------------------------------------
    // LOAD 3D
    //------------------------------------------------

    newRef->Load3D(false);
    newRef->Update3DPosition(true);

    //------------------------------------------------
    // CRÍTICO: reattach ao mundo
    //------------------------------------------------

    newRef->MoveTo(newRef);

    //------------------------------------------------
    // 🔥 IMPORTANTE: ADIAR HAVOK PARA PRÓXIMO FRAME
    //------------------------------------------------

    auto task = SKSE::GetTaskInterface();
    task->AddTask([ref = RE::NiPointer<RE::TESObjectREFR>(newRef)]() {
        if (!ref) return;

        //logger::info("Initializing Havok (delayed)");

        ref->InitHavok();
        ref->SetCollision(true);
        ref->MoveHavok(true);

        ref->Update3DPosition(true);

        //logger::info("Havok ready for {}", ref->GetFormID());
    });

    //logger::info("Spawn queued (Havok deferred)");

    return newRef;
}


std::int32_t GiveLeveledLoot(RE::Actor* actor, RE::TESLevItem* lootList) {
    if (!actor || !lootList) {
        return 0;
    }

    RE::BSScrapArray<RE::CALCED_OBJECT> results;

    lootList->CalculateCurrentFormList(actor->GetLevel(), 1, results, 0, true);

    if (results.empty()) {
        return 0;
    }

    std::int32_t totalCount = 0;

    for (auto& result : results) {
        auto boundObject = result.form ? result.form->As<RE::TESBoundObject>() : nullptr;

        if (!boundObject) {
            logger::warn("Generated form is not TESBoundObject: {:08X}", result.form ? result.form->GetFormID() : 0);
            continue;
        }

        actor->AddObjectToContainer(boundObject, nullptr, result.count, nullptr);

        totalCount += result.count;

        logger::info("Added {} x{}", boundObject->GetName(), result.count);
    }

    logger::info("GiveLeveledLoot returned {}", totalCount);

    return totalCount;
}


