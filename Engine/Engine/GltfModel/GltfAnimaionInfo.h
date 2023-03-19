#pragma once
#include "math/vector4.h"


namespace Engine
{
	//关键帧动画（Translation、Rotation、Scaling）信息
	struct AnimationKey
	{
		//时间戳
		float fTime = 0;
		//四维变化信息
		math::Vector4 value{};
	};

	enum AnimationChannelType
	{
		TARGET_ROTATE = 0,
		TARGET_SCALE,
		TARGET_TRANSLATE,
		TARGET_WEIGHT
	};
	//一个动画中某部分（对应某个Mesh、Bone）的信息
	struct AnimationChannelInfo
	{
		AnimationChannelInfo()
		{
			nNodeID = -1;
			eType = TARGET_ROTATE;
			pInputTime = NULL;
			pOutputAnimateValue = NULL;
			nKeyFrame = 0;
			nOutCount = 0;
		}
		void findKey(float during, int& nKeyL, int& nKeyR, float& alpha)
		{
			if (nKeyFrame < 1)
			{
				return;
			}
			int Index = -1;
			if (during < pInputTime[0])
			{
				nKeyL = 0;
				nKeyR = 0;
				alpha = 1.0;
				return;
			}

			if (during > pInputTime[nKeyFrame - 1])
			{
				nKeyL = -1;
				nKeyR = -1;
				return;
			}

			for (int i = 0; i < nKeyFrame - 1; i++)
			{
				if (pInputTime[i] <= during && pInputTime[i + 1] >= during)
				{
					nKeyL = i;
					nKeyR = i + 1;
					break;
				}
			}
			if (during >= pInputTime[nKeyFrame - 1])
			{
				nKeyL = nKeyFrame - 1;
				nKeyR = nKeyFrame - 1;
			}
			float ticks = pInputTime[nKeyR] - pInputTime[nKeyL];
			if (ticks < 0.001)
			{
				alpha = 1.0;
			}
			else
			{
				alpha = (during - pInputTime[nKeyL]) / ticks;
			}
		}
		uint32_t nNodeID;
		AnimationChannelType eType;
		float* pInputTime;
		float* pOutputAnimateValue;
		uint32_t nKeyFrame;
		uint32_t nOutCount;
	};
}