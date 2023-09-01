#include "pt/AreaObj/ToxicGasArea.h"
#include "Game/Util/ObjUtil.h"
#include "Game/Util/PlayerUtil.h"
#include "Game/Player/MarioHolder.h"
#include "Game/Player/MarioActor.h"

namespace pt {
    ToxicGasArea::ToxicGasArea(const char *pName) : AreaObj(pName) {
        oxygenCount = 8;
    }

    void ToxicGasArea::init(const JMapInfoIter &rIter) {
		AreaObj::init(rIter);
		MR::connectToSceneAreaObj(this);
	}

    //Access decrease oxygen method in the game memory
    extern "C" void decOxygen__9MarioSwimFUs(MarioActor *, u16);

    void ToxicGasArea::movement() {
        MarioHolder *mh = MR::getMarioHolder();
        MarioActor *ma = mh->getMarioActor();

        if (isInVolume(*MR::getPlayerPos())) {
            // Decrease oxygen while player is in this area - Set to 1 for normal decrease speed
            decOxygen__9MarioSwimFUs(ma, 1);
        } else {
            MR::incPlayerOxygen(oxygenCount);
        }
    }

    const char* ToxicGasArea::getManagerName() const {
		return "SwitchArea";
	}
}