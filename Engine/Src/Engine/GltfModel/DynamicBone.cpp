#include "GltfModel/DynamicBone.h"
#include "GltfModel/DyTransfromNode.h"

namespace Engine
{
	struct DynamicBoneP
	{
		// 由这个去驱动 一般就是跟着姿态矩阵走
		DyTransformNode* RootNode = nullptr;
		math::Vector3 Gravity;
		math::Vector3 LocalGravity;
		math::Vector3 ObjectMove;
		math::Vector3 EndOffset;

		float BoneTotalLength{ 0 };
		float ObjectScale{ 0.f };
		float Time{ 0.f };
		float Weight{ 0.f };
		float EndLength{ 0.f };
		// 我们默认30帧
		float UpdateRate{ 30.f };

		DynamicBoneInfo Info;
		std::string DBName;

		std::vector <std::shared_ptr<DynamicBone::DynamicParticle>> VecDynameicParticle;
	};

	DynamicBone::DynamicBone()
		:Impl(new DynamicBoneP())
	{

	}

	DynamicBone::~DynamicBone()
	{
		delete Impl;
	}

	void DynamicBone::Update(float delta)
	{

	}

	void DynamicBone::Init(DynamicBoneInfo& BoneInfo)
	{
		Impl->Info.Damping = BoneInfo.Damping;
		Impl->Info.Elasticity = BoneInfo.Elasticity;
		Impl->Info.Stiffness = BoneInfo.Stiffness;
		Impl->Info.Inert = BoneInfo.Inert;
		Impl->Info.Gravity = BoneInfo.Gravity;
		Impl->Info.EndLength = BoneInfo.EndLength;
		Impl->Info.Radius = BoneInfo.Radius;
		Impl->Info.Force = BoneInfo.Force;
		Impl->Info.EndOffset = BoneInfo.EndOffset;

		Impl->EndOffset = BoneInfo.EndOffset;

		Impl->Gravity = Impl->Info.Gravity;
	}

	void DynamicBone::InitParticle(DyTransformNode* RootTransform)
	{
		Impl->VecDynameicParticle.clear();
		Impl->DBName = RootTransform->GetID();
		Impl->RootNode = RootTransform;


		AppendParticles(Impl->RootNode, -1, 0.0f);
		UpdateParticleParam();
	}

	void DynamicBone::AppendParticles(DyTransformNode* TransformNode, int ParentIndex, float BoneLength)
	{
		if (!TransformNode)
		{
			return;
		}


		std::shared_ptr<DynamicParticle> particle = std::make_shared<DynamicParticle>();
		particle->Position = particle->PrevPosition = TransformNode->GetWorldPosition();
		particle->GPTransform = TransformNode;
		particle->ParentIndex = BoneLength;
		
		particle->LocalPosition = TransformNode->GetLocalPosition();
		particle->LocalRotation = TransformNode->GetLocalRotation();

		if (ParentIndex >= 0)
		{
			BoneLength += (Impl->VecDynameicParticle[ParentIndex]->GPTransform->GetWorldPosition() - particle->Position).GetLength();
			particle->BoneLength = BoneLength;
			Impl->BoneTotalLength = (std::max)(Impl->BoneTotalLength, BoneLength);
		}


		int32_t index = (int32_t)Impl->VecDynameicParticle.size();
		Impl->VecDynameicParticle.push_back(particle);
		DyTransformNode* ChildNode = TransformNode->GetFirstChild();
		if (ChildNode != nullptr) {
			for (int i = 0; i < TransformNode->GetChildCount(); ++i)
			{
				AppendParticles(ChildNode, index, BoneLength);
			}
		}

	}

