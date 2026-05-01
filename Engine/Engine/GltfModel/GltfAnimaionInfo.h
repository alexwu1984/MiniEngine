#pragma once
#include "math/vector3.h"
#include "math/vector4.h"
#include <unordered_map>
#include <vector>


namespace Engine
{
	// Keyframe data for translation, rotation, scaling, and weights.
	struct AnimationKey
	{
		// Timestamp.
		float fTime = 0;
		// Animated value.
		math::Vector4 value{};
	};

	enum AnimationChannelType
	{
		TARGET_ROTATE = 0,
		TARGET_SCALE,
		TARGET_TRANSLATE,
		TARGET_WEIGHT
	};
	// Animation channel targeting a node, mesh, or bone.
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
				alpha = 0.0f;
				return;
			}

			if (during > pInputTime[nKeyFrame - 1])
			{
				nKeyL = nKeyFrame - 1;
				nKeyR = nKeyFrame - 1;
				alpha = 1.0f;
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

	struct AnimationTRSInfo
	{
		math::Vector4 Rotation = math::Vector4(0, 0, 0, 1);
		bool EnableRotate = false;
		math::Vector3 Scale = math::Vector3(1, 1, 1);
		bool EnableScale = false;
		math::Vector3 Translate = math::Vector3(0, 0, 0);
		bool EnableTranslate = false;
	};

	struct AnimationBlendInfo
	{
		std::unordered_map<int32_t, AnimationTRSInfo> TransformMap;
		std::unordered_map<int32_t, std::vector<float>> WeightMap;
		int32_t AnimationIndex = -1;
	};
}