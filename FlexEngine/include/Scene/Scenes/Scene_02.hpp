#pragma once

#include "Scene/Scenes/BaseScene.hpp"

#include <vector>

#include "Scene/MeshPrefab.hpp"

class btCollisionShape;

namespace flex
{
	class RigidBody;
	class ReflectionProbe;

	class Scene_02 : public BaseScene
	{
	public:
		Scene_02(const GameContext& gameContext);
		virtual ~Scene_02();

		virtual void Initialize(const GameContext& gameContext) override;
		virtual void PostInitialize(const GameContext& gameContext) override;
		virtual void Destroy(const GameContext& gameContext) override;
		virtual void Update(const GameContext& gameContext) override;

	private:
		std::vector<MeshPrefab*> m_Spheres;

		MeshPrefab* m_Arisaka = nullptr;
		MeshPrefab* m_Cerberus = nullptr;

		MaterialID m_SkyboxMatID_1 = InvalidMaterialID;
		MaterialID m_SkyboxMatID_2 = InvalidMaterialID;
		MaterialID m_SkyboxMatID_3 = InvalidMaterialID;
		MaterialID m_SkyboxMatID_4 = InvalidMaterialID;
		MaterialID m_SkyboxMatID_5 = InvalidMaterialID;
		MaterialID m_CurrentSkyboxMatID = InvalidMaterialID;

		MeshPrefab* m_Skybox = nullptr;
		MeshPrefab* m_Grid = nullptr;

		ReflectionProbe* m_ReflectionProbe = nullptr;

		MaterialID m_GridMaterialID = InvalidMaterialID;
		MaterialID m_WorldAxisMaterialID = InvalidMaterialID;


		// TEMP:
		btCollisionShape* m_BoxShape = nullptr;

		RigidBody* groundPlaneRB = nullptr;
		RigidBody* rb1 = nullptr;
		RigidBody* rb2 = nullptr;

		MeshPrefab* m_GroundPlane = nullptr;
		MeshPrefab* m_Box1 = nullptr;
		MeshPrefab* m_Box2 = nullptr;



		Scene_02(const Scene_02&) = delete;
		Scene_02& operator=(const Scene_02&) = delete;
	};
} // namespace flex
