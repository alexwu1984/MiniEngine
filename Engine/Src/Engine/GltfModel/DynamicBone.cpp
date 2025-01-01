#include "GltfModel/DynamicBone.h"
#include "GltfModel/DyTransfromNode.h"

namespace Engine
{
	struct FDynamicBonePrivate
	{
		FDyTransformNode* RootNode = nullptr;
		math::Vector3 Gravity;
		math::Vector3 LocalGravity;
		math::Vector3 ObjectMove;
		math::Vector3 EndOffset;

		float BoneTotalLength{ 0 };
		float Time{ 0.f };
		float Weight{ 0.f };
		float EndLength{ 0.f };

		FDynamicBoneInfo Info;
		std::string DBName;
		std::vector <std::shared_ptr<FDynamicBone::DynamicParticle>> VecDynameicParticle;
	};

	FDynamicBone::FDynamicBone()
		:d_ptr(new FDynamicBonePrivate())
	{

	}

	FDynamicBone::~FDynamicBone()
	{
		delete d_ptr;
	}

	void FDynamicBone::Update()
	{
		C_P(FDynamicBone)
		if (!d->RootNode)
			return;
			
		UpdateParticle1();
		UpdateParticle2();

		ApplyParticlesToTransforms();
	}

	void FDynamicBone::Init(FDynamicBoneInfo& BoneInfo)
	{
		C_P(FDynamicBone)
		d->Info.Damping = BoneInfo.Damping;
		d->Info.Elasticity = BoneInfo.Elasticity;
		d->Info.Stiffness = BoneInfo.Stiffness;
		d->Info.Inert = BoneInfo.Inert;
		d->Info.Gravity = BoneInfo.Gravity;
		d->Info.EndLength = BoneInfo.EndLength;
		d->Info.Radius = BoneInfo.Radius;
		d->Info.Force = BoneInfo.Force;
		d->Info.EndOffset = BoneInfo.EndOffset;
		d->Info.UpdateScale = BoneInfo.UpdateScale;
		d->EndOffset = BoneInfo.EndOffset;
		d->Gravity = d->Info.Gravity;
	}

	void FDynamicBone::InitParticle(FDyTransformNode* RootTransform)
	{
		C_P(FDynamicBone)
		d->VecDynameicParticle.clear();
		d->DBName = RootTransform->GetID();
		d->RootNode = RootTransform;

		AppendParticles(d->RootNode, -1, 0.0f);
		UpdateParticleParam();
	}

	void FDynamicBone::InitTransform()
	{
		C_P(FDynamicBone)
		for (int i = 0; i < d->VecDynameicParticle.size(); ++i)
		{
			auto p = d->VecDynameicParticle[i];
			if (p->GPTransform != nullptr)
			{
				p->GPTransform->SetLocalPosition(p->LocalPosition);
				p->GPTransform->SetLocalRotation(p->LocalRotation);
			}
		}
	}

	void FDynamicBone::AppendParticles(FDyTransformNode* TransformNode, int ParentIndex, float BoneLength)
	{
		if (!TransformNode)
			return;
		C_P(FDynamicBone)
		std::shared_ptr<DynamicParticle> particle = std::make_shared<DynamicParticle>();
		particle->Position = particle->PrevPosition = TransformNode->GetWorldPosition();
		particle->GPTransform = TransformNode;
		particle->ParentIndex = ParentIndex;
		
		particle->LocalPosition = TransformNode->GetLocalPosition();
		particle->LocalRotation = TransformNode->GetLocalRotation();

		if (ParentIndex >= 0)
		{
			BoneLength += (d->VecDynameicParticle[ParentIndex]->GPTransform->GetWorldPosition() - particle->Position).GetLength();
			particle->BoneLength = BoneLength;
			d->BoneTotalLength = (std::max)(d->BoneTotalLength, BoneLength);
		}


		int32_t index = (int32_t)d->VecDynameicParticle.size();
		d->VecDynameicParticle.push_back(particle);
		FDyTransformNode* ChildNode = TransformNode->GetFirstChild();
		if (ChildNode) 
		{
			for (int i = 0; i < TransformNode->GetChildCount(); ++i)
				AppendParticles(ChildNode, index, BoneLength);
		}

	}

	void FDynamicBone::UpdateParticleParam()
	{
		C_P(FDynamicBone)
		if (!d->RootNode)
			return;
		auto Gravity = math::Vector4(d->Info.Gravity, 0.0f) * d->RootNode->GetWorldToLocal() ;
		d->LocalGravity = math::Vector3(Gravity.x, Gravity.y, Gravity.z);

		for (int Index = 0; Index < d->VecDynameicParticle.size(); ++Index)
		{
			auto p = d->VecDynameicParticle[Index];
			p->Damping = d->Info.Damping;
			p->Elasticity = d->Info.Elasticity;
			p->Stiffness = d->Info.Stiffness;
			p->Inert = d->Info.Inert;
			p->Radius = d->Info.Radius;
			p->UpdateScale = d->Info.UpdateScale;

			p->Damping = (std::min)((std::max)(p->Damping, 0.0f), 1.0f);
			p->Elasticity = (std::min)((std::max)(p->Elasticity, 0.0f), 1.0f);
			p->Stiffness = (std::min)((std::max)(p->Stiffness, 0.0f), 1.0f);
			p->Inert = (std::min)((std::max)(p->Inert, 0.0f), 1.0f);
			p->Radius = (std::max)(p->Radius, 0.0f);
		}
	}

