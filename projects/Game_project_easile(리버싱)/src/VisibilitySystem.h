#pragma once
#include "GameWorld.h"
class VisibilitySystem final
{
public:
    bool IsVisible(const Hero& observer, const Hero& target, const Brush* brushes, int count) const
    {
        if (&observer == &target || !target.Alive() || target.Reveal() > 0.0F) { return true; }
        for (int index = 0; index < count; ++index)
        {
            if (brushes[index].Contains(target.Position())) { return brushes[index].Contains(observer.Position()); }
        }
        return Distance(observer.Position(), target.Position()) <= 520.0F;
    }
};
