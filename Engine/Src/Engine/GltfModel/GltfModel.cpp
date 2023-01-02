#include "GltfModel/GltfModel.h"
#include "core/strings.h"
#include "GltfModel/GltfNode.h"

namespace Engine
{
	struct GltfModelP
	{
		tinygltf::TinyGLTF GltfCtx;
		tinygltf::Model GltfMode;
		std::shared_ptr<GltfNode> RootNode;
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

	}

}
