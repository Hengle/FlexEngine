#include "stdafx.hpp"

#include "Transform.hpp"

IGNORE_WARNINGS_PUSH
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "BulletDynamics/Dynamics/btRigidBody.h"
IGNORE_WARNINGS_POP

#include "Helpers.hpp"
#include "Physics/RigidBody.hpp"
#include "Scene/GameObject.hpp"

namespace flex
{
	Transform Transform::m_Identity = Transform(VEC3_ZERO, QUAT_IDENTITY, VEC3_ONE);

	Transform::Transform()
	{
		SetAsIdentity();
	}

	Transform::Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) :
		localPosition(position),
		localRotation(rotation),
		localScale(scale),
		worldPosition(position),
		worldRotation(rotation),
		worldScale(scale),
		localTransform(glm::translate(MAT4_IDENTITY, position) *
			// Cast away constness
			glm::mat4((glm::quat)rotation) *
			glm::scale(MAT4_IDENTITY, scale)),
		worldTransform(glm::translate(MAT4_IDENTITY, position) *
			// Cast away constness
			glm::mat4((glm::quat)rotation) *
			glm::scale(MAT4_IDENTITY, scale)),
		forward(VEC3_FORWARD),
		up(VEC3_UP),
		right(VEC3_RIGHT)
	{
	}

	Transform::Transform(const glm::vec3& position, const glm::quat& rotation) :
		localPosition(position),
		localRotation(rotation),
		localScale(VEC3_ONE),
		worldPosition(position),
		worldRotation(rotation),
		worldScale(VEC3_ONE),
		localTransform(glm::translate(MAT4_IDENTITY, position) *
			glm::mat4((glm::quat)rotation) *
			glm::scale(MAT4_IDENTITY, VEC3_ONE)),
		worldTransform(glm::translate(MAT4_IDENTITY, position) *
			glm::mat4((glm::quat)rotation) *
			glm::scale(MAT4_IDENTITY, VEC3_ONE)),
		forward(VEC3_FORWARD),
		up(VEC3_UP),
		right(VEC3_RIGHT)
	{
	}

	Transform::Transform(const glm::vec3& position) :
		localPosition(position),
		localRotation(QUAT_IDENTITY),
		localScale(VEC3_ONE),
		worldPosition(position),
		worldRotation(VEC3_ZERO),
		worldScale(VEC3_ONE),
		localTransform(glm::translate(MAT4_IDENTITY, position) *
			glm::mat4(glm::quat(VEC3_ZERO)) *
			glm::scale(MAT4_IDENTITY, VEC3_ONE)),
		worldTransform(glm::translate(MAT4_IDENTITY, position) *
			glm::mat4(glm::quat(VEC3_ZERO)) *
			glm::scale(MAT4_IDENTITY, VEC3_ONE)),
		forward(VEC3_FORWARD),
		up(VEC3_UP),
		right(VEC3_RIGHT)
	{
	}

	Transform::Transform(const Transform& other) :
		localPosition(other.localPosition),
		localRotation(other.localRotation),
		localScale(other.localScale),
		worldPosition(other.localPosition),
		worldRotation(other.localRotation),
		worldScale(other.localScale),
		forward(VEC3_FORWARD),
		up(VEC3_UP),
		right(VEC3_RIGHT),
		localTransform(glm::translate(MAT4_IDENTITY, other.localPosition) *
			glm::mat4((glm::quat)other.localRotation) *
			glm::scale(MAT4_IDENTITY, other.localScale)),
		worldTransform(glm::translate(MAT4_IDENTITY, other.worldPosition) *
			glm::mat4((glm::quat)other.worldRotation) *
			glm::scale(MAT4_IDENTITY, other.worldScale))
	{
	}

	Transform::Transform(const Transform&& other) :
		localPosition(other.localPosition),
		localRotation(other.localRotation),
		localScale(other.localScale),
		worldPosition(other.worldPosition),
		worldRotation(other.worldRotation),
		worldScale(other.worldScale),
		forward(VEC3_FORWARD),
		up(VEC3_UP),
		right(VEC3_RIGHT)
	{
		localTransform = (glm::translate(MAT4_IDENTITY, localPosition) *
			glm::mat4(localRotation) *
			glm::scale(MAT4_IDENTITY, localScale));
		worldTransform = (glm::translate(MAT4_IDENTITY, worldPosition) *
			glm::mat4(worldRotation) *
			glm::scale(MAT4_IDENTITY, worldScale));
	}

	Transform& Transform::operator=(const Transform& other)
	{
		localPosition = other.localPosition;
		localRotation = other.localRotation;
		localScale = other.localScale;
		worldPosition = other.worldPosition;
		worldRotation = other.worldRotation;
		worldScale = other.worldScale;
		localTransform = glm::mat4(glm::translate(MAT4_IDENTITY, localPosition) *
			glm::mat4(localRotation) *
			glm::scale(MAT4_IDENTITY, localScale));
		worldTransform = glm::mat4(glm::translate(MAT4_IDENTITY, worldPosition) *
			glm::mat4(worldRotation) *
			glm::scale(MAT4_IDENTITY, worldScale));
		forward = other.forward;
		up = other.up;
		right = other.right;
		return *this;
	}

	Transform& Transform::operator=(const Transform&& other)
	{
		if (this != &other)
		{
			localPosition = other.localPosition;
			localRotation = other.localRotation;
			localScale = other.localScale;
			worldPosition = other.worldPosition;
			worldRotation = other.worldRotation;
			worldScale = other.worldScale;
			localTransform = glm::mat4(glm::translate(MAT4_IDENTITY, localPosition) *
				glm::mat4(localRotation) *
				glm::scale(MAT4_IDENTITY, localScale));
			worldTransform = glm::mat4(glm::translate(MAT4_IDENTITY, worldPosition) *
				glm::mat4(worldRotation) *
				glm::scale(MAT4_IDENTITY, worldScale));
			forward = other.forward;
			up = other.up;
			right = other.right;
		}

		return *this;
	}

	Transform::~Transform()
	{
	}

	Transform Transform::ParseJSON(const JSONObject& transformObject)
	{
		glm::vec3 pos;
		glm::quat rot;
		glm::vec3 scale;
		ParseJSON(transformObject, pos, rot, scale);
		return Transform(pos, rot, scale);
	}

	void Transform::ParseJSON(const JSONObject& object, glm::mat4& outModel)
	{
		const JSONObject& transformObject = object.GetObject("transform");

		glm::vec3 pos;
		glm::quat rot;
		glm::vec3 scale;
		ParseJSON(transformObject, pos, rot, scale);
		outModel = glm::translate(MAT4_IDENTITY, pos) *
			glm::mat4(rot) *
			glm::scale(MAT4_IDENTITY, scale);
	}

	void Transform::ParseJSON(const JSONObject& object, glm::vec3& outPos, glm::quat& outRot, glm::vec3& outScale)
	{
		std::string posStr = object.GetString("pos");
		std::string rotStr = object.GetString("rot");
		std::string scaleStr = object.GetString("scale");

		outPos = glm::vec3(0.0f);
		if (!posStr.empty())
		{
			outPos = ParseVec3(posStr);
		}

		glm::vec3 rotEuler(0.0f);
		if (!rotStr.empty())
		{
			rotEuler = ParseVec3(rotStr);
		}

		outScale = glm::vec3(1.0f);
		if (!scaleStr.empty())
		{
			outScale = ParseVec3(scaleStr);
		}

		// Check we aren't getting garbage data in
#if DEBUG
		if (IsNanOrInf(outPos))
		{
			PrintError("Read garbage value from transform pos in serialized scene file! Using default value instead\n");
			outPos = VEC3_ZERO;
		}

		if (IsNanOrInf(rotEuler))
		{
			PrintError("Read garbage value from transform rot in serialized scene file! Using default value instead\n");
			rotEuler = VEC3_ZERO;
		}

		if (IsNanOrInf(outScale))
		{
			PrintError("Read garbage value from transform scale in serialized scene file! Using default value instead\n");
			outScale = VEC3_ONE;
		}
#endif

		outRot = glm::quat(rotEuler);
	}

	JSONField Transform::Serialize() const
	{
		return Serialize(localPosition, localRotation, localScale, m_GameObject->GetName().c_str());
	}

	JSONField Transform::Serialize(const glm::mat4 matrix, const char* objName)
	{
		glm::vec3 pos;
		glm::quat rot;
		glm::vec3 scale;
		glm::vec3 skew;
		glm::vec4 persp;
		glm::decompose(matrix, scale, rot, pos, skew, persp);

		return Serialize(pos, rot, scale, objName);
	}

	JSONField Transform::Serialize(const glm::vec3& inPos, const glm::quat& inRot, const glm::vec3& inScale, const char* objName)
	{
		const i32 floatPrecision = 3;

		glm::vec3 pos = inPos;
		glm::quat rot = inRot;
		glm::vec3 scale = inScale;

		JSONField transformField = {};
		transformField.label = "transform";

		JSONObject transformObject = {};

		if (IsNanOrInf(pos))
		{
			PrintError("Attempted to serialize garbage value for position of %s, writing default value\n", objName);
			pos = VEC3_ZERO;
		}

		if (IsNanOrInf(rot))
		{
			PrintError("Attempted to serialize garbage value for rotation of %s, writing default value\n", objName);
			rot = glm::quat();
		}

		if (IsNanOrInf(scale))
		{
			PrintError("Attempted to serialize garbage value for scale of %s, writing default value\n", objName);
			scale = VEC3_ONE;
		}


		if (pos != VEC3_ZERO)
		{
			std::string posStr = VecToString(pos, floatPrecision);
			transformObject.fields.emplace_back("pos", JSONValue(posStr));
		}

		if (rot != QUAT_IDENTITY)
		{
			glm::vec3 rotEuler = glm::eulerAngles(rot);
			std::string rotStr = VecToString(rotEuler, floatPrecision);
			transformObject.fields.emplace_back("rot", JSONValue(rotStr));
		}

		if (scale != VEC3_ONE)
		{
			std::string scaleStr = VecToString(scale, floatPrecision);
			transformObject.fields.emplace_back("scale", JSONValue(scaleStr));
		}

		transformField.value = JSONValue(transformObject);

		return transformField;
	}

	void Transform::Translate(const glm::vec3& deltaPosition)
	{
		localPosition += deltaPosition;

		UpdateParentTransform();
	}

	void Transform::Translate(real deltaX, real deltaY, real deltaZ)
	{
		localPosition.x += deltaX;
		localPosition.y += deltaY;
		localPosition.z += deltaZ;

		UpdateParentTransform();
	}

	void Transform::Rotate(const glm::quat& deltaQuatRotation)
	{
		localRotation *= deltaQuatRotation;

		UpdateParentTransform();
	}

	void Transform::Scale(const glm::vec3& deltaScale)
	{
		localScale *= deltaScale;

		UpdateParentTransform();
	}

	void Transform::Scale(real deltaScale)
	{
		Scale(glm::vec3(deltaScale));
	}

	void Transform::Scale(real deltaX, real deltaY, real deltaZ)
	{
		Scale(glm::vec3(deltaX, deltaY, deltaZ));
	}

	void Transform::SetWorldTransform(const glm::mat4& desiredWorldTransform)
	{
		if (m_GameObject->GetParent() != nullptr)
		{
			localTransform = glm::inverse(m_GameObject->GetParent()->GetTransform()->GetWorldTransform()) * desiredWorldTransform;
		}
		else
		{
			localTransform = desiredWorldTransform;
		}

		glm::vec3 localSkew;
		glm::vec4 localPerspective;
		glm::decompose(localTransform, localScale, localRotation, localPosition, localSkew, localPerspective);

		UpdateParentTransform();
	}

	void Transform::SetAsIdentity()
	{
		*this = m_Identity;
	}

	bool Transform::IsIdentity() const
	{
		bool result = (localPosition == m_Identity.localPosition &&
			localRotation == m_Identity.localRotation &&
			localScale == m_Identity.localScale);
		return result;
	}

	Transform Transform::Identity()
	{
		Transform result;

		result.SetAsIdentity();

		return result;
	}

	void Transform::SetGameObject(GameObject* gameObject)
	{
		m_GameObject = gameObject;
	}

	GameObject* Transform::GetGameObject() const
	{
		return m_GameObject;
	}

	const glm::mat4& Transform::GetWorldTransform() const
	{
		return worldTransform;
	}

	const glm::mat4& Transform::GetLocalTransform() const
	{
		return localTransform;
	}

	void Transform::UpdateParentTransform()
	{
		localTransform = (glm::translate(MAT4_IDENTITY, localPosition) *
			glm::mat4(localRotation) *
			glm::scale(MAT4_IDENTITY, localScale));

		GameObject* parent = m_GameObject->GetParent();

		if (parent == nullptr)
		{
			worldPosition = localPosition;
			worldRotation = localRotation;
			worldScale = localScale;
		}

		if (parent != nullptr)
		{
			parent->GetTransform()->UpdateParentTransform();
		}
		else
		{
			UpdateChildTransforms();
		}

		glm::mat3 rotMat(worldRotation);
		right = rotMat[0];
		up = rotMat[1];
		forward = rotMat[2];

		if (updateParentOnStateChange && m_GameObject != nullptr)
		{
			m_GameObject->OnTransformChanged();
		}
	}

	void Transform::UpdateChildTransforms()
	{
		// Our local matrix should already have been updated at this point (in UpdateParentTransform)

		GameObject* parent = m_GameObject->GetParent();
		if (parent != nullptr)
		{
			Transform* parentTransform = parent->GetTransform();

			worldTransform = parentTransform->GetWorldTransform() * localTransform;

			glm::vec3 worldSkew;
			glm::vec4 worldPerspective;
			glm::decompose(worldTransform, worldScale, worldRotation, worldPosition, worldSkew, worldPerspective);
		}
		else
		{
			worldTransform = localTransform;
			worldPosition = localPosition;
			worldRotation = localRotation;
			worldScale = localScale;
		}

		RigidBody* rigidBody= m_GameObject->GetRigidBody();
		if (rigidBody != nullptr)
		{
			rigidBody->MatchParentTransform();
		}

		const std::vector<GameObject*>& children = m_GameObject->GetChildren();
		for (GameObject* child : children)
		{
			child->GetTransform()->UpdateChildTransforms();
		}
	}

	glm::vec3 Transform::GetLocalPosition() const
	{
		return localPosition;
	}

	glm::vec3 Transform::GetWorldPosition() const
	{
		return worldPosition;
	}

	glm::quat Transform::GetLocalRotation() const
	{
		return localRotation;
	}

	glm::quat Transform::GetWorldRotation() const
	{
		return worldRotation;
	}

	glm::vec3 Transform::GetLocalScale() const
	{
		return localScale;
	}

	glm::vec3 Transform::GetWorldScale() const
	{
		return worldScale;
	}

	glm::vec3 Transform::GetRight() const
	{
		return right;
	}

	glm::vec3 Transform::GetUp() const
	{
		return up;
	}

	glm::vec3 Transform::GetForward() const
	{
		return forward;
	}

	void Transform::SetLocalPosition(const glm::vec3& position, bool bUpdateChain /* = true */)
	{
		localPosition = position;

		if (bUpdateChain)
		{
			UpdateParentTransform();
		}
	}

	void Transform::SetWorldPosition(const glm::vec3& position, bool bUpdateChain /* = true */)
	{
		GameObject* parent = m_GameObject->GetParent();
		if (parent != nullptr)
		{
			localPosition = position - parent->GetTransform()->GetWorldPosition();
		}
		else
		{
			localPosition = position;
			// NOTE: World position will be set in UpdateParentTransform
		}

		if (bUpdateChain)
		{
			UpdateParentTransform();
		}
	}

	void Transform::SetLocalRotation(const glm::quat& quatRotation, bool bUpdateChain /* = true */)
	{
		localRotation = quatRotation;

		if (bUpdateChain)
		{
			UpdateParentTransform();
		}
	}

	void Transform::SetWorldRotation(const glm::quat& quatRotation, bool bUpdateChain /* = true */)
	{
		GameObject* parent = m_GameObject->GetParent();
		if (parent != nullptr)
		{
			localRotation = glm::inverse(parent->GetTransform()->GetWorldRotation()) * quatRotation;
		}
		else
		{
			localRotation = quatRotation;
			// World rotation will be set in UpdateParentTransform
		}

		if (bUpdateChain)
		{
			UpdateParentTransform();
		}
	}

	void Transform::SetLocalScale(const glm::vec3& scale, bool bUpdateChain /* = true */)
	{
		localScale = scale;

		if (bUpdateChain)
		{
			UpdateParentTransform();
		}
	}

	void Transform::SetWorldScale(const glm::vec3& scale, bool bUpdateChain /* = true */)
	{
		GameObject* parent = m_GameObject->GetParent();
		if (parent != nullptr)
		{
			localScale = scale / parent->GetTransform()->GetWorldScale();
		}
		else
		{
			localScale = scale;
			// World scale will be set in UpdateParentTransform
		}

		if (bUpdateChain)
		{
			UpdateParentTransform();
		}
	}

	void Transform::SetWorldFromMatrix(const glm::mat4& mat, bool bUpdateChain /* = true */)
	{
		glm::vec3 pos;
		glm::quat rot;
		glm::vec3 scale;
		glm::vec3 skew;
		glm::vec4 perspective;
		if (glm::decompose(mat, scale, rot, pos, skew, perspective))
		{
			SetWorldPosition(pos, false);
			SetWorldRotation(rot, false);
			SetWorldScale(scale, bUpdateChain);

			if (!bUpdateChain)
			{
				glm::mat3 rotMat(worldRotation);
				right = rotMat[0];
				up = rotMat[1];
				forward = rotMat[2];
			}
		}
	}
} // namespace flex
