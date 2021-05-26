﻿#include "spack/Actor/Mogu.h"
#include "JGeometry/TUtil.h"
#include "Util/ActorAnimUtil.h"
#include "Util/ActorInitUtil.h"
#include "Util/ActorMovementUtil.h"
#include "Util/ActorSensorUtil.h"
#include "Util/ActorShadowUtil.h"
#include "Util/ActorStateUtil.h"
#include "Util/ActorSwitchUtil.h"
#include "Util/EffectUtil.h"
#include "Util/JMapUtil.h"
#include "Util/JointUtil.h"
#include "Util/LiveActorUtil.h"
#include "Util/MathUtil.h"
#include "Util/MtxUtil.h"
#include "Util/ObjUtil.h"
#include "Util/PlayerUtil.h"
#include "Util/SoundUtil.h"
#include "Util/StarPointerUtil.h"
#include "mtx.h"

Mogu::Mogu(const char* pName) : LiveActor(pName) {
    mAnimScaleCtrl = NULL;
    mStone = NULL;
    mHole = NULL;
    mFrontVec.set<f32>(0.0f, 0.0f, 1.0f);
    mUpVec.set<f32>(0.0f, 1.0f, 0.0f);
    mCalcStoneGravity = true;
    mIsCannonFleetSight = false;
}

void Mogu::init(const JMapInfoIter& rIter) {
    if (MR::isValidInfo(rIter)) {
        MR::initDefaultPos(this, rIter);
        MR::useStageSwitchWriteDead(this, rIter);
        MR::useStageSwitchReadA(this, rIter);
        MR::useStageSwitchAwake(this, rIter);

        bool useStoneGravity = false;
        MR::getJMapInfoArg0NoInit(rIter, &useStoneGravity);
        mCalcStoneGravity = !useStoneGravity;

        MR::getJMapInfoArg1NoInit(rIter, &mIsCannonFleetSight);
    }

    initModelManagerWithAnm("Mogu", NULL);

    MR::extractMtxYDir((Mtx4*)getBaseMtx(), &mUpVec);
    mGravity.set(-mUpVec);

    MR::connectToSceneEnemy(this);
    MR::declareStarPiece(this, 3);
    MR::declareCoin(this, 1);

    // Setup binder and sensors
    f32 scale = mScale.y;

    f32 binder = 160.0f * scale;
    initBinder(binder, binder, 0);

    initHitSensor(2);
    TVec3f senOffset(-55.0f, 0.0f, 13.0f);
    senOffset *= scale;
    MR::addHitSensorAtJointEnemy(this, "Body", "Head", 32, 150.0f, senOffset);

    initEffectKeeper(0, NULL, false);
    initSound(4, "Mogu", false, TVec3f(0.0f, 0.0f, 0.0f));

    MR::initStarPointerTarget(this, 100.0f, TVec3f(0.0f, 50.0f, 0.0f));

    initNerve(&NrvMogu::NrvSearch::sInstance, 0);

    MR::initShadowVolumeSphere(this, 60.0f * scale);
    MR::invalidateShadow(this, NULL);
    MR::initLightCtrl(this);

    mAnimScaleCtrl = new AnimScaleController(NULL);
    mBindStarPointer = new WalkerStateBindStarPointer(this, mAnimScaleCtrl);

    mStone = new MoguStone("投石用の石", "MoguStone");
    mStone->initWithoutIter();

    mHole = new ModelObj("巣穴モデル", "MoguHole", NULL, -2, -2, -2, false);
    mHole->mTranslation.set(mTranslation);
    mHole->mRotation.set(mRotation);
    mHole->initWithoutIter();
    MR::startBrk(mHole, "Normal"); // Required in SMG2, smh

    makeActorAppeared();
}

void Mogu::kill() {
    LiveActor::kill();

    if (MR::isValidSwitchDead(this))
        MR::onSwitchDead(this);

    MR::emitEffect(this, "Death");
    MR::startBck(mHole, "Down");
    MR::startLevelSound(this, "EmExplodeS", -1, -1, -1);
    MR::startLevelSound(this, "OjStarPieceBurst", -1, -1, -1);
}

