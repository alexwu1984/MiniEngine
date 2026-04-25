#include "GLTFModel/GltfAnimation.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfNode.h"
#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfSkeleton.h"
#include "math/vector4.h"
#include "math/quaternion.h"

using namespace math;

namespace Engine
{

	GltfAnimation::GltfAnimation(tinygltf::Model* gltfModel, GltfModel* Model)
		:GltfModelBase(gltfModel)
		,_Model(Model)
	{

	}

	GltfAnimation::~GltfAnimation()
	{

	}

	void GltfAnimation::InitAnimate(uint32_t AnimateIndex)
	{
		if (AnimateIndex >= _GltfModel->animations.size())
		{
			return;
		}
		auto& Animate = _GltfModel->animations[AnimateIndex];

		_AnimateName = Animate.name;

		for (int i = 0; i < Animate.channels.size(); i++)
		{
			auto& pChannel = Animate.channels[i];
			std::shared_ptr<AnimationChannelInfo> pInfo = std::make_shared<AnimationChannelInfo>();
			pInfo->nNodeID = pChannel.target_node;
			std::string sType = pChannel.target_path;
			if (sType == "translation")
			{
				pInfo->eType = TARGET_TRANSLATE;
			}
			else if (sType == "rotation")
			{
				pInfo->eType = TARGET_ROTATE;
			}
			else if (sType == "scale")
			{
				pInfo->eType = TARGET_SCALE;
			}
			else if (sType == "weights")
			{
				pInfo->eType = TARGET_WEIGHT;
			}
			auto sampleID = pChannel.sampler;
			auto Sample = Animate.samplers[sampleID];

			auto input = Sample.input;
			auto output = Sample.output;
			int type = 0;
			pInfo->pInputTime = (float*)Getdata(input, pInfo->nKeyFrame, type);
			pInfo->pOutputAnimateValue = (float*)Getdata(output, pInfo->nOutCount, type);
			auto maxVaue = _GltfModel->accessors[input].maxValues;
			_ChannelInfo.push_back(pInfo);
			if (maxVaue.size() == 1)
			{
				_AnimateAllTime = (std::max)(_AnimateAllTime * 1.0, maxVaue[0]);
			}
			if (pInfo->nKeyFrame > 0)
			{
				_AnimateAllTime = (std::max)(_AnimateAllTime, pInfo->pInputTime[pInfo->nKeyFrame - 1]);
			}

		}
	}

	float GltfAnimation::GetAnimationTime() const
	{
		return _AnimateAllTime;
	}

	bool GltfAnimation::HasModelAnimatie() const
	{
		return _hasModelAnimate;
	}

	void GltfAnimation::Play(float fSecond, GltfModel* Model)
	{
		int nChannels = _ChannelInfo.size();
		for (int i = 0; i < nChannels; i++)
		{
			auto pInfo = _ChannelInfo[i];
			if (pInfo->eType == TARGET_WEIGHT)
			{
				playBlendShape(fSecond, pInfo);
			}
			else
			{
				PlaySkeleton(fSecond, pInfo);
			}

		}
	}

	void GltfAnimation::PlaySkeleton(float fSecond, std::shared_ptr< AnimationChannelInfo> ChannelInfo)
	{
		int nNodeID = ChannelInfo->nNodeID;

		std::shared_ptr<GltfNodeInfo> pNodeInfo = _Model->RootNode()->GetNodeInfo(nNodeID);
		auto pBoneInfo = _Model->GetSkeleton() ? _Model->GetSkeleton()->GetBoneNodeByNodeId(nNodeID) : nullptr;
		int nKeyL = -1, nKeyR = -1;
		float alpha;
		ChannelInfo->findKey(fSecond, nKeyL, nKeyR, alpha);
		if (nKeyL >= 0)
		{
			if (ChannelInfo->eType == TARGET_ROTATE)
			{
				Vector4* pL = (Vector4*)ChannelInfo->pOutputAnimateValue;
				Vector4* pR = (Vector4*)ChannelInfo->pOutputAnimateValue;
				Quaternion Rotation = Quaternion::Slerp(Quaternion(pL[nKeyL]), Quaternion(pR[nKeyR]), alpha);
				if (pBoneInfo)
				{
					pBoneInfo->TargetRotation = Vector4(Rotation.x, Rotation.y, Rotation.z, Rotation.w);
				}
				else if (pNodeInfo)
				{
					pNodeInfo->Rotation = Rotation;
				}
			}
			else if (ChannelInfo->eType == TARGET_SCALE)
			{
				Vector3* pL = (Vector3*)ChannelInfo->pOutputAnimateValue;
				Vector3* pR = (Vector3*)ChannelInfo->pOutputAnimateValue;
				Vector3 Scale = pL[nKeyL] * (1.0f - alpha) + pR[nKeyR] * alpha;
				if (pBoneInfo)
				{
					pBoneInfo->TargetScale = Scale;
				}
				else if (pNodeInfo)
				{
					pNodeInfo->Scale = Scale;
				}
			}
			else if (ChannelInfo->eType == TARGET_TRANSLATE)
			{
				Vector3* pL = (Vector3*)ChannelInfo->pOutputAnimateValue;
				Vector3* pR = (Vector3*)ChannelInfo->pOutputAnimateValue;
				Vector3 Translate = pL[nKeyL] * (1.0f - alpha) + pR[nKeyR] * alpha;
				if (pBoneInfo)
				{
					pBoneInfo->TargetTranslate = Translate;
				}
				else if (pNodeInfo)
				{
					pNodeInfo->Translate = Translate;
				}
			}
		}
		_hasModelAnimate = true;
	}


	void GltfAnimation::playBlendShape(float fSecond, std::shared_ptr< AnimationChannelInfo> ChannelInfo)
	{
		uint32_t nNodeID = ChannelInfo->nNodeID;

		if (nNodeID >= _GltfModel->nodes.size())
		{
			return;
		}

		auto pNode = _GltfModel->nodes[nNodeID];
		if (pNode.mesh >= 0 && pNode.mesh < _Model->GetModelMesh().size())
		{
			if (ChannelInfo->nKeyFrame == 0)
			{
				return;
			}

			int nBlendShape = ChannelInfo->nOutCount / ChannelInfo->nKeyFrame;
			if (nBlendShape <= 0)
			{
				return;
			}
			int nKeyL = -1, nKeyR = -1;
			float alpha;
			ChannelInfo->findKey(fSecond, nKeyL, nKeyR, alpha);
			std::vector<float>vWeight(nBlendShape);
			if (nKeyL >= 0)
			{
				float* pL = ChannelInfo->pOutputAnimateValue + nKeyL * nBlendShape;
				float* pR = ChannelInfo->pOutputAnimateValue + nKeyR * nBlendShape;

				for (int i = 0; i < nBlendShape; i++)
				{
					vWeight[i] = pL[i] * (1.0f - alpha) + pR[i] * alpha;
				}
			}
			for (int i = 0; i < _Model->GetModelMesh().size(); i++)
			{
				if (_Model->GetModelMesh()[i]->GetNodeId() == nNodeID)
				{
					_Model->GetModelMesh()[i]->GenVertWithWeights(vWeight);
				}
			}
		}
	}

	bool GltfAnimation::GetBlendShapeWeights(float fSecond, AnimationBlendInfo& BlendInfo)
	{
		bool HasWeights = false;
		for (const auto& ChannelInfo : _ChannelInfo)
		{
			if (ChannelInfo->eType != TARGET_WEIGHT)
			{
				continue;
			}

			uint32_t nNodeID = ChannelInfo->nNodeID;
			if (nNodeID >= _GltfModel->nodes.size())
			{
				continue;
			}

			auto pNode = _GltfModel->nodes[nNodeID];
			if (pNode.mesh < 0)
			{
				continue;
			}

			if (ChannelInfo->nKeyFrame == 0)
			{
				continue;
			}

			int nBlendShape = ChannelInfo->nOutCount / ChannelInfo->nKeyFrame;
			if (nBlendShape <= 0)
			{
				continue;
			}

			int nKeyL = -1, nKeyR = -1;
			float alpha = 0.0f;
			ChannelInfo->findKey(fSecond, nKeyL, nKeyR, alpha);

			std::vector<float> vWeight(nBlendShape);
			if (nKeyL >= 0)
			{
				float* pL = ChannelInfo->pOutputAnimateValue + nKeyL * nBlendShape;
				float* pR = ChannelInfo->pOutputAnimateValue + nKeyR * nBlendShape;

				for (int i = 0; i < nBlendShape; i++)
				{
					vWeight[i] = pL[i] * (1.0f - alpha) + pR[i] * alpha;
				}
			}

			for (int i = 0; i < _Model->GetModelMesh().size(); i++)
			{
				if (_Model->GetModelMesh()[i]->GetNodeId() == nNodeID)
				{
					BlendInfo.WeightMap[nNodeID] = vWeight;
					HasWeights = true;
				}
			}
		}

		return HasWeights;
	}

	bool GltfAnimation::GetAnimationBlendInfo(float fSecond, AnimationBlendInfo& BlendInfo)
	{
		bool HasData = GetBlendShapeWeights(fSecond, BlendInfo);

		for (const auto& ChannelInfo : _ChannelInfo)
		{
			if (ChannelInfo->eType == TARGET_WEIGHT)
			{
				continue;
			}

			uint32_t nNodeID = ChannelInfo->nNodeID;
			if (ChannelInfo->nKeyFrame == 0)
			{
				continue;
			}

			int nKeyL = -1, nKeyR = -1;
			float alpha = 0.0f;
			ChannelInfo->findKey(fSecond, nKeyL, nKeyR, alpha);
			if (nKeyL < 0)
			{
				continue;
			}

			AnimationTRSInfo& TRSInfo = BlendInfo.TransformMap[nNodeID];
			if (ChannelInfo->eType == TARGET_ROTATE)
			{
				Vector4* pL = (Vector4*)ChannelInfo->pOutputAnimateValue;
				Vector4* pR = (Vector4*)ChannelInfo->pOutputAnimateValue;
				Quaternion Rotation = Quaternion::Slerp(Quaternion(pL[nKeyL]), Quaternion(pR[nKeyR]), alpha);
				TRSInfo.Rotation = Vector4(Rotation.x, Rotation.y, Rotation.z, Rotation.w);
				TRSInfo.EnableRotate = true;
				HasData = true;
			}
			else if (ChannelInfo->eType == TARGET_SCALE)
			{
				Vector3* pL = (Vector3*)ChannelInfo->pOutputAnimateValue;
				Vector3* pR = (Vector3*)ChannelInfo->pOutputAnimateValue;
				TRSInfo.Scale = pL[nKeyL] * (1.0f - alpha) + pR[nKeyR] * alpha;
				TRSInfo.EnableScale = true;
				HasData = true;
			}
			else if (ChannelInfo->eType == TARGET_TRANSLATE)
			{
				Vector3* pL = (Vector3*)ChannelInfo->pOutputAnimateValue;
				Vector3* pR = (Vector3*)ChannelInfo->pOutputAnimateValue;
				TRSInfo.Translate = pL[nKeyL] * (1.0f - alpha) + pR[nKeyR] * alpha;
				TRSInfo.EnableTranslate = true;
				HasData = true;
			}
		}

		return HasData;
	}

}