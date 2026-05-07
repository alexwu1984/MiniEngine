#include "GltfModel/GltfModel.h"
#include "core/strings.h"
#include "GltfModel/GltfNode.h"
#include "GltfModel/GltfMesh.h"
#include "Material/GltfMaterial.h"
#include "Material/FurMaterial.h"
#include "GltfModel/GltfAnimationManager.h"
#include "GltfModel/GltfSkeleton.h"
#include "Scene/SceneModelAsset.h"


namespace Engine
{
	using namespace math;

	struct GltfModelPrivate
	{
		tinygltf::TinyGLTF GltfCtx;
		tinygltf::Model GltfMode;
		std::shared_ptr<GltfNode> RootNode;
		std::vector<std::shared_ptr<GltfMesh>> ModelMesh;
		std::shared_ptr<GltfSkeleton> Skeleton;
		bool HasSkin = false;
		AABB3  ModelBox;
		std::shared_ptr<GltfAnimationManager> AnimationMgr;
		std::shared_ptr<SceneModelAsset> Asset;
	};

	GltfModel::GltfModel()
		:d_ptr(new GltfModelPrivate())
	{

	}

	GltfModel::~GltfModel()
	{
		delete d_ptr;
	}

	bool GltfModel::Load(const std::wstring& FileName, std::shared_ptr<SceneModelAsset> Asset)
	{
		C_P(GltfModel);
		d->Asset = std::move(Asset);
		std::string err;
		std::string warn;
		std::string utf8FileName = core::ucs2_u8(FileName);

		if (FileName.find(L".gltf") != std::wstring::npos)
		{
			if (d->GltfCtx.LoadASCIIFromFile(&d->GltfMode, &err, &warn, utf8FileName))
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
			if (d->GltfCtx.LoadBinaryFromFile(&d->GltfMode, &err, &warn, utf8FileName))
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

	std::vector<std::shared_ptr<GltfMesh>>& GltfModel::GetModelMesh()
	{
		C_P(GltfModel);
		return d->ModelMesh;
	}

	math::AABB3 GltfModel::GetModelBox() const
	{
		C_P(GltfModel);
		return d->ModelBox;
	}

	std::shared_ptr<Engine::GltfNode> GltfModel::RootNode()
	{
		C_P(GltfModel);
		return d->RootNode;
	}

	std::shared_ptr<Engine::GltfSkeleton> GltfModel::GetSkeleton()
	{
		C_P(GltfModel);
		return d->Skeleton;
	}

	void GltfModel::Play(float TotalDeltaTime, float DeltaFrameTime)
	{
		C_P(GltfModel);
		const bool bSkipAnim = d->Asset && d->Asset->GetDisableAnimation();
		if (!bSkipAnim && d->AnimationMgr)
			d->AnimationMgr->Play(TotalDeltaTime);

		if (d->Skeleton)
			d->Skeleton->UpdateBone();

		// UpdateBone() calls UpdateNodeParent on skeleton parents and refreshes FinalMeshMat on the graph.
		// MeshMat must be copied from nodes every frame; gating UpdateNode() on AnimationMgr::Play()'s return
		// left stale CurrModelMatrix (BasePass / world position wrong while vertex fetch + UVs still look fine).
		if (d->RootNode && !d->ModelMesh.empty())
			UpdateNode();
	}

	std::shared_ptr<Engine::SceneModelAsset> GltfModel::GetAsset() const
	{
		C_P(const GltfModel);
		return d->Asset;
	}

	void GltfModel::LoadNode()
	{
		C_P(GltfModel);
		if (!d->GltfMode.nodes.empty())
		{
			d->RootNode = std::make_shared<GltfNode>(&d->GltfMode);

			auto& Scenes = d->GltfMode.scenes;
			if (Scenes.size() > 0)
			{
				auto& NodeIds = Scenes[0].nodes;
				for (int k = 0; k < NodeIds.size(); k++)
				{
					d->RootNode->InitGroupNode(NodeIds[k]);
				}
			}
		}
	}

	void GltfModel::UpdateNode()
	{
		C_P(GltfModel);
		if (!d->RootNode)
		{
			return;
		}
		for (int i = 0; i < d->ModelMesh.size(); i++)
		{
			auto NodeInfo = d->RootNode->GetNodeInfo(d->ModelMesh[i]->GetNodeId());
			if (NodeInfo)
			{
				d->RootNode->UpdateNodeParent(NodeInfo);
				d->ModelMesh[i]->SetMeshMat(NodeInfo->FinalMeshMat);
			}
		}
	}

	void GltfModel::LoadMesh()
	{
		C_P(GltfModel);
		std::vector <std::shared_ptr<GltfMaterial>> ModelMaterial(std::move(LoadMaterial()));
		for (int i = 0; i < d->GltfMode.meshes.size(); i++)
		{
			auto& ModelMesh = d->GltfMode.meshes[i];
			for (int j = 0; j < ModelMesh.primitives.size(); j++)
			{
				std::shared_ptr<GltfMesh> Mesh = std::make_shared<GltfMesh>(&d->GltfMode,this);
				Mesh->Init(i, j, ModelMaterial, d->RootNode);

				d->HasSkin = Mesh->HasSkin();
				d->ModelMesh.push_back(Mesh);
			}
		}

		bool isInit = false;
		for (int i = 0; i < d->ModelMesh.size(); i++)
		{
			const AABB3& MeshBox = d->ModelMesh[i]->GetBoundingBox();

			AABB3 TmpMeshBox = MeshBox.Transform(d->ModelMesh[i]->GetMeshMat());

			if (!isInit)
			{
				d->ModelBox = TmpMeshBox;
				isInit = true;
			}
			d->ModelBox.UpdateMinMax(TmpMeshBox.GetMinPoint());
			d->ModelBox.UpdateMinMax(TmpMeshBox.GetMaxPoint());
		}
	}

	void GltfModel::LoadAnimate()
	{
		C_P(GltfModel);
		if (!d->AnimationMgr)
		{
			d->AnimationMgr = std::make_shared<GltfAnimationManager>(&d->GltfMode,this);
		}
		d->AnimationMgr->InitAnimation();
	}

	void GltfModel::LoadSkeleton()
	{
		C_P(GltfModel);
		d->Skeleton = std::make_shared<GltfSkeleton>(&d->GltfMode, d->RootNode);
		d->Skeleton->InitSkeleton();
		if (d->Asset)
		{
			d->Skeleton->AddDynamicBone(d->Asset->GetDyNamicBoneInfoList());
		}
	}

	std::vector <std::shared_ptr<GltfMaterial>> GltfModel::LoadMaterial()
	{
		C_P(GltfModel);
		std::vector <std::shared_ptr<GltfMaterial>> ModelMaterial;
		for (int i = 0; i < d->GltfMode.materials.size(); i++)
		{
			auto& Material = d->GltfMode.materials[i];
			std::string MaterialName = Material.name;
			std::shared_ptr< GltfMaterial> PBRMaterial;
			if (d->Asset && !d->Asset->GetFurConfig().Name.empty() &&  MaterialName == d->Asset->GetFurConfig().Name)
			{
				PBRMaterial = std::make_shared<FurMaterial>(this, &d->GltfMode);	
			}
			else
			{
				PBRMaterial = std::make_shared<GltfMaterial>(this, &d->GltfMode);
			}
			PBRMaterial->InitMaterial(i);

			ModelMaterial.push_back(PBRMaterial);
		}

		if (d->GltfMode.materials.empty())
		{
			// Default primitive material: use plain PBR. FurMaterial is only for assets whose named material matches FurConfig.Name.
			std::shared_ptr<GltfMaterial> PBRMaterial = std::make_shared<GltfMaterial>(this, &d->GltfMode);
			PBRMaterial->InitMaterial(0);
			ModelMaterial.push_back(PBRMaterial);
		}
		return ModelMaterial;
	}

}
