#include "GLTFModel/GltfAnimation.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfNode.h"
#include "GltfModel/GltfMesh.h"
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
		int nKeyL = -1, nKeyR = -1;
		float alpha;
		ChannelInfo->findKey(fSecond, nKeyL, nKeyR, alpha);
		if (nKeyL >= 0)
		{
			if (ChannelInfo->eType == TARGET_ROTATE)
			{
				Vector4* pL = (Vector4*)ChannelInfo->pOutputAnimateValue;
				Vector4* pR = (Vector4*)ChannelInfo->pOutputAnimateValue;
				//pBoneInfo->TargetRotation = pL[nKeyL] * (1.0 - alpha) + pR[nKeyR] * alpha;
				//CC3DUtils::QuaternionInterpolate(pNodeInfo->Rotation, pL[nKeyL], pR[nKeyR], alpha);
				pNodeInfo->Rotation = Quaternion::Lerp(Quaternion(pL[nKeyL]), Quaternion(pR[nKeyR]), alpha);
			}
			else if (ChannelInfo->eType == TARGET_SCALE)
			{
				Vector3* pL = (Vector3*)ChannelInfo->pOutputAnimateValue;
				Vector3* pR = (Vector3*)ChannelInfo->pOutputAnimateValue;
				pNodeInfo->Scale = pL[nKeyL] * (1.0f - alpha) + pR[nKeyR] * alpha;
			}
			else if (ChannelInfo->eType == TARGET_TRANSLATE)
			{
				Vector3* pL = (Vector3*)ChannelInfo->pOutputAnimateValue;
				Vector3* pR = (Vector3*)ChannelInfo->pOutputAnimateValue;
				pNodeInfo->Translate = pL[nKeyL] * (1.0f - alpha) + pR[nKeyR] * alpha;
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
			int nBlendShape = ChannelInfo->nOutCount / ChannelInfo->nKeyFrame;
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

}