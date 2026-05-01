#include "GltfModel/GltfAnimationManager.h"
#include "GltfModel/GltfAnimation.h"
#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfNode.h"
#include "GltfModel/GltfSkeleton.h"
#include "math/quaternion.h"
#include <algorithm>
#include <cmath>

namespace Engine 
{
	namespace
	{
		math::Vector4 BlendRotation(const math::Vector4& Src, const math::Vector4& Dst, float Alpha)
		{
			math::Quaternion Rotation = math::Quaternion::Slerp(math::Quaternion(Src), math::Quaternion(Dst), Alpha);
			return math::Vector4(Rotation.x, Rotation.y, Rotation.z, Rotation.w);
		}

		void ApplyTransform(GltfModel* Model, int32_t NodeID, const AnimationTRSInfo& TRSInfo)
		{
			auto Skeleton = Model->GetSkeleton();
			auto BoneNode = Skeleton ? Skeleton->GetBoneNodeByNodeId(NodeID) : nullptr;
			if (BoneNode)
			{
				if (TRSInfo.EnableRotate)
					BoneNode->TargetRotation = TRSInfo.Rotation;
				if (TRSInfo.EnableScale)
					BoneNode->TargetScale = TRSInfo.Scale;
				if (TRSInfo.EnableTranslate)
					BoneNode->TargetTranslate = TRSInfo.Translate;
				return;
			}

			auto NodeInfo = Model->RootNode()->GetNodeInfo(NodeID);
			if (!NodeInfo)
			{
				return;
			}

			if (TRSInfo.EnableRotate)
				NodeInfo->Rotation = math::Quaternion(TRSInfo.Rotation);
			if (TRSInfo.EnableScale)
				NodeInfo->Scale = TRSInfo.Scale;
			if (TRSInfo.EnableTranslate)
				NodeInfo->Translate = TRSInfo.Translate;
		}
	}

	GltfAnimationManager::GltfAnimationManager(tinygltf::Model* gltfModel, GltfModel* Model)
		:_gltfModel(gltfModel)
		,_Model(Model)
	{

	}

	GltfAnimationManager::~GltfAnimationManager()
	{

	}

	void GltfAnimationManager::InitAnimation()
	{
		for (size_t index = 0; index < _gltfModel->animations.size(); ++index)
		{
			std::shared_ptr<GltfAnimation> Animation = std::make_shared<GltfAnimation>(_gltfModel,_Model);
			Animation->InitAnimate(index);
			_Animations.push_back(Animation);

			_AnimationAllTime = (std::max)(_AnimationAllTime, Animation->GetAnimationTime());
			_BlendShapeTimelineAllTime += Animation->GetAnimationTime();
		}

		if (_Animations.size() > 1)
		{
			_BlendShapeTimelineAllTime -= _BlendShapeBlendTime * (_Animations.size() - 1);
			_BlendShapeTimelineAllTime = (std::max)(0.0f, _BlendShapeTimelineAllTime);
		}

	}

	bool GltfAnimationManager::Play(float Second)
	{
		if (_Animations.empty())
		{
			return false;
		}

		float timelineAllTime = _AnimationAllTime;
		if (timelineAllTime < 0.001f)
		{
			return false;
		}

		float during = std::fmod(Second, timelineAllTime);
		if (during < 0.0f)
		{
			during += timelineAllTime;
		}

		//during = frameCount * (1000.0f / 40.0f) / 1000.0f;
		_FrameCount++;

		for (int i = 0; i < _Animations.size(); i++)
		{
			_Animations[i]->Play(during, _Model);
			if (_Animations[i]->HasModelAnimatie())
			{
				_hasModelAnimate = true;
			}
		}
		return _hasModelAnimate;
	}

	bool GltfAnimationManager::ApplyAnimationTimeline(float Second)
	{
		if (_Animations.size() < 2 || _BlendShapeTimelineAllTime < 0.001f)
		{
			return false;
		}

		float during = (std::max)(0.0f, (std::min)(Second, _BlendShapeTimelineAllTime));

		AnimationBlendInfo srcBlendInfo;
		AnimationBlendInfo dstBlendInfo;
		bool foundSrc = false;
		float currentStartTime = 0.0f;
		float dstStartTime = 0.0f;

		for (int i = 0; i < _Animations.size(); ++i)
		{
			float animTime = _Animations[i]->GetAnimationTime();
			float startTime = (i == 0) ? 0.0f : currentStartTime - _BlendShapeBlendTime;
			float endTime = startTime + animTime;

			if (during >= startTime && during <= endTime)
			{
				AnimationBlendInfo blendInfo;
				blendInfo.AnimationIndex = i;
				_Animations[i]->GetAnimationBlendInfo(during - startTime, blendInfo);

				if (!foundSrc)
				{
					srcBlendInfo = std::move(blendInfo);
					foundSrc = true;
				}
				else
				{
					dstBlendInfo = std::move(blendInfo);
					dstStartTime = startTime;
					break;
				}
			}

			currentStartTime = endTime;
		}

		if (srcBlendInfo.WeightMap.empty())
		{
			if (srcBlendInfo.TransformMap.empty())
			{
				return false;
			}
		}

		for (const auto& SrcTransform : srcBlendInfo.TransformMap)
		{
			AnimationTRSInfo Result = SrcTransform.second;
			auto DstIt = dstBlendInfo.TransformMap.find(SrcTransform.first);
			if (DstIt != dstBlendInfo.TransformMap.end())
			{
				float alpha = (during - dstStartTime) / _BlendShapeBlendTime;
				alpha = (std::max)(0.0f, (std::min)(1.0f, alpha));

				if (Result.EnableRotate && DstIt->second.EnableRotate)
				{
					Result.Rotation = BlendRotation(Result.Rotation, DstIt->second.Rotation, alpha);
				}
				if (Result.EnableScale && DstIt->second.EnableScale)
				{
					Result.Scale = Result.Scale * (1.0f - alpha) + DstIt->second.Scale * alpha;
				}
				if (Result.EnableTranslate && DstIt->second.EnableTranslate)
				{
					Result.Translate = Result.Translate * (1.0f - alpha) + DstIt->second.Translate * alpha;
				}
			}

			ApplyTransform(_Model, SrcTransform.first, Result);
		}

		for (const auto& Mesh : _Model->GetModelMesh())
		{
			if (!Mesh)
			{
				continue;
			}

			auto srcIt = srcBlendInfo.WeightMap.find(Mesh->GetNodeId());
			if (srcIt == srcBlendInfo.WeightMap.end())
			{
				continue;
			}

			std::vector<float> weights = srcIt->second;
			auto dstIt = dstBlendInfo.WeightMap.find(Mesh->GetNodeId());
			if (dstIt != dstBlendInfo.WeightMap.end() && dstIt->second.size() == weights.size())
			{
				float alpha = (during - dstStartTime) / _BlendShapeBlendTime;
				alpha = (std::max)(0.0f, (std::min)(1.0f, alpha));
				for (size_t i = 0; i < weights.size(); ++i)
				{
					weights[i] = weights[i] * (1.0f - alpha) + dstIt->second[i] * alpha;
				}
			}

			Mesh->GenVertWithWeights(weights);
		}

		return true;
	}

	bool GltfAnimationManager::HasAnimation() const
	{
		return _hasModelAnimate;
	}

}