void Mogu::endClipped() {
    LiveActor::endClipped();

    // Reset NrvTurn
    if (isNerve(&NrvMogu::NrvTurn::sInstance))
        setNerve(&NrvMogu::NrvTurn::sInstance);
}

void Mogu::control() {
    mAnimScaleCtrl->updateNerve();

    if (!isNerveTypeInactive() && mBindStarPointer->tryStartPointBind())
        setNerve(&NrvMogu::NrvDPDSwoon::sInstance);
}

void Mogu::calcAndSetBaseMtx() {
    Mtx mtxTR;
    MR::makeMtxUpFrontPos((TPositionMtx*)&mtxTR, mUpVec, mFrontVec, mTranslation);
    MR::setBaseTRMtx(this, (TPositionMtx&)mtxTR);
    MR::updateBaseScale(this, mAnimScaleCtrl);

    // Update wrench position
    if (isNerve(&NrvMogu::NrvThrow::sInstance) && MR::isLessStep(this, 47)) {
        Mtx4* mtxHand = (Mtx4*)MR::getJointMtx(this, "ArmR2");
        MR::extractMtxTrans(mtxHand, &mStone->mTranslation);
    }
}

void Mogu::attackSensor(HitSensor* pHit1, HitSensor* pHit2) {
    if (pHit1 == getSensor("Body")) {
        if ((isNerve(&NrvMogu::NrvDPDSwoon::sInstance) || !isNerveTypeInactive()) && MR::isSensorPlayer(pHit2))
            MR::sendMsgPush(pHit2, pHit1);
    }
}

u32 Mogu::receiveMsgPlayerAttack(u32 msg, HitSensor* pHit1, HitSensor* pHit2) {
    if (MR::isMsgStarPieceAttack(msg)) {
        if (isNerveTypeInactive())
            mAnimScaleCtrl->startHitReaction();
        else if (isNerve(&NrvMogu::NrvAppear::sInstance)
            || isNerve(&NrvMogu::NrvSearch::sInstance)
            || isNerve(&NrvMogu::NrvThrow::sInstance))
            setNerve(&NrvMogu::NrvSwoonStart::sInstance);
        return 1;
    }
    else {
        if (MR::isMsgJetTurtleAttack(msg) || MR::isMsgInvincibleAttack(msg)) {
            if (isNerve(&NrvMogu::NrvHideWait::sInstance)
                || isNerve(&NrvMogu::NrvHide::sInstance)
                || isNerve(&NrvMogu::NrvStampDeath::sInstance)
                || isNerve(&NrvMogu::NrvHitBlow::sInstance))
                return 0;
        }
        else if (!isNerve(&NrvMogu::NrvDPDSwoon::sInstance) && isNerveTypeInactive())
            return 0;
        else if (MR::isMsgPlayerHipDrop(msg) || MR::isMsgPlayerTrample(msg)) {
            setNerve(&NrvMogu::NrvStampDeath::sInstance);
            return 1;
        }
        else if (!MR::isMsgPlayerSpinAttackOrSupportTico(msg))
            return 0;

        return tryPunchHitted(pHit1, pHit2);
    }

	return 0;
}

void Mogu::exeHideWait() {
    TVec3f playerOffset;
    playerOffset.sub(*MR::getPlayerPos(), mTranslation);
    MR::normalizeOrZero(&playerOffset);

    // Is player above?
    if (mGravity.dot(playerOffset) >= -0.75f) {
        f32 distToPlayer = MR::calcDistanceToPlayer(this);

        if (MR::isGreaterStep(this, 120) && 400.0f < distToPlayer && distToPlayer < 2000.0f)
            setNerve(&NrvMogu::NrvAppear::sInstance);
    }
}

void Mogu::exeHide() {
    if (MR::isFirstStep(this)) {
        MR::startBck(this, "Hide");
        MR::startBck(mHole, "Hide");
        MR::startLevelSound(this, "EmMoguHide", -1, -1, -1);

        TVec3f playerOffset;
        playerOffset.sub(mTranslation, *MR::getPlayerCenterPos());
        f32 mag = PSVECMag((Vec*)&playerOffset);

        MR::vecKillElement(playerOffset, mGravity, &playerOffset);
        MR::normalizeOrZero(&playerOffset);
        f32 dot = MR::getPlayerVelocity()->dot(playerOffset);

        if (8.0f < dot || mag < 200.0f)
            MR::setBckRate(this, 1.5f);
    }

    if (MR::isActionEnd(this)) {
        MR::startLevelSound(this, "EmMoguholeClose", -1, -1, -1);
        setNerve(&NrvMogu::NrvHideWait::sInstance);
    }
}

void Mogu::exeAppear() {
    if (MR::isFirstStep(this)) {
        MR::startBck(this, "Appear");
        MR::startBck(mHole, "Open");
        MR::startLevelSound(this, "EmMoguholeOpen", -1, -1, -1);
        MR::startLevelSound(this, "EmMoguAppear", -1, -1, -1);

        TVec3f dirToPlayer;
        MR::calcVecToPlayerH(&dirToPlayer, this, NULL);
        MR::turnVecToVecRadian(&mFrontVec, mFrontVec, dirToPlayer, JGeometry::TUtil<f32>::PI(), mUpVec);
    }

    if (MR::isGreaterStep(this, 30) && isNearPlayerHipDrop())
        setNerve(&NrvMogu::NrvSwoonStart::sInstance);
    else if (MR::calcDistanceToPlayer(this) < 400.0f || isPlayerExistUp())
        setNerve(&NrvMogu::NrvHide::sInstance);
    else if (MR::isActionEnd(this))
        setNerve(&NrvMogu::NrvSearch::sInstance);
}

void Mogu::exeSearch() {
    if (MR::isFirstStep(this)) {
        if (isNerve(&NrvMogu::NrvTurn::sInstance))
            MR::startBck(this, "Turn");
        else
            MR::startBck(this, "Wait");
    }

    if (isNearPlayerHipDrop())
        setNerve(&NrvMogu::NrvSwoonStart::sInstance);
    else {
        if (isNerve(&NrvMogu::NrvTurn::sInstance)) {
            TVec3f dirToPlayer;
            MR::calcVecToPlayerH(&dirToPlayer, this, NULL);
            MR::turnVecToVecRadian(&mFrontVec, mFrontVec, dirToPlayer, 0.03f, mUpVec);
        }

        f32 distToPlayer = MR::calcDistanceToPlayer(this);
        f32 sight = mIsCannonFleetSight ? 1500.0f : 900.0f;

        // Hide if too close to player, too far from player or if player is above
        if (distToPlayer < 400.0f || isPlayerExistUp() || 2000.0f < distToPlayer)
            setNerve(&NrvMogu::NrvHide::sInstance);
        // Turn to player if in sight
        else if (isNerve(&NrvMogu::NrvSearch::sInstance) && distToPlayer < sight) {
            if (!MR::isValidSwitchA(this) || MR::isValidSwitchA(this))
                setNerve(&NrvMogu::NrvTurn::sInstance);
        }
        // Search player if out of sight
        else if (isNerve(&NrvMogu::NrvTurn::sInstance) && sight < distToPlayer)
            setNerve(&NrvMogu::NrvSearch::sInstance);
        // Throw wrench if in direct sight
        else if (!MR::isValidSwitchA(this) || MR::isValidSwitchA(this)) {
            if (MR::isInSightFanPlayer(this, mFrontVec, sight, 10.0f, 90.0f)) {
                if (MR::isGreaterStep(this, 45) && MR::isDead(mStone))
                    setNerve(&NrvMogu::NrvThrow::sInstance);
            }
        }
    }
}

void Mogu::exeThrow() {
    if (MR::isFirstStep(this)) {
        MR::startBck(this, "Throw");
        mStone->appear();
        MR::startBck(mStone, "Rotate");
        MR::startLevelSound(this, "EmMoguTakeItem", -1, -1, -1);
    }

    if (isNearPlayerHipDrop())
        setNerve(&NrvMogu::NrvSwoonStart::sInstance);
    else {
        if (MR::isStep(this, 47)) {
            TVec3f upVec;
            MR::calcUpVec(&upVec, this);

            TVec3f playerOffset;
            playerOffset.sub(*MR::getPlayerCenterPos(), mStone->mTranslation);

            TVec3f dirToPlayer;
            MR::vecKillElement(playerOffset, upVec, &dirToPlayer);

            f32 dot = upVec.dot(playerOffset);
            TVec3f upVecScaled;
            upVecScaled.scale(dot, upVec);

            f32 mag = PSVECMag((Vec*)&dirToPlayer);
            TVec3f frontVecScaled;
            frontVecScaled.scale(mag, mFrontVec);

            TVec3f throwDir;
            throwDir.add(frontVecScaled, upVecScaled);

            mStone->emit(mCalcStoneGravity, mStone->mTranslation, throwDir, 15.0f);
            MR::startLevelSound(this, "EmMoguThrow", -1, -1, -1);
        }

        f32 distToPlayer = MR::calcDistanceToPlayer(this);

        if (distToPlayer < 400.0f || isPlayerExistUp() || 2000.0f < distToPlayer)
            setNerve(&NrvMogu::NrvHide::sInstance);
        else if (MR::isActionEnd(this))
            setNerve(&NrvMogu::NrvSearch::sInstance);
    }
}

void Mogu::tearDownThrow() {
    if (mStone->isTaken())
        mStone->kill();
}

void Mogu::exeSwoonStart() {
    if (MR::isFirstStep(this)) {
        MR::startLevelSound(this, "EmMoguTurnover", -1, -1, -1);
        MR::startLevelSound(this, "EvMoguTurnover", -1, -1, -1);
        MR::startBck(this, "SwoonStart");

        if (!MR::isBckPlaying(mHole, "Open"))
            MR::startBck(mHole, "Break");
    }

    MR::startLevelSound(this, "EmLvSwoonS", -1, -1, -1);

    if (MR::isActionEnd(this))
        setNerve(&NrvMogu::NrvSwoon::sInstance);
}

void Mogu::exeSwoon() {
    if (MR::isFirstStep(this))
        MR::startBck(this, "Swoon");

    MR::startLevelSound(this, "EmLvSwoonS", -1, -1, -1);

    if (isNearPlayerHipDrop())
        setNerve(&NrvMogu::NrvHipDropReaction::sInstance);
    else if (MR::isGreaterStep(this, 120))
        setNerve(&NrvMogu::NrvSwoonEnd::sInstance);
}

void Mogu::exeSwoonEnd() {
    if (MR::isFirstStep(this))
        MR::startBck(this, "SwoonEnd");

    MR::startLevelSound(this, "EmLvMoguSwoonRecover", -1, -1, -1);

    if (MR::isActionEnd(this)) {
        MR::startBck(mHole, "Close");
        MR::startLevelSound(this, "EmMoguholeClose", -1, -1, -1);
        setNerve(&NrvMogu::NrvHideWait::sInstance);
    }
}

void Mogu::exeHipDropReaction() {
    if (MR::isFirstStep(this))
        MR::startBck(this, "HipDropReaction");

    if (MR::isActionEnd(this))
        setNerve(&NrvMogu::NrvSwoon::sInstance);
}

void Mogu::exeStampDeath() {
    if (MR::isFirstStep(this)) {
        MR::startBck(this, "Down");
        MR::startBck(mHole, "Close");
        MR::startLevelSound(this, "EmStompedS", -1, -1, -1);
    }

    if (MR::isGreaterStep(this, 60)) {
        kill();
        MR::appearCoinPop(this, mTranslation, 1);
    }
}

void Mogu::exeHitBlow() {
    if (MR::isFirstStep(this)) {
        MR::startBck(this, "PunchDown");
        MR::startBck(mHole, "Close");
        MR::startLevelSound(this, "EmMoguholeClose", -1, -1, -1);
        MR::startBlowHitSound(this);
        MR::validateShadow(this, NULL);

        TVec3f dirToPlayer;
        MR::calcVecToPlayerH(&dirToPlayer, this, NULL);
        MR::turnVecToVecRadian(&mFrontVec, mFrontVec, dirToPlayer, JGeometry::TUtil<f32>::PI(), mUpVec);
    }

    MR::applyVelocityDampAndGravity(this, 2.0f, 0.8f, 0.98f, 0.98f, 1.0f);

    if (MR::isGreaterStep(this, 20)) {
        kill();
        MR::appearStarPiece(this, mTranslation, 3, 10.0f, 40.0f, false);
    }
}

void Mogu::exeDPDSwoon() {
    if (MR::isFirstStep(this)) {
        mAnimScaleCtrl->startDpdHitVibration();
    }

    MR::startDPDFreezeLevelSound(this);
    MR::updateActorStateAndNextNerve(this, (ActorStateBaseInterface*)mBindStarPointer, &NrvMogu::NrvSearch::sInstance);
}

void Mogu::endDPDSwoon() {
    mAnimScaleCtrl->startAnim();
}

bool Mogu::isPlayerExistUp() const {
    TVec3f playerOffset;
    playerOffset.sub(*MR::getPlayerCenterPos(), mTranslation);

    if (mUpVec.dot(playerOffset) < 0.0f)
        return false;

    MR::vecKillElement(playerOffset, *MR::getPlayerGravity(), &playerOffset);
    f32 mag = PSVECMag((Vec*)&playerOffset);

    return mag < 400.0f;
}

bool Mogu::isNearPlayerHipDrop() const {
    if (MR::isPlayerHipDropLand()) {
        f32 distToPlayer = MR::calcDistanceToPlayer(this);

        if (130.0f < distToPlayer && distToPlayer < 1500.0f)
            return true;
    }

	return false;
}

bool Mogu::isNerveTypeInactive() const {
    if (isNerve(&NrvMogu::NrvHideWait::sInstance)
        || isNerve(&NrvMogu::NrvHide::sInstance)
        || isNerve(&NrvMogu::NrvStampDeath::sInstance)
        || isNerve(&NrvMogu::NrvHitBlow::sInstance)
        || isNerve(&NrvMogu::NrvDPDSwoon::sInstance))
        return true;
    return false;
}

bool Mogu::tryPunchHitted(HitSensor* pHit1, HitSensor* pHit2) {
    TVec3f offBetweenSensors;
    offBetweenSensors.sub(pHit2->_4, pHit1->_4);

    MR::vecKillElement(offBetweenSensors, mGravity, &offBetweenSensors);
    MR::normalizeOrZero(&offBetweenSensors);

    if (MR::isNearZero(offBetweenSensors, 0.001f))
        setNerve(&NrvMogu::NrvStampDeath::sInstance);
    else {
        offBetweenSensors *= 30.0f;
        offBetweenSensors -= mGravity * 50.0f;
        mVelocity.set(offBetweenSensors);

        if (MR::isOnGround(this))
            mTranslation -= mGravity * 5.0f;

        setNerve(&NrvMogu::NrvHitBlow::sInstance);
    }

	return true;
}

namespace NrvMogu {
    void NrvHideWait::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeHideWait();
    }

    void NrvHide::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeHide();
    }

    void NrvAppear::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeAppear();
    }

    void NrvSearch::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeSearch();
    }

    void NrvTurn::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeSearch();
    }

    void NrvThrow::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeThrow();
    }

    void NrvThrow::executeOnEnd(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->tearDownThrow();
    }

    void NrvSwoonStart::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeSwoonStart();
    }

    void NrvSwoonEnd::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeSwoonEnd();
    }

    void NrvSwoon::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeSwoon();
    }

    void NrvHipDropReaction::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeHipDropReaction();
    }

    void NrvStampDeath::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeStampDeath();
    }

    void NrvHitBlow::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeHitBlow();
    }

    void NrvDPDSwoon::execute(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->exeDPDSwoon();
    }

    void NrvDPDSwoon::executeOnEnd(Spine* pSpine) const {
        Mogu* pActor = (Mogu*)pSpine->mExecutor;
        pActor->endDPDSwoon();
    }

    NrvHideWait(NrvHideWait::sInstance);
    NrvHide(NrvHide::sInstance);
    NrvAppear(NrvAppear::sInstance);
    NrvSearch(NrvSearch::sInstance);
    NrvTurn(NrvTurn::sInstance);
    NrvThrow(NrvThrow::sInstance);
    NrvSwoonStart(NrvSwoonStart::sInstance);
    NrvSwoonEnd(NrvSwoonEnd::sInstance);
    NrvSwoon(NrvSwoon::sInstance);
    NrvHipDropReaction(NrvHipDropReaction::sInstance);
    NrvStampDeath(NrvStampDeath::sInstance);
    NrvHitBlow(NrvHitBlow::sInstance);
    NrvDPDSwoon(NrvDPDSwoon::sInstance);
};
