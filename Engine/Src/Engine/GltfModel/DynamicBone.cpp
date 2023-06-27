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
		float ObjectScale{ 1.f };
		float Time{ 0.f };
		float Weight{ 0.f };
		float EndLength{ 0.f };

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

	void DynamicBone::Update()
	{
		if (Impl->RootNode == nullptr)
		{
			return;
		}
			
		double UpdateDelta = 1.0;
		UpdateParticle1(UpdateDelta);
		UpdateParticle2(UpdateDelta);

		ApplyParticlesToTransforms();
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

	void DynamicBone::InitTransform()
	{
		for (int i = 0; i < Impl->VecDynameicParticle.size(); ++i)
		{
			auto p = Impl->VecDynameicParticle[i];
			if (p->GPTransform != nullptr)
			{
				p->GPTransform->SetLocalPosition(p->LocalPosition);
				p->GPTransform->SetLocalRotation(p->LocalRotation);
			}
		}
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
		particle->ParentIndex = ParentIndex;
		
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

	std::string DynamicBone::GetID() const
	{
		return Impl->DBName;
	}

	void DynamicBone::UpdateParticle1(float UpdateDelta)
	{
		math::Vector3 Force = Impl->Gravity;
		math::Vector3 Dir = Impl->Gravity.Normalize();

		auto TmpGravity = math::Vector4(Impl->LocalGravity, 0.0f) * Impl->RootNode->GetLocalToWorld();
		math::Vector3 Gravity3 = math::Vector3(TmpGravity.x, TmpGravity.y, TmpGravity.z);

		math::Vector3 GravityF = Dir * (std::max)(Gravity3.Dot(Dir), 0.0f);
		Force -= GravityF;
		Force = (Force + Impl->Info.Force) * (Impl->ObjectScale * UpdateDelta);

		for (int32_t Index = 0; Index < Impl->VecDynameicParticle.size(); ++Index)
		{
			auto Dp = Impl->VecDynameicParticle[Index];
			if (Dp->ParentIndex >= 0)
			{
				// verlet integration
				math::Vector3 v = Dp->Position - Dp->PrevPosition; //自动添加的动态骨骼节点
				math::Vector3 rmove =  Impl->ObjectMove * Dp->Inert;
				Dp->PrevPosition = Dp->Position + rmove;
				float damping = Dp->Damping;

				Dp->Position += v * (1 - damping) + Force + rmove;
			}
			else
			{
				Dp->PrevPosition = Dp->Position; //胸部骨骼的节点位置应该进行改变，在每一帧的运动之后需要进行更新
				Dp->Position = Dp->GPTransform->GetWorldPosition(); //这个worldPosition是否需要在外面每一帧动作完成之后进行变化呢
			}
		}
	}

	void DynamicBone::UpdateParticle2(float UpdateDelta)
	{
		for (int i = 1; i < Impl->VecDynameicParticle.size(); ++i)
		{
			std::shared_ptr<DynamicParticle> p = Impl->VecDynameicParticle[i]; //添加的动态骨骼节点
			std::shared_ptr<DynamicParticle> p0 = Impl->VecDynameicParticle[p->ParentIndex]; //胸部节点lPectoral，也就是所谓的父节点

			float restLen;
			if (p->GPTransform != nullptr)
			{
				restLen = (p0->GPTransform->GetWorldPosition() - p->GPTransform->GetWorldPosition()).GetLength();
			}
			else
			{
				restLen = (p->EndOffset * p0->GPTransform->GetLocalToWorld() ).GetLength();
			}

			//TODO:keep shape
			float Stiffness = p->Stiffness;

			if (Stiffness > 0.0f || p->Stiffness > 0.0f)
			{
				math::Matrix4x4 TempMat = p0->GPTransform->GetLocalToWorld();
				TempMat[3][0] = p0->Position.x;
				TempMat[3][1] = p0->Position.y;
				TempMat[3][2] = p0->Position.z;

				math::Vector3 RestPos;
				if (p->GPTransform != nullptr)
				{
					auto TmpResult = math::Vector4(p->GPTransform->GetLocalPosition(), 1.0f) * TempMat;
					RestPos = math::Vector3(TmpResult.x, TmpResult.y, TmpResult.z);
				}
				else
				{
					//parentMatrix.transformPoint(p->_endOffset, &restPos);
					auto TmpResult = math::Vector4(p->EndOffset, 1.0f) * TempMat;
					RestPos = math::Vector3(TmpResult.x, TmpResult.y, TmpResult.z);
				}

				math::Vector3 d = RestPos - p->Position;
				p->Position += d * (p->Elasticity * UpdateDelta);

				if (Stiffness > 0.0f)
				{
					d = RestPos - p->Position;
					float len = d.GetLength();
					float maxlen = restLen * (1.0f - Stiffness) * 2.0f;
					if (len > maxlen)
					{
						p->Position += d * ((len - maxlen) / len);
					}
						
				}
			}

			// keep length
			math::Vector3 dd = p0->Position - p->Position;
			float leng = dd.GetLength();
			if (leng > 0)
			{
				p->Position += dd * ((leng - restLen) / leng);
			}
				
		}
	}

	void DynamicBone::ApplyParticlesToTransforms()
	{
		for (int i = 1; i < Impl->VecDynameicParticle.size(); ++i)
		{
			std::shared_ptr<DynamicParticle> p = Impl->VecDynameicParticle[i];
			std::shared_ptr<DynamicParticle> p0 = Impl->VecDynameicParticle[p->ParentIndex];

			if (p0->GPTransform->GetChildCount() <= 1)
			{
				math::Vector3 v;
				if (p->GPTransform != nullptr)
				{
					v = p->GPTransform->GetLocalPosition();
				}
				else
				{
					v = p->EndOffset;
				}

				math::Vector3 v2 = p->Position - p0->Position;

				auto  Tmp = math::Vector4(v.x, v.y, v.z, 0.0f) * p0->GPTransform->GetLocalToWorld();
				math::Vector3 TransPoint{ Tmp.x, Tmp.y, Tmp.z };

				auto Rot = math::Quaternion::FromToRotation(TransPoint, v2);
				p0->GPTransform->SetWorldRotation( math::Quaternion::Concatenate(Rot , p0->GPTransform->GetWorldRotation()));
			}

			if (p->GPTransform != nullptr)
			{
				p->GPTransform->SetWorldPosition(p->Position);
			}
		}
	}

}