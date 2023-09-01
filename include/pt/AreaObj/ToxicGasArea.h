#pragma once

#include "Game/AreaObj/AreaObj.h"

namespace pt {
    class ToxicGasArea : public AreaObj {
    public: 
        ToxicGasArea(const char *pName);

        virtual void init(const JMapInfoIter &rIter);
        virtual void movement();
        virtual const char* getManagerName() const;

        u32 oxygenCount;
    };
}