	void FDynamicBone::UpdateParticleParam(FDynamicBoneInfo& Info)
	{
		C_P(FDynamicBone)
		d->Gravity = Info.Gravity;
		d->Info.Gravity = Info.Gravity;

		auto Gravity = math::Vector4(d->Info.Gravity, 0.0f) * d->RootNode->GetWorldToLocal();
		d->LocalGravity = math::Vector3(Gravity.x, Gravity.y, Gravity.z);

		for (int Index = 0; Index < d->VecDynameicParticle.size(); ++Index)
		{
			auto p = d->VecDynameicParticle[Index];
			p->Damping = Info.Damping;
			p->Elasticity = Info.Elasticity;
			p->Stiffness = Info.Stiffness;
			p->Inert = Info.Inert;
			p->Radius = Info.Radius;
			p->UpdateScale = Info.UpdateScale;

			p->Damping = (std::min)((std::max)(p->Damping, 0.0f), 1.0f);
			p->Elasticity = (std::min)((std::max)(p->Elasticity, 0.0f), 1.0f);
			p->Stiffness = (std::min)((std::max)(p->Stiffness, 0.0f), 1.0f);
			p->Inert = (std::min)((std::max)(p->Inert, 0.0f), 1.0f);
			p->Radius = (std::max)(p->Radius, 0.0f);
		}
	}

	std::string FDynamicBone::GetID() const
	{
		C_P(const FDynamicBone)
		return d->DBName;
	}

	void FDynamicBone::UpdateParticle1()
	{
		C_P(FDynamicBone)
		math::Vector3 Force = d->Gravity;
		math::Vector3 Dir = d->Gravity.Normalize();

		auto TmpGravity = math::Vector4(d->LocalGravity, 0.0f) * d->RootNode->GetLocalToWorld();
		math::Vector3 Gravity3 = math::Vector3(TmpGravity.x, TmpGravity.y, TmpGravity.z);

		math::Vector3 GravityF = Dir * (std::max)(Gravity3.Dot(Dir), 0.0f);
		Force -= GravityF;
		Force = (Force + d->Info.Force) * d->Info.UpdateScale;

		for (int32_t Index = 0; Index < d->VecDynameicParticle.size(); ++Index)
		{
			auto Dp = d->VecDynameicParticle[Index];
			if (Dp->ParentIndex >= 0)
			{
				// verlet integration
				math::Vector3 v = Dp->Position - Dp->PrevPosition;
				math::Vector3 rmove =  d->ObjectMove * Dp->Inert;
				Dp->PrevPosition = Dp->Position + rmove;
				float damping = Dp->Damping;

				Dp->Position += v * (1 - damping) + Force + rmove;
			}
			else
			{
				Dp->PrevPosition = Dp->Position;
				Dp->Position = Dp->GPTransform->GetWorldPosition(); 
			}
		}
	}

	void FDynamicBone::UpdateParticle2()
	{
		C_P(FDynamicBone)
		for (int i = 1; i < d->VecDynameicParticle.size(); ++i)
		{
			std::shared_ptr<DynamicParticle> p = d->VecDynameicParticle[i]; 
			std::shared_ptr<DynamicParticle> p0 = d->VecDynameicParticle[p->ParentIndex];

			float restLen;
			if (p->GPTransform != nullptr)
			{
				restLen = (p0->GPTransform->GetWorldPosition() - p->GPTransform->GetWorldPosition()).GetLength();
			}
			else
			{
				restLen = (p->EndOffset * p0->GPTransform->GetLocalToWorld() ).GetLength();
			}

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
					auto TmpResult = math::Vector4(p->EndOffset, 1.0f) * TempMat;
					RestPos = math::Vector3(TmpResult.x, TmpResult.y, TmpResult.z);
				}

				math::Vector3 Distance = RestPos - p->Position;
				p->Position += Distance * (p->Elasticity * d->Info.UpdateScale);

				if (Stiffness > 0.0f)
				{
					Distance = RestPos - p->Position;
					float Len = Distance.GetLength();
					float Maxlen = restLen * (1.0f - Stiffness) * 2.0f;
					if (Len > Maxlen)
					{
						p->Position += Distance * ((Len - Maxlen) / Len);
					}
						
				}
			}

			// keep length
			math::Vector3 Distance = p0->Position - p->Position;
			float Len = Distance.GetLength();
			if (Len > 0)
			{
				p->Position += Distance * ((Len - restLen) / Len);
			}
				
		}
	}

	void FDynamicBone::ApplyParticlesToTransforms()
	{
		C_P(FDynamicBone)
		for (int i = 1; i < d->VecDynameicParticle.size(); ++i)
		{
			std::shared_ptr<DynamicParticle> p = d->VecDynameicParticle[i];
			std::shared_ptr<DynamicParticle> p0 = d->VecDynameicParticle[p->ParentIndex];

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