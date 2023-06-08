#include "GltfModel/GltfModel.h"
#include "core/strings.h"
#include "GltfModel/GltfNode.h"
#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMaterial.h"
#include "GltfModel/GltfAnimationManager.h"
#include "GltfModel/GltfSkeleton.h"
#include "GltfModel/GltfModelConfig.h"

namespace Engine
{
	using namespace math;

	struct GltfModelP
	{
		tinygltf::TinyGLTF GltfCtx;
		tinygltf::Model GltfMode;
		std::shared_ptr<GltfNode> RootNode;
		std::vector<std::shared_ptr<GltfMesh>> ModelMesh;
		std::shared_ptr<GltfSkeleton> Skeleton;
		bool HasSkin = false;
		AABB3  ModelBox;
		std::shared_ptr<GltfAnimationManager> AnimationMgr;
		std::shared_ptr< GltfModelConfig> Config;
	};

	GltfModel::GltfModel()
		:Impl(std::make_shared<GltfModelP>())
	{

	}

	GltfModel::~GltfModel()
	{

	}

	bool GltfModel::Load(const std::wstring& FileName, std::shared_ptr< GltfModelConfig> Config)
	{
		Impl->Config = Config;
		std::string err;
		std::string warn;
		std::string utf8FileName = core::ucs2_u8(FileName);

		if (FileName.find(L".gltf") != std::wstring::npos)
		{
			if (Impl->GltfCtx.LoadASCIIFromFile(&Impl->GltfMode, &err, &warn, utf8FileName))
			{
				LoadNode();
				LoadMesh();
				LoadAnimate();
				LoadSkeleton();

				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			if (Impl->GltfCtx.LoadBinaryFromFile(&Impl->GltfMode, &err, &warn, utf8FileName))
			{
				LoadNode();
				LoadMesh();
				LoadAnimate();
				LoadSkeleton();

				return true;
			}
			else
			{
				return false;
			}
		}
	}

	void GltfModel::UpdateNode()
	{
		if (!Impl->RootNode)
		{
			return;
		}
		for (int i = 0; i < Impl->ModelMesh.size(); i++)
		{
			auto NodeInfo = Impl->RootNode->GetNodeInfo(Impl->ModelMesh[i]->GetNodeId());
			if (NodeInfo)
			{
				Impl->RootNode->UpdateNodeParent(NodeInfo);
				Impl->ModelMesh[i]->SetMeshMat(NodeInfo->FinalMeshMat);
			}
		}
	}

	std::vector<std::shared_ptr<GltfMesh>>& GltfModel::GetModelMesh()
	{
		return Impl->ModelMesh;
	}

	math::AABB3 GltfModel::GetModelBox() const
	{
		return Impl->ModelBox;
	}

	std::shared_ptr<Engine::GltfNode> GltfModel::RootNode()
	{
		return Impl->RootNode;
	}

	std::shared_ptr<Engine::GltfSkeleton> GltfModel::GetSkeleton()
	{
		return Impl->Skeleton;
	}

	void GltfModel::Play(float TotalDeltaTime, float DeltaFrameTime)
	{
		if (Impl->AnimationMgr)
		{
			Impl->AnimationMgr->Play(TotalDeltaTime);
		}

		if (Impl->Skeleton)
		{
			Impl->Skeleton->UpdateBone(DeltaFrameTime);
		}

		UpdateNode();
	}

	void GltfModel::LoadNode()
	{
		if (!Impl->GltfMode.nodes.empty())
		{
			Impl->RootNode = std::make_shared<GltfNode>(&Impl->GltfMode);

			auto& Scenes = Impl->GltfMode.scenes;
			if (Scenes.size() > 0)
			{
				auto& NodeIds = Scenes[0].nodes;
				for (int k = 0; k < NodeIds.size(); k++)
				{
					Impl->RootNode->InitGroupNode(NodeIds[k]);
				}
			}
		}
	}

	void GltfModel::LoadMesh()
	{
		std::vector <std::shared_ptr<GltfMaterial>> ModelMaterial(std::move(LoadMaterial()));
		for (int i = 0; i < Impl->GltfMode.meshes.size(); i++)
		{
			auto& ModelMesh = Impl->GltfMode.meshes[i];
			for (int j = 0; j < ModelMesh.primitives.size(); j++)
			{
				std::shared_ptr<GltfMesh> Mesh = std::make_shared<GltfMesh>(&Impl->GltfMode);
				Mesh->Init(i, j, ModelMaterial, Impl->RootNode);

				Impl->HasSkin = Mesh->HasSkin();
				Impl->ModelMesh.push_back(Mesh);
			}
		}

		bool isInit = false;
		for (int i = 0; i < Impl->ModelMesh.size(); i++)
		{
			const AABB3& MeshBox = Impl->ModelMesh[i]->GetBoundingBox();

			AABB3 TmpMeshBox = MeshBox.Transform(Impl->ModelMesh[i]->GetMeshMat());

			if (!isInit)
			{
				Impl->ModelBox = TmpMeshBox;
				isInit = true;
			}
			Impl->ModelBox.UpdateMinMax(TmpMeshBox.GetMinPoint());
			Impl->ModelBox.UpdateMinMax(TmpMeshBox.GetMaxPoint());
		}
	}

	void GltfModel::LoadAnimate()
	{
		if (!Impl->AnimationMgr)
		{
			Impl->AnimationMgr = std::make_shared<GltfAnimationManager>(&Impl->GltfMode,this);
		}
		Impl->AnimationMgr->InitAnimation();
	}

	void GltfModel::LoadSkeleton()
	{
		Impl->Skeleton = std::make_shared<GltfSkeleton>(&Impl->GltfMode, Impl->RootNode);
		Impl->Skeleton->InitSkeleton();
		if (Impl->Config)
		{
			Impl->Skeleton->AddDynamicBone(Impl->Config->GetDyNamicBoneInfoList());
		}
	}

	std::vector <std::shared_ptr<GltfMaterial>> GltfModel::LoadMaterial()
	{
		std::vector <std::shared_ptr<GltfMaterial>> ModelMaterial;
		for (int i = 0; i < Impl->GltfMode.materials.size(); i++)
		{
			std::shared_ptr< GltfMaterial> PBRMaterial = std::make_shared<GltfMaterial>(&Impl->GltfMode);
			PBRMaterial->InitMaterial(i);
			ModelMaterial.push_back(PBRMaterial);
		}
		return ModelMaterial;
	}

}
