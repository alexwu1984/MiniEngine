#include "GltfModel/GltfModel.h"
#include "core/strings.h"
#include "GltfModel/GltfNode.h"
#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMaterial.h"

namespace Engine
{
	using namespace math;

	struct GltfModelP
	{
		tinygltf::TinyGLTF GltfCtx;
		tinygltf::Model GltfMode;
		std::shared_ptr<GltfNode> RootNode;
		std::vector<std::shared_ptr<GltfMesh>> ModelMesh;
		bool HasSkin = false;
		AABB3  ModelBox;
	};

	GltfModel::GltfModel()
		:Data(std::make_shared<GltfModelP>())
	{

	}

	GltfModel::~GltfModel()
	{

	}

	bool GltfModel::Load(const std::wstring& FileName)
	{
		std::string err;
		std::string warn;
		std::string utf8FileName = core::ucs2_u8(FileName);

		if (FileName.find(L".gltf") != std::wstring::npos)
		{
			if (Data->GltfCtx.LoadASCIIFromFile(&Data->GltfMode, &err, &warn, utf8FileName))
			{
				LoadNode();
				LoadMesh();

				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			if (Data->GltfCtx.LoadBinaryFromFile(&Data->GltfMode, &err, &warn, utf8FileName))
			{
				LoadNode();
				LoadMesh();

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
		return Data->ModelMesh;
	}

	void GltfModel::LoadNode()
	{
		if (!Data->GltfMode.nodes.empty())
		{
			Data->RootNode = std::make_shared<GltfNode>(&Data->GltfMode);

			auto& Scenes = Data->GltfMode.scenes;
			if (Scenes.size() > 0)
			{
				auto& NodeIds = Scenes[0].nodes;
				for (int k = 0; k < NodeIds.size(); k++)
				{
					Data->RootNode->InitGroupNode(NodeIds[k]);
				}
			}
		}
	}

	void GltfModel::LoadMesh()
	{
		std::vector <std::shared_ptr<GltfMaterial>> ModelMaterial(std::move(LoadMaterial()));
		for (int i = 0; i < Data->GltfMode.meshes.size(); i++)
		{
			auto& ModelMesh = Data->GltfMode.meshes[i];
			for (int j = 0; j < ModelMesh.primitives.size(); j++)
			{
				std::shared_ptr<GltfMesh> Mesh = std::make_shared<GltfMesh>(&Data->GltfMode);
				Mesh->Init(i, j, ModelMaterial, Data->RootNode);

				Data->HasSkin = Mesh->HasSkin();
				Data->ModelMesh.push_back(Mesh);
			}
		}

		bool isInit = false;
		for (int i = 0; i < Data->ModelMesh.size(); i++)
		{
			const AABB3& MeshBox = Data->ModelMesh[i]->GetBoundingBox();

			AABB3 TmpMeshBox = MeshBox.Transform(Data->ModelMesh[i]->GetMeshMat());

			if (!isInit)
			{
				Data->ModelBox = TmpMeshBox;
				isInit = true;
			}
			Data->ModelBox.UpdateMinMax(TmpMeshBox.GetMinPoint());
			Data->ModelBox.UpdateMinMax(TmpMeshBox.GetMaxPoint());
		}
	}

	std::vector <std::shared_ptr<GltfMaterial>> GltfModel::LoadMaterial()
	{
		std::vector <std::shared_ptr<GltfMaterial>> ModelMaterial;
		for (int i = 0; i < Data->GltfMode.materials.size(); i++)
		{
			std::shared_ptr< GltfMaterial> PBRMaterial = std::make_shared<GltfMaterial>(&Data->GltfMode);
			PBRMaterial->InitMaterial(i);
			ModelMaterial.push_back(PBRMaterial);
		}
		return ModelMaterial;
	}

}
