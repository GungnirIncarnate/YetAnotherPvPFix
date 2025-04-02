#include <Mod/CppUserModBase.hpp>
#include <UE4SSProgram.hpp>
#include <cstdint>
#include <memory>
#include <safetyhook.hpp>
#include <Unreal/AActor.hpp>
#include <Unreal/UClass.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/UScriptStruct.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/GameplayStatics.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/Property/NumericPropertyTypes.hpp>

#include <Signatures.hpp>
#include <SigScanner/SinglePassSigScanner.hpp>

// Palworld SDK
#include <SDK/Classes/PalMapObjectDamageReactionComponent.h>
#include <SDK/Classes/PalMapObjectUtility.h>
#include <SDK/Classes/PalUtility.h>
#include <SDK/Classes/PalOptionSubsystem.h>

#include <ModConfig.h>

using namespace RC;
using namespace RC::Unreal;
using namespace Palworld;

std::vector<SignatureContainer> SigContainer;
SinglePassScanner::SignatureContainerMap SigContainerMap;

UClass* CLASS_MapObject = nullptr;

bool EnablePvPDamageToBuildings = false;

// Signature stuff, expect them to break with updates
void BeginScan()
{
    SignatureContainer MapObjectDamageReactionComponent_CanProcessDamage_Signature = [=]() -> SignatureContainer {
        return {
            {{ "48 89 5C 24 18 48 89 6C 24 20 56 57 41 56 48 83 EC 40 48 8B 79 F0"}},
            [=](SignatureContainer& self) {
                void* FunctionPointer = static_cast<void*>(self.get_match_address());

                UPalMapObjectDamageReactionComponent::CanProcessDamage_Internal =
                    reinterpret_cast<UPalMapObjectDamageReactionComponent::TYPE_CanProcessDamage>(FunctionPointer);

                self.get_did_succeed() = true;

                return true;
            },
            [](const SignatureContainer& self) {
                if (!self.get_did_succeed())
                {
                    Output::send<LogLevel::Error>(STR("Failed to find signature for UPalMapObjectDamageReactionComponent::CanProcessDamage\n"));
                }
            }
        };
    }();

    SigContainer.emplace_back(MapObjectDamageReactionComponent_CanProcessDamage_Signature);
    SigContainerMap.emplace(ScanTarget::MainExe, SigContainer);
    SinglePassScanner::start_scan(SigContainerMap);
}

SafetyHookInline MapObject_CanProcessDamage_Hook{};
bool __stdcall MapObjectDamageReactionComponent_CanProcessDamage(UPalMapObjectDamageReactionComponent* This, uint8_t* DamageInfo)
{
    auto MapObject = *(AActor**)(This + -0x10);
    auto MapObjectClass = MapObject->GetClassPrivate();
    if (MapObjectClass)
    {
        auto SuperClass = MapObject->GetClassPrivate()->GetSuperClass();
        if (SuperClass == CLASS_MapObject)
        {
            return MapObject_CanProcessDamage_Hook.call<bool>(This, DamageInfo);
        }
    }

    return EnablePvPDamageToBuildings;
}

class YetAnotherPvPFix : public RC::CppUserModBase
{
public:
    YetAnotherPvPFix() : CppUserModBase()
    {
        ModName = STR("YetAnotherPvPFix");
        ModVersion = STR("1.0.0");
        ModDescription = STR("Fixes PvP damage not working for both players and buildings after Crossplay patch.");
        ModAuthors = STR("Okaetsu");

        Output::send<LogLevel::Verbose>(STR("{} v{} by {} loaded.\n"), ModName, ModVersion, ModAuthors);
    }

    ~YetAnotherPvPFix() override
    {
    }

    auto on_update() -> void override
    {
    }

    auto on_unreal_init() -> void override
    {
        Output::send<LogLevel::Verbose>(STR("[{}] loaded successfully!\n"), ModName);

        CLASS_MapObject = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, TEXT("/Script/Pal.PalMapObject"));

        auto& Settings = PVP::Config::Settings::get();
        Settings.deserialize();

        static bool HasInitialized = false;
        Hook::RegisterBeginPlayPostCallback([&](AActor* Actor) {
            if (HasInitialized) return;
            HasInitialized = true;

            auto OptionSubsystem = UPalUtility::GetOptionSubsystem(Actor);
            auto Property = OptionSubsystem->GetPropertyByNameInChain(STR("OptionWorldSettings"));
            if (!Property)
            {
                Output::send<LogLevel::Error>(STR("[{}] failed to load, property 'OptionWorldSettings' doesn't exist in OptionSubsystem anymore.\n"), ModName);
                return;
            }

            auto OptionWorldSettingsProperty = CastField<FStructProperty>(Property);
            if (!OptionWorldSettingsProperty)
            {
                Output::send<LogLevel::Error>(STR("[{}] failed to load, property 'OptionWorldSettings' is not a Struct Property?\n"), ModName);
                return;
            }

            auto OptionWorldSettingsStruct = OptionWorldSettingsProperty->GetStruct();
            if (!OptionWorldSettingsStruct)
            {
                Output::send<LogLevel::Error>(STR("[{}] failed to load, property 'OptionWorldSettings' is not a valid Struct?\n"), ModName);
                return;
            }

            void* OptionWorldSettings = OptionWorldSettingsProperty->ContainerPtrToValuePtr<void>(OptionSubsystem);
            if (!OptionWorldSettings)
            {
                Output::send<LogLevel::Error>(STR("[{}] failed to load, could not get internal data for property 'OptionWorldSettings'.\n"), ModName);
                return;
            }

            if (Settings.PVP.EnablePlayerToPlayerDamage)
            {
                EnablePvPDamageToBuildings = true;

                auto bEnablePlayerToPlayerDamage_Property = OptionWorldSettingsStruct->GetPropertyByName(STR("bEnablePlayerToPlayerDamage"));
                if (!bEnablePlayerToPlayerDamage_Property)
                {
                    Output::send<LogLevel::Error>(STR("[{}] failed to load, property 'bEnablePlayerToPlayerDamage' doesn't exist in OptionWorldSettings anymore.\n"), ModName);
                    return;
                }

                auto bEnablePlayerToPlayerDamage = CastField<FBoolProperty>(bEnablePlayerToPlayerDamage_Property);
                if (!bEnablePlayerToPlayerDamage)
                {
                    Output::send<LogLevel::Error>(STR("[{}] failed to load, property 'bEnablePlayerToPlayerDamage' is not a bool anymore.\n"), ModName);
                    return;
                }

                bEnablePlayerToPlayerDamage->SetPropertyValueInContainer(OptionWorldSettings, true);
                OptionSubsystem->SetOptionWorldSettings(OptionWorldSettings);
                OptionSubsystem->ApplyWorldSettings();
                Output::send<LogLevel::Verbose>(STR("[YAPP] Enabling PvP Damage to Buildings and Players.\n"));
            }
            else
            {
                EnablePvPDamageToBuildings = false;
                Output::send<LogLevel::Verbose>(STR("[YAPP] Disabling PvP Damage to Buildings and Players.\n"));
            }
        });

        BeginScan();

        MapObject_CanProcessDamage_Hook = safetyhook::create_inline(reinterpret_cast<void*>(UPalMapObjectDamageReactionComponent::CanProcessDamage_Internal),
            reinterpret_cast<void*>(MapObjectDamageReactionComponent_CanProcessDamage));
    }
};


#define YETANOTHERPVPFIX_API __declspec(dllexport)
extern "C"
{
    YETANOTHERPVPFIX_API RC::CppUserModBase* start_mod()
    {
        return new YetAnotherPvPFix();
    }

    YETANOTHERPVPFIX_API void uninstall_mod(RC::CppUserModBase* mod)
    {
        delete mod;
    }
}
