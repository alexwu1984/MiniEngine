cmake_minimum_required(VERSION 3.15)
project(Engine)

#-------------------------------------------------------------------------------------------
# Library definition
#-------------------------------------------------------------------------------------------
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/../ThirdParty)
include_directories(${PROJECT_SOURCE_DIR}/../Core)
include_directories(${PROJECT_SOURCE_DIR}/../Render)
include_directories(${PROJECT_SOURCE_DIR}/../ThirdParty/Assimp/include)

file(GLOB App_Header App/*.h)
file(GLOB Engine_Header Engine/*.h)
file(GLOB Engine_Thread_Header Engine/Thread/*.h)
file(GLOB Engine_Scene_Header Engine/Scene/*.h)
file(GLOB Engine_GltfModel_Header Engine/GltfModel/*.h)
file(GLOB Engine_ObjModel_Header Engine/ObjModel/*.h)
file(GLOB Engine_Render_Header Engine/Render/*.h)
file(GLOB Engine_Render_Header_Shadow Engine/Render/Shadow/*.h)
file(GLOB Engine_Render_SceneRendering_Header Engine/Render/SceneRendering/*.h)
file(GLOB Engine_Material_Header Engine/Material/*.h)
file(GLOB Engine_Procedural_Header Engine/Procedural/*.h)

source_group("Header Files\\App" FILES ${App_Header} )
source_group("Header Files\\Engine" FILES ${Engine_Header} )
source_group("Header Files\\Engine\\Thread" FILES ${Engine_Thread_Header} )
source_group("Header Files\\Engine\\Scene" FILES ${Engine_Scene_Header} )
source_group("Header Files\\Engine\\GltfModel" FILES ${Engine_GltfModel_Header} )
source_group("Header Files\\Engine\\ObjModel" FILES ${Engine_ObjModel_Header} )
source_group("Header Files\\Engine\\Render" FILES ${Engine_Render_Header} )
source_group("Header Files\\Engine\\Render\\Shadow" FILES ${Engine_Render_Header_Shadow} )
source_group("Header Files\\Engine\\Render\\SceneRendering" FILES ${Engine_Render_SceneRendering_Header} )
source_group("Header Files\\Engine\\Material" FILES ${Engine_Material_Header} )
source_group("Header Files\\Engine\\Procedural" FILES ${Engine_Procedural_Header} )

file(GLOB App_Source Src/App/*.cpp)
file(GLOB Engine_Source Src/Engine/*.cpp)
# Explicit sources: GLOB can miss new files until CMake reconfigure (LNK2019 on RHISubmissionThread).
set(Engine_Thread_Source
	Src/Engine/Thread/EngineThread.cpp
	Src/Engine/Thread/RenderThread.cpp
	Src/Engine/Thread/RHISubmissionThread.cpp
)
# Explicit: GLOB can omit new files until CMake reconfigure (LNK2019).
set(Engine_Scene_Source
	Src/Engine/Scene/Actor.cpp
	Src/Engine/Scene/CameraComponent.cpp
	Src/Engine/Scene/Component.cpp
	Src/Engine/Scene/DirectionalLightComponent.cpp
	Src/Engine/Scene/GameViewportClient.cpp
	Src/Engine/Scene/GltfActor.cpp
	Src/Engine/Scene/GltfInputComponent.cpp
	Src/Engine/Scene/GltfMeshComponent.cpp
	Src/Engine/Scene/FreeRoamCameraComponent.cpp
	Src/Engine/Scene/RoamCameraActor.cpp
	Src/Engine/Scene/SkyLightComponent.cpp
	Src/Engine/Scene/World.cpp
)
file(GLOB Engine_GltfModel_Source Src/Engine/GltfModel/*.cpp)
file(GLOB Engine_ObjModel_Source Src/Engine/ObjModel/*.cpp)
file(GLOB Engine_Render_Source Src/Engine/Render/*.cpp)
file(GLOB Engine_Render_Shadow_Source Src/Engine/Render/Shadow/*.cpp)
# Explicit: GLOB can omit new files until reconfigure (LNK2019).
set(Engine_Render_SceneRendering_Source
	Src/Engine/Render/SceneRendering/FDeferredBasePassMeshDispatch.cpp
	Src/Engine/Render/SceneRendering/FDeferredShadingBasePassRenderer.cpp
	Src/Engine/Render/SceneRendering/FMeshMaterialRenderCache.cpp
	Src/Engine/Render/SceneRendering/FOpaqueMeshDrawBuilder.cpp
	Src/Engine/Render/SceneRendering/FSceneMaterialShaderParameters.cpp
	Src/Engine/Render/SceneRendering/FSceneRenderer.cpp
	Src/Engine/Render/SceneRendering/FSceneRendererPrimitiveGather.cpp
	Src/Engine/Render/SceneRendering/FSceneViewData.cpp
	Src/Engine/Render/SceneRendering/FSceneViewFamily.cpp
	Src/Engine/Render/SceneRendering/FTranslucentMeshSorter.cpp
)
file(GLOB Engine_Material_Source Src/Engine/Material/*.cpp)
file(GLOB Engine_Procedural_Source Src/Engine/Procedural/*.cpp)

source_group("Source Files\\Src\\App" FILES ${App_Source} )
source_group("Source Files\\Src\\Engine" FILES ${Engine_Source} )
source_group("Source Files\\Src\\Engine\\Thread" FILES ${Engine_Thread_Source} )
source_group("Source Files\\Src\\Engine\\Scene" FILES ${Engine_Scene_Source} )
source_group("Source Files\\Src\\Engine\\GltfModel" FILES ${Engine_GltfModel_Source} )
source_group("Source Files\\Src\\Engine\\ObjModel" FILES ${Engine_ObjModel_Source} )
source_group("Source Files\\Src\\Engine\\Render" FILES ${Engine_Render_Source} )
source_group("Source Files\\Src\\Engine\\Render\\Shadow" FILES ${Engine_Render_Shadow_Source} )
source_group("Source Files\\Src\\Engine\\Render\\SceneRendering" FILES ${Engine_Render_SceneRendering_Source} )
source_group("Source Files\\Src\\Engine\\Material" FILES ${Engine_Material_Source} )
source_group("Source Files\\Src\\Engine\\Procedural" FILES ${Engine_Procedural_Source} )

add_library(Engine ${App_Source} ${Engine_Source} ${App_Header} ${Engine_Header} ${Engine_Thread_Source} ${Engine_Thread_Header} 
                   ${Engine_Scene_Header} ${Engine_Scene_Source} ${Engine_GltfModel_Header} ${Engine_GltfModel_Source}
                   ${Engine_Render_Header} ${Engine_Render_Source} ${Engine_Render_Header_Shadow} ${Engine_Render_SceneRendering_Header}
                   ${Engine_Render_Shadow_Source} ${Engine_Render_SceneRendering_Source} ${Engine_ObjModel_Header} ${Engine_ObjModel_Source}
                   ${Engine_Material_Header} ${Engine_Material_Source} ${Engine_Procedural_Header} ${Engine_Procedural_Source})

#-------------------------------------------------------------------------------------------
# Defines
#-------------------------------------------------------------------------------------------
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_compile_options(/wd4201)

target_link_libraries(Engine LINK_PUBLIC Render tinygltf)
target_include_directories(Render PUBLIC App Engine)