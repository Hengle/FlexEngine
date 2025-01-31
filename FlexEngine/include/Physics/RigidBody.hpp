#pragma once

#include "Physics/PhysicsHelpers.hpp"

class btRigidBody;
class btCollisionShape;
class btMotionState;
class btTransform;
class btTypedConstraint;

namespace flex
{
	class RigidBody
	{
	public:
		RigidBody(u32 group = (u32)CollisionType::DEFAULT, u32 mask = (u32)CollisionType::DEFAULT);
		// NOTE: This copy constructor does not initialize data, it only copies POD fields
		RigidBody(const RigidBody& other);
		virtual ~RigidBody();

		void Initialize(btCollisionShape* collisionShape, Transform* transform);
		void Destroy();

		void AddConstraint(btTypedConstraint* constraint);

		void SetMass(real mass);
		real GetMass() const;
		void SetKinematic(bool bKinematic);
		bool IsKinematic() const;
		void SetStatic(bool bStatic);
		bool IsStatic() const;
		void SetFriction(real friction);
		real GetFriction() const;

		void SetLinearDamping(real linearDamping);
		void SetAngularDamping(real angularDamping);

		// Limit movement to the given axes (0 = locked, 1 = free)
		void SetLinearFactor(const btVector3& factor);
		// Limit rotation to the given axes (0 = locked, 1 = free)
		void SetAngularFactor(const btVector3& factor);

		// Vector passed in defines the axis (or axes) this body can rotate around
		void SetOrientationConstraint(const btVector3& axis);

		// Vector passed in defines the axis (or axes) this body can move along
		void SetPositionalConstraint(const btVector3& axis);

		void SetWorldPosition(const glm::vec3& worldPos);
		void SetWorldRotation(const glm::quat& worldRot);
		void SetWorldPositionAndRotation(const glm::vec3& worldPos, const glm::quat& worldRot);

		u32 GetGroup() const;
		// NOTE: This function removes, then re-adds this object to the world!
		void SetGroup(u32 group);

		u32 GetMask() const;
		// NOTE: This function removes, then re-adds this object to the world!
		void SetMask(u32 mask);

		// Applies rigid body transform to our transform (taking into account local transform)
		//void UpdateTransform();

		// Sets rigid body transform to our transform (taking into account local transform)
		//void MatchTransform();

		void GetUpRightForward(btVector3& up, btVector3& right, btVector3& forward);

		btRigidBody* GetRigidBodyInternal();

		void SetPhysicsFlags(u32 flags);
		u32 GetPhysicsFlags();

		static RigidBody* ParseFromJSON(const JSONObject& rigidBodyObj);
		JSONObject SerializeToJSON();

	private:
		btRigidBody* m_btRigidBody = nullptr;
		btMotionState* m_btMotionState = nullptr;

		std::vector<btTypedConstraint*> m_Constraints;

		// Must be 0 if static, non-zero otherwise
		real m_Mass = 1.0f;
		bool m_bStatic = false;
		bool m_bKinematic = false;
		real m_Friction = 1.0f;

		u32 m_Group = (u32)CollisionType::DEFAULT;
		u32 m_Mask = (u32)CollisionType::DEFAULT;

		real m_AngularDamping = 0.0f;
		real m_LinearDamping = 0.0f;

		// Flags set from PhysicsFlag enum
		u32 m_Flags = 0;
	};
} // namespace flex
