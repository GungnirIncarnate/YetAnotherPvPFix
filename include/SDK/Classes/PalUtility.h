#pragma once

#include <Unreal/UObject.hpp>

namespace RC::Unreal {
    class UScriptStruct;
}

namespace Palworld {
    class UPalOptionSubsystem;

    class UPalUtility : public RC::Unreal::UObject {
    public:
        static auto GetDefault() -> UPalUtility*;

        static auto GetOptionSubsystem(RC::Unreal::UObject* WorldContextObject) -> UPalOptionSubsystem*;
    private:
        static inline UPalUtility* Self = nullptr;
    };
}
