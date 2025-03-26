#pragma once

#include "Unreal/UObject.hpp"

namespace Palworld {
    class UPalOptionSubsystem : public RC::Unreal::UObject {
    public:
        void ApplyWorldSettings();

        void SetOptionWorldSettings(void* SettingsDataPtr);
    };
}