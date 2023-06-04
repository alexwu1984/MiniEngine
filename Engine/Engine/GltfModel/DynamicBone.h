#pragma once
#include "GltfModel/DynamicBoneInfo.h"
#include "math/vector3.h"
#include "math/quaternion.h"

namespace Engine
{
	class DyTransformNode;

	struct DynamicBoneP;

	class DynamicBone
	{
	public:
		// 记录每个粒子(节点)的信息
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

			DyTransformNode* GPTransform = nullptr;
		};
	public:
		DynamicBone();
		~DynamicBone();

		// 去更新内部所有节点的信息
		void Update(float delta); //更新节点，每一帧都会进行更新，更新动态骨骼的位置
		void Init( DynamicBoneInfo& BoneInfo); //3.设置界面上设置的刚性、弹性等参数
		void InitParticle(DyTransformNode* RootTransform);  //1.初始化每个节点信息
		void InitTransform(); //2.初始化动态骨骼的局部位置和局部旋转
		void UpdateParticleParam(DynamicBoneInfo& Info);
		std::string GetID() const;
	private:
		void AppendParticles(DyTransformNode* TransformNode, int ParentIndex, float BoneLength);
		void UpdateParticleParam();
		void UpdateParticle1(float DeltaTime);
		void UpdateParticle2(float DeltaTime);
		void ApplyParticlesToTransforms();

	private:
		DynamicBoneP* Impl = nullptr;
	};
}