#pragma once
#include "core/inc.h"
#include "math/aabb3.h"

struct aiScene;
struct aiMesh;
namespace Engine
{
	struct ObjMeshPrivate;

	class ObjMesh
	{
	public:
		ObjMesh(const aiScene *pScene, aiMesh* pMesh,const std::string& Directory);
		~ObjMesh();

		void Init();
	private:
		void ProcessVertex();
		void ProcessIndices();
	private:
		ObjMeshPrivate* d_ptr = nullptr;
	};
}