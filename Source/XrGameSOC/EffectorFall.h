#pragma once

#include "../XrEngine/Effector.h"

// ���������� ����� �������
class CEffectorFall : public CEffectorCam
{
	float	fPower;
	float	fPhase;
public:
	virtual	BOOL	Process(Fvector &p, Fvector &d, Fvector &n, float& fFov, float& fFar, float& fAspect);

	CEffectorFall(float power, float life_time=1);
	virtual ~CEffectorFall();
};