	void DynamicBone::UpdateParticleParam()
	{
		// 没有绑定根节点
		if (Impl->RootNode == nullptr)
			return;
		// 先假设重力是0
		auto Gravity = math::Vector4(Impl->Info.Gravity, 0.0f) * Impl->RootNode->GetWorldToLocal() ;
		Impl->LocalGravity = math::Vector3(Gravity.x, Gravity.y, Gravity.z);

		for (int Index = 0; Index < Impl->VecDynameicParticle.size(); ++Index)
		{
			auto p = Impl->VecDynameicParticle[Index];
			p->Damping = Impl->Info.Damping;
			p->Elasticity = Impl->Info.Elasticity;
			p->Stiffness = Impl->Info.Stiffness;
			p->Inert = Impl->Info.Inert;
			p->Radius = Impl->Info.Radius;

			p->Damping = (std::min)((std::max)(p->Damping, 0.0f), 1.0f);
			p->Elasticity = (std::min)((std::max)(p->Elasticity, 0.0f), 1.0f);
			p->Stiffness = (std::min)((std::max)(p->Stiffness, 0.0f), 1.0f);
			p->Inert = (std::min)((std::max)(p->Inert, 0.0f), 1.0f);
			p->Radius = (std::max)(p->Radius, 0.0f);
		}
	}

	void DynamicBone::UpdateParticleParam(DynamicBoneInfo& Info)
	{
		Impl->Gravity = Info.Gravity;
		Impl->Info.Gravity = Info.Gravity;

		auto Gravity = math::Vector4(Impl->Info.Gravity, 0.0f) * Impl->RootNode->GetWorldToLocal();
		Impl->LocalGravity = math::Vector3(Gravity.x, Gravity.y, Gravity.z);

		for (int Index = 0; Index < Impl->VecDynameicParticle.size(); ++Index)
		{
			auto p = Impl->VecDynameicParticle[Index];
			p->Damping = Info.Damping;
			p->Elasticity = Info.Elasticity;
			p->Stiffness = Info.Stiffness;
			p->Inert = Info.Inert;
			p->Radius = Info.Radius;

			p->Damping = (std::min)((std::max)(p->Damping, 0.0f), 1.0f);
			p->Elasticity = (std::min)((std::max)(p->Elasticity, 0.0f), 1.0f);
			p->Stiffness = (std::min)((std::max)(p->Stiffness, 0.0f), 1.0f);
			p->Inert = (std::min)((std::max)(p->Inert, 0.0f), 1.0f);
			p->Radius = (std::max)(p->Radius, 0.0f);
		}
	}

	void DynamicBone::UpdateParticle1(float timevar)
	{
		math::Vector3 Force = Impl->Gravity;
		math::Vector3 Dir = Impl->Gravity.Normalize();

		auto Gravity = math::Vector4(Impl->LocalGravity, 0.0f) * Impl->RootNode->GetLocalToWorld();

		math::Vector3 rf = math::Vector3(Gravity.x, Gravity.y, Gravity.z);

		math::Vector3 pf = Dir * (std::max)(rf.Dot(Dir), 0.0f);
		Force -= pf;
		Force = (Force + Impl->Info.Force) * (Impl->ObjectScale * timevar);

		for (int i = 0; i < Impl->VecDynameicParticle.size(); ++i)
		{
			auto p = Impl->VecDynameicParticle[i];
			if (p->ParentIndex >= 0)
			{
				// verlet integration
				math::Vector3 v = p->Position - p->PrevPosition; //自动添加的动态骨骼节点
				math::Vector3 rmove =  Impl->ObjectMove * p->Inert;
				p->PrevPosition = p->Position + rmove;
				float damping = p->Damping;

				p->Position += v * (1 - damping) + Force + rmove;
			}
			else
			{
				p->PrevPosition = p->Position; //胸部骨骼的节点位置应该进行改变，在每一帧的运动之后需要进行更新
				p->Position = p->GPTransform->GetWorldPosition(); //这个worldPosition是否需要在外面每一帧动作完成之后进行变化呢
			}
		}
	}

	void DynamicBone::UpdateParticle2(float timevar)
	{

	}

}