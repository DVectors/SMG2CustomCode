#pragma once

#include "Actor/LiveActor/LiveActor.h"

class BegomanBase : public LiveActor {
public:
	BegomanBase(const char*);

	virtual void initAfterPlacement();
	virtual void appear();
	virtual void kill();
	virtual void control();
	virtual void calcAndSetBaseMtx();
	virtual void funcBegomanBase1() = 0;
	virtual void funcBegomanBase2() = 0;
	virtual void sub_8015F280(s32);
	virtual void sub_801620B0(s32);
	virtual s32 getKind() const = 0;
	virtual bool onTouchElectric(const JGeometry::TVec3<f32>&, const JGeometry::TVec3<f32>&);
	virtual void setNerveReturn() = 0;
	virtual void setNerveLaunch();
	virtual void setNerveWait() = 0;
	virtual bool addVelocityOnPushedFromElectricRail(const JGeometry::TVec3<f32>&, const JGeometry::TVec3<f32>&);
	virtual bool requestAttack();

	s32 _90;
	s32 _94;
	s32 _98;
	JGeometry::TVec3<f32> _9C;
	JGeometry::TVec3<f32> _A8;
	JGeometry::TVec3<f32> _B4;
	JGeometry::TVec3<f32> _C0;
	f32 _CC[4];
	f32 _DC[4];
	s32 _EC;
	s32 _F0;
	JGeometry::TVec3<f32> _F4;
	s32 _100;
	s32 _104;
	u8 _108;
	u8 _109;
};
