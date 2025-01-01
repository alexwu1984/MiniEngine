#pragma once
#include "GltfModel/DynamicBoneInfo.h"
#include "math/vector3.h"
#include "math/quaternion.h"

namespace Engine
{
	class FDyTransformNode;

	struct DynamicBonePrivate;

	class FDynamicBone
	{
	public:
		struct DynamicParticle
		{
			math::Vector3  LocalPosition;
			math::Quaternion LocalRotation;
			int32_t ParentIndex{-1};
			float  Damping{0.f};
			float  Elasticity{0.f};
			float  Stiffness{0.f};
			float  Inert{0.f};
			float  Radius{0.f};
			float  BoneLength{0.f};

			math::Vector3 Position;
			math::Vector3 PrevPosition;
			math::Vector3 EndOffset;

			FDyTransformNode* GPTransform = nullptr;
		};
	public:
		FDynamicBone();
		~FDynamicBone();

		// 去更新内部所有节点的信息
		void Update(); //更新节点，每一帧都会进行更新，更新动态骨骼的位置
		void Init( FDynamicBoneInfo& BoneInfo); //3.设置界面上设置的刚性、弹性等参数
		void InitParticle(FDyTransformNode* RootTransform);  //1.初始化每个节点信息
		void InitTransform(); //2.初始化动态骨骼的局部位置和局部旋转
		void UpdateParticleParam(FDynamicBoneInfo& Info);
		std::string GetID() const;
	private:
		void AppendParticles(FDyTransformNode* TransformNode, int ParentIndex, float BoneLength);
		void UpdateParticleParam();
		void UpdateParticle1(float UpdateDelta);
		void UpdateParticle2(float UpdateDelta);
		void ApplyParticlesToTransforms();

	private:
		DynamicBonePrivate* Impl = nullptr;
	};
}