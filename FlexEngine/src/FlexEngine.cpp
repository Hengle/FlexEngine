#include "stdafx.hpp"

#include "FlexEngine.hpp"

#include <stdlib.h> // For srand, rand
#include <time.h> // For time

IGNORE_WARNINGS_PUSH
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <glm/gtx/intersect.hpp>

#if COMPILE_RENDERDOC_API
#include "renderdoc/api/app/renderdoc_app.h"
#endif

#if COMPILE_IMGUI
#include "imgui_internal.h" // For something & InputTextEx
#endif
IGNORE_WARNINGS_POP

#include "Audio/AudioManager.hpp"
#include "Cameras/CameraManager.hpp"
#include "Cameras/DebugCamera.hpp"
#include "Cameras/FirstPersonCamera.hpp"
#include "Cameras/OverheadCamera.hpp"
#include "Cameras/TerminalCamera.hpp"
#include "Cameras/VehicleCamera.hpp"
#include "Editor.hpp"
#include "Graphics/Renderer.hpp"
#include "Helpers.hpp"
#include "InputManager.hpp"
#include "JSONParser.hpp"
#include "JSONTypes.hpp"
#include "Physics/PhysicsManager.hpp"
#include "Physics/PhysicsWorld.hpp"
#include "Physics/RigidBody.hpp"
#include "Platform/Platform.hpp"
#include "Player.hpp"
#include "PlayerController.hpp"
#include "Profiler.hpp"
#include "ResourceManager.hpp"
#include "Scene/BaseScene.hpp"
#include "Scene/GameObject.hpp"
#include "Scene/Mesh.hpp"
#include "Scene/MeshComponent.hpp"
#include "Scene/SceneManager.hpp"
#include "Time.hpp"
#include "Window/GLFWWindowWrapper.hpp"
#include "Window/Monitor.hpp"

#if COMPILE_OPEN_GL
#include "Graphics/GL/GLRenderer.hpp"
#endif

#if COMPILE_VULKAN
#include "Graphics/Vulkan/VulkanRenderer.hpp"
#endif

// TODO: Find out equivalent for nix systems
#ifdef _WINDOWS
// Specify that we prefer to be run on a discrete card on laptops when available
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x01;
	__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x01;
}
#endif // _WINDOWS

namespace flex
{
	bool FlexEngine::s_bHasGLDebugExtension = false;

	const u32 FlexEngine::EngineVersionMajor = 0;
	const u32 FlexEngine::EngineVersionMinor = 8;
	const u32 FlexEngine::EngineVersionPatch = 6;

	std::string FlexEngine::s_CurrentWorkingDirectory;
	std::string FlexEngine::s_ExecutablePath;
	std::vector<AudioSourceID> FlexEngine::s_AudioSourceIDs;

	// Globals declared in stdafx.hpp
	class Window* g_Window = nullptr;
	class CameraManager* g_CameraManager = nullptr;
	class InputManager* g_InputManager = nullptr;
	class Renderer* g_Renderer = nullptr;
	System* g_Systems[(i32)SystemType::_NONE];
	class FlexEngine* g_EngineInstance = nullptr;
	class Editor* g_Editor = nullptr;
	class SceneManager* g_SceneManager = nullptr;
	struct Monitor* g_Monitor = nullptr;
	class PhysicsManager* g_PhysicsManager = nullptr;
	class ResourceManager* g_ResourceManager = nullptr;

	sec g_SecElapsedSinceProgramStart = 0;
	sec g_DeltaTime = 0;
	sec g_UnpausedDeltaTime = 0;

	size_t g_TotalTrackedAllocatedMemory = 0;
	size_t g_TrackedAllocationCount = 0;
	size_t g_TrackedDeallocationCount = 0;

	FlexEngine::FlexEngine() :
		m_MouseButtonCallback(this, &FlexEngine::OnMouseButtonEvent),
		m_KeyEventCallback(this, &FlexEngine::OnKeyEvent),
		m_ActionCallback(this, &FlexEngine::OnActionEvent)
	{
		std::srand((u32)time(NULL));

		Platform::RetrievePathToExecutable();
		Platform::RetrieveCurrentWorkingDirectory();

		memset(m_CmdLineStrBuf, 0, MAX_CHARS_CMD_LINE_STR);

		{
			std::string configPathAbs = RelativePathToAbsolute(COMMON_CONFIG_LOCATION);
			m_CommonSettingsAbsFilePath = configPathAbs;
			m_CommonSettingsFileName = StripLeadingDirectories(configPathAbs);
			std::string configDirAbs = ExtractDirectoryString(configPathAbs);
			Platform::CreateDirectoryRecursive(configDirAbs);
		}

		{
			std::string bootupDirAbs = RelativePathToAbsolute(SAVED_LOCATION "");
			m_BootupTimesFileName = "bootup-times.log";
			m_BootupTimesAbsFilePath = bootupDirAbs + m_BootupTimesFileName;
			Platform::CreateDirectoryRecursive(bootupDirAbs);
		}

		{
			std::string renderDocSettingsDirAbs = RelativePathToAbsolute(CONFIG_DIRECTORY);
			m_RenderDocSettingsFileName = "renderdoc.json";
			m_RenderDocSettingsAbsFilePath = renderDocSettingsDirAbs + m_RenderDocSettingsFileName;
		}

		{
			// Default, can be overriden in common.json
			m_ShaderEditorPath = "C:/Program Files/Sublime Text 3/sublime_text.exe";
		}

#if COMPILE_OPEN_GL
		m_RendererName = "Open GL";
#elif COMPILE_VULKAN
		m_RendererName = "Vulkan";
#endif

#if defined(__clang__)
		m_CompilerName = "Clang";

		m_CompilerVersion =
			IntToString(__clang_major__) + '.' +
			IntToString(__clang_minor__) + '.' +
			IntToString(__clang_patchlevel__);
#elif defined(_MSC_VER)
		m_CompilerName = "MSVC";

#if _MSC_VER >= 1920
		m_CompilerVersion = "2019";
#elif _MSC_VER >= 1910
		m_CompilerVersion = "2017";
#elif _MSC_VER >= 1900
		m_CompilerVersion = "2015";
#elif _MSC_VER >= 1800
		m_CompilerVersion = "2013";
#else
		m_CompilerVersion = "Unknown";
#endif
#elif defined(__GNUC__)
		m_CompilerName = "GCC";

		m_CompilerVersion =
			IntToString(__GNUC__) + '.' +
			IntToString(__GNUC_MINOR__) + '.' +
			IntToString(__GNUC_PATCHLEVEL__);
#else
		m_CompilerName = "Unknown";
		m_CompilerVersion = "Unknown";
#endif

#ifdef DEBUG
		const char* configStr = "Debug";
#elif defined(PROFILE)
		const char* configStr = "Profile";
#elif defined(RELEASE)
#if defined(SYMBOLS)
		const char* configStr = "Shipping (with symbols)";
#else
		const char* configStr = "Shipping";
#endif
#else
		static_assert(false);
#endif

#if defined(FLEX_64)
		const char* targetStr = "x64";
#elif defined(FLEX_32)
		const char* targetStr = "x32";
#endif

#if defined(_WINDOWS)
		const char* platformStr = "Windows";
#elif defined(linux)
		const char* platformStr = "Linux";
#endif

		Print("FlexEngine v%u.%u.%u - Config: [%s %s, %s] - Compiler: [%s %s]\n", EngineVersionMajor, EngineVersionMinor, EngineVersionPatch, configStr, targetStr, platformStr, m_CompilerName.c_str(), m_CompilerVersion.c_str());
	}

	FlexEngine::~FlexEngine()
	{
		Destroy();
	}

	void FlexEngine::Initialize()
	{
		const char* engineInitBlockName = "Engine initialize";
		PROFILE_BEGIN(engineInitBlockName);

#if COMPILE_RENDERDOC_API
		SetupRenderDocAPI();
#endif

		g_EngineInstance = this;

		m_FrameTimes.resize(256);

		Platform::Init();

		CreateWindowAndRenderer();

		g_ResourceManager = new ResourceManager();
		g_ResourceManager->Initialize();

		g_Editor = new Editor();

		g_InputManager = new InputManager();

		g_CameraManager = new CameraManager();

		InitializeWindowAndRenderer();

		AudioManager::Initialize();

		Print("Bullet v%d\n", btGetVersion());

#if COMPILE_RENDERDOC_API
		if (m_RenderDocAPI != nullptr &&
			m_RenderDocAutoCaptureFrameCount != -1 &&
			m_RenderDocAutoCaptureFrameOffset == 0)
		{
			m_bRenderDocCapturingFrame = true;
			m_RenderDocAPI->StartFrameCapture(NULL, NULL);
		}
#endif

		g_PhysicsManager = new PhysicsManager();
		g_PhysicsManager->Initialize();

		g_Editor->Initialize();

		g_Systems[(i32)SystemType::PLUGGABLES] = new PluggablesSystem();
		g_Systems[(i32)SystemType::PLUGGABLES]->Initialize();

		g_Systems[(i32)SystemType::ROAD_MANAGER] = new RoadManager();
		g_Systems[(i32)SystemType::ROAD_MANAGER]->Initialize();

		g_Systems[(i32)SystemType::TERMINAL_MANAGER] = new TerminalManager();
		g_Systems[(i32)SystemType::TERMINAL_MANAGER]->Initialize();


		g_ResourceManager->DiscoverMeshes();
		g_ResourceManager->ParseMaterialsFile();
		g_ResourceManager->ParseFontFile();

		g_SceneManager = new SceneManager();
		g_SceneManager->AddFoundScenes();

		LoadCommonSettingsFromDisk();

		ImGui::CreateContext();
		SetupImGuiStyles();

		g_Editor->PostInitialize();

		g_Renderer->PostInitialize();

		g_InputManager->Initialize();

		g_InputManager->BindMouseButtonCallback(&m_MouseButtonCallback, 99);
		g_InputManager->BindKeyEventCallback(&m_KeyEventCallback, 10);
		g_InputManager->BindActionCallback(&m_ActionCallback, 10);

		if (s_AudioSourceIDs.empty())
		{
			s_AudioSourceIDs.push_back(AudioManager::AddAudioSource(SFX_DIRECTORY "dud_dud_dud_dud.wav"));
			s_AudioSourceIDs.push_back(AudioManager::AddAudioSource(SFX_DIRECTORY "drmapan.wav"));
			s_AudioSourceIDs.push_back(AudioManager::AddAudioSource(SFX_DIRECTORY "blip.wav"));
			s_AudioSourceIDs.push_back(AudioManager::SynthesizeSound(0.5f, 100.727f));
			s_AudioSourceIDs.push_back(AudioManager::SynthesizeSound(0.5f, 200.068f));
			s_AudioSourceIDs.push_back(AudioManager::SynthesizeSound(0.5f, 300.811f));
			s_AudioSourceIDs.push_back(AudioManager::SynthesizeSound(0.5f, 400.645f));
			s_AudioSourceIDs.push_back(AudioManager::SynthesizeSound(0.5f, 500.099f));
			s_AudioSourceIDs.push_back(AudioManager::SynthesizeSound(0.5f, 600.091f));
			s_AudioSourceIDs.push_back(AudioManager::SynthesizeSound(0.5f, 700.20f));
		}

		i32 springCount = 6;
		m_TestSprings.reserve(springCount);
		for (i32 i = 0; i < springCount; ++i)
		{
			m_TestSprings.emplace_back(Spring<glm::vec3>());
			m_TestSprings[i].DR = 0.9f;
			m_TestSprings[i].UAF = 15.0f;
		}

		PROFILE_END(engineInitBlockName);

		ms blockDuration = Profiler::GetBlockDuration(engineInitBlockName);
		if (blockDuration != -1.0f && blockDuration < 20000) // Exceptionally long times are almost always due to hitting breakpoints
		{
			std::string bootupTimesEntry = Platform::GetDateString_YMDHMS() + "," + FloatToString(blockDuration, 2);
			AppendToBootupTimesFile(bootupTimesEntry);
		}

		ImGuiIO& io = ImGui::GetIO();
		m_ImGuiIniFilepathStr = IMGUI_INI_LOCATION;
		io.IniFilename = m_ImGuiIniFilepathStr.c_str();
		m_ImGuiLogFilepathStr = IMGUI_LOG_LOCATION;
		io.LogFilename = m_ImGuiLogFilepathStr.c_str();
		io.DisplaySize = (ImVec2)g_Window->GetFrameBufferSize();
		io.IniSavingRate = 10.0f;


		memset(m_CmdLineStrBuf, 0, MAX_CHARS_CMD_LINE_STR);
		m_ConsoleCommands.emplace_back("reload.scene", []() { g_SceneManager->ReloadCurrentScene(); });
		m_ConsoleCommands.emplace_back("reload.scene.hard", []()
		{
			g_ResourceManager->DestroyAllLoadedMeshes();
			g_SceneManager->ReloadCurrentScene();
		});
		m_ConsoleCommands.emplace_back("reload.shaders", []() { g_Renderer->RecompileShaders(false); });
		m_ConsoleCommands.emplace_back("reload.shaders.force", []() { g_Renderer->RecompileShaders(true); });
		m_ConsoleCommands.emplace_back("reload.fontsdfs", []() { g_ResourceManager->LoadFonts(true); });
		m_ConsoleCommands.emplace_back("select.all", []() { g_Editor->SelectAll(); });
		m_ConsoleCommands.emplace_back("select.none", []() { g_Editor->SelectNone(); });
		m_ConsoleCommands.emplace_back("exit", []() { g_EngineInstance->Stop(); });
		// TODO: Support params like "aa=0"
		m_ConsoleCommands.emplace_back("aa.off", []() { g_Renderer->SetTAAEnabled(false); });
		m_ConsoleCommands.emplace_back("aa.taa", []() { g_Renderer->SetTAAEnabled(true); });
		m_ConsoleCommands.emplace_back("toggle.wireframe", []() { g_Renderer->ToggleWireframeOverlay(); });
		m_ConsoleCommands.emplace_back("toggle.wireframe.selection", []() { g_Renderer->ToggleWireframeSelectionOverlay(); });
	}

	AudioSourceID FlexEngine::GetAudioSourceID(SoundEffect effect)
	{
		assert((i32)effect >= 0);
		assert((i32)effect < (i32)SoundEffect::LAST_ELEMENT);

		return s_AudioSourceIDs[(i32)effect];
	}

	void FlexEngine::Destroy()
	{
		// TODO: Time engine destruction using non-glfw timer

#if COMPILE_RENDERDOC_API
		FreeLibrary(m_RenderDocModule);
#endif

		g_InputManager->UnbindMouseButtonCallback(&m_MouseButtonCallback);
		g_InputManager->UnbindKeyEventCallback(&m_KeyEventCallback);
		g_InputManager->UnbindActionCallback(&m_ActionCallback);

		SaveCommonSettingsToDisk(false);
		g_Window->SaveToConfig();

		g_Editor->Destroy();
		g_Renderer->DestroyPersistentObjects();
		g_SceneManager->DestroyAllScenes();
		g_CameraManager->Destroy();
		g_PhysicsManager->Destroy();
		g_ResourceManager->Destroy();
		DestroyWindowAndRenderer();
		g_ResourceManager->DestroyAllLoadedMeshes();

		AudioManager::Destroy();

		g_Systems[(i32)SystemType::ROAD_MANAGER]->Destroy();
		g_Systems[(i32)SystemType::ROAD_MANAGER] = nullptr;

		g_Systems[(i32)SystemType::PLUGGABLES]->Destroy();
		g_Systems[(i32)SystemType::PLUGGABLES] = nullptr;

		g_Systems[(i32)SystemType::TERMINAL_MANAGER]->Destroy();
		g_Systems[(i32)SystemType::TERMINAL_MANAGER] = nullptr;

		delete g_SceneManager;
		g_SceneManager = nullptr;

		delete g_PhysicsManager;
		g_PhysicsManager = nullptr;

		delete g_ResourceManager;
		g_ResourceManager = nullptr;

		delete g_CameraManager;
		g_CameraManager = nullptr;

		delete g_Editor;
		g_Editor = nullptr;

		delete g_Monitor;
		g_Monitor = nullptr;

		delete g_InputManager;
		g_InputManager = nullptr;


		// Reset console colour to default
		Print("\n");
	}

	void FlexEngine::CreateWindowAndRenderer()
	{
		assert(g_Window == nullptr);
		assert(g_Renderer == nullptr);

		const std::string titleString = "Flex Engine v" + EngineVersionString();

		if (g_Window == nullptr)
		{
			g_Window = new GLFWWindowWrapper(titleString);
		}
		if (g_Window == nullptr)
		{
			PrintError("Failed to create a window! Are any compile flags set in stdafx.hpp?\n");
			return;
		}

		g_Window->Initialize();
		g_Monitor = new Monitor();
		g_Window->RetrieveMonitorInfo();

		real desiredAspectRatio = 16.0f / 9.0f;
		real desiredWindowSizeScreenPercetange = 0.85f;

		// What kind of monitor has different scales along each axis?
		assert(g_Monitor->contentScaleX == g_Monitor->contentScaleY);

		i32 newWindowSizeY = i32(g_Monitor->height * desiredWindowSizeScreenPercetange * g_Monitor->contentScaleY);
		i32 newWindowSizeX = i32(newWindowSizeY * desiredAspectRatio);

		i32 newWindowPosX = i32(newWindowSizeX * 0.1f);
		i32 newWindowPosY = i32(newWindowSizeY * 0.1f);

		g_Window->Create(glm::vec2i(newWindowSizeX, newWindowSizeY), glm::vec2i(newWindowPosX, newWindowPosY));

#if COMPILE_OPEN_GL
		g_Renderer = new gl::GLRenderer();
#elif COMPILE_VULKAN
		g_Renderer = new vk::VulkanRenderer();
#endif
	}

	void FlexEngine::InitializeWindowAndRenderer()
	{
		g_Window->SetUpdateWindowTitleFrequency(0.5f);
		g_Window->PostInitialize();

		g_Renderer->Initialize();
	}

	void FlexEngine::DestroyWindowAndRenderer()
	{
		if (g_Renderer)
		{
			g_Renderer->Destroy();
			delete g_Renderer;
		}

		if (g_Window)
		{
			g_Window->Destroy();
			delete g_Window;
		}
	}

	void FlexEngine::OnSceneChanged()
	{
		SaveCommonSettingsToDisk(false);
	}

	bool FlexEngine::IsRenderingImGui() const
	{
		return m_bRenderImGui;
	}

	bool FlexEngine::IsRenderingEditorObjects() const
	{
		return m_bRenderEditorObjects;
	}

	void FlexEngine::SetRenderingEditorObjects(bool bRenderingEditorObjects)
	{
		m_bRenderEditorObjects = bRenderingEditorObjects;
	}

	void FlexEngine::UpdateAndRender()
	{
		m_bRunning = true;
		sec frameStartTime = Time::CurrentSeconds();
		while (m_bRunning)
		{
			sec now = Time::CurrentSeconds();

			if (m_bToggleRenderImGui)
			{
				m_bToggleRenderImGui = false;
				m_bRenderImGui = !m_bRenderImGui;
			}

			g_Window->PollEvents();

			sec frameEndTime = now;

			if (m_FramesToFakeDT > 0)
			{
				--m_FramesToFakeDT;
				// Pretend this frame started m_FakeDT seconds ago to prevent spike
				frameStartTime = frameEndTime - m_FakeDT;
			}

			sec secondsElapsed = frameEndTime - frameStartTime;
			frameStartTime = frameEndTime;


			g_UnpausedDeltaTime = glm::clamp(secondsElapsed, m_MinDT, m_MaxDT);
			g_SecElapsedSinceProgramStart = frameEndTime;

			// TODO: Pause audio when editor is paused
			if (m_bSimulationPaused && !m_bSimulateNextFrame)
			{
				g_DeltaTime = 0.0f;
			}
			else
			{
				g_DeltaTime = glm::clamp(secondsElapsed * m_SimulationSpeed, m_MinDT, m_MaxDT);
			}

			Profiler::StartFrame();

			if (!m_bSimulationPaused)
			{
				for (i32 i = 1; i < (i32)m_FrameTimes.size(); ++i)
				{
					m_FrameTimes[i - 1] = m_FrameTimes[i];
				}
				m_FrameTimes[m_FrameTimes.size() - 1] = g_UnpausedDeltaTime * 1000.0f;
			}

			// Update
			{
				PROFILE_BEGIN("Update");

				const glm::vec2i frameBufferSize = g_Window->GetFrameBufferSize();
				if (frameBufferSize.x == 0 || frameBufferSize.y == 0)
				{
					g_InputManager->ClearAllInputs();
				}

#if COMPILE_RENDERDOC_API
				if (m_RenderDocAPI != nullptr)
				{
					if (!m_bRenderDocCapturingFrame &&
						m_RenderDocAutoCaptureFrameOffset != -1 &&
						m_RenderDocAutoCaptureFrameCount != -1)
					{
						i32 frameIndex = g_Renderer->GetFramesRenderedCount();
						if (frameIndex >= m_RenderDocAutoCaptureFrameOffset &&
							frameIndex < m_RenderDocAutoCaptureFrameOffset + m_RenderDocAutoCaptureFrameCount)
						{
							m_bRenderDocTriggerCaptureNextFrame = true;
						}
					}

					if (m_bRenderDocTriggerCaptureNextFrame)
					{
						m_bRenderDocTriggerCaptureNextFrame = false;
						m_bRenderDocCapturingFrame = true;
						m_RenderDocAPI->StartFrameCapture(NULL, NULL);
					}
				}
#endif

				// Call as early as possible in the frame
				// Starts new ImGui frame and clears debug draw lines
				g_Renderer->NewFrame();

				SecSinceLogSave += g_UnpausedDeltaTime;
				if (SecSinceLogSave >= LogSaveRate)
				{
					SecSinceLogSave -= LogSaveRate;
					SaveLogBufferToFile();
				}

				if (m_bRenderImGui)
				{
					PROFILE_BEGIN("DrawImGuiObjects");
					DrawImGuiObjects();
					PROFILE_END("DrawImGuiObjects");
				}

				g_Editor->EarlyUpdate();

				Profiler::Update();

				const bool bSimulateFrame = (!m_bSimulationPaused || m_bSimulateNextFrame);
				m_bSimulateNextFrame = false;

				g_CameraManager->Update();

				if (bSimulateFrame)
				{
					g_CameraManager->CurrentCamera()->Update();

					g_SceneManager->CurrentScene()->Update();
					Player* p0 = g_SceneManager->CurrentScene()->GetPlayer(0);
					if (p0)
					{
						glm::vec3 targetPos = p0->GetTransform()->GetWorldPosition() + p0->GetTransform()->GetForward() * -2.0f;
						m_SpringTimer += g_DeltaTime;
						real amplitude = 1.5f;
						real period = 5.0f;
						if (m_SpringTimer > period)
						{
							m_SpringTimer -= period;
						}
						targetPos.y += pow(sin(glm::clamp(m_SpringTimer - period / 2.0f, 0.0f, PI)), 40.0f) * amplitude;
						glm::vec3 targetVel = ToVec3(p0->GetRigidBody()->GetRigidBodyInternal()->getLinearVelocity());

						for (Spring<glm::vec3>& spring : m_TestSprings)
						{
							spring.SetTargetPos(targetPos);
							spring.SetTargetVel(targetVel);

							targetPos = spring.pos;
							targetVel = spring.vel;
						}
					}

					AudioManager::Update();
				}
				else
				{
					// Simulation is paused
					if (!g_CameraManager->CurrentCamera()->bIsGameplayCam)
					{
						g_CameraManager->CurrentCamera()->Update();
					}
				}

#if 0
				btIDebugDraw* debugDrawer = g_Renderer->GetDebugDrawer();
				for (i32 i = 0; i < (i32)m_TestSprings.size(); ++i)
				{
					m_TestSprings[i].Tick(g_DeltaTime);
					real t = (real)i / (real)m_TestSprings.size();
					debugDrawer->drawSphere(ToBtVec3(m_TestSprings[i].pos), (1.0f - t + 0.1f) * 0.5f, btVector3(0.5f - 0.3f * t, 0.8f - 0.4f * t, 0.6f - 0.2f * t));
				}
#endif

				g_Window->Update();

				g_ResourceManager->Update();

				g_Editor->LateUpdate();

				if (bSimulateFrame)
				{
					for (u32 i = 0; i < (u32)SystemType::_NONE; ++i)
					{
						g_Systems[i]->Update();
					}

					g_SceneManager->CurrentScene()->LateUpdate();
				}

				g_Renderer->Update();

				g_InputManager->Update();

				g_InputManager->PostUpdate();
				PROFILE_END("Update");
			}

			PROFILE_BEGIN("Render");
			g_Renderer->Draw();
			PROFILE_END("Render");

#if COMPILE_RENDERDOC_API
			if (m_RenderDocAPI != nullptr && m_bRenderDocCapturingFrame)
			{
				m_bRenderDocCapturingFrame = false;
				Print("Capturing RenderDoc frame...\n");
				m_RenderDocAPI->EndFrameCapture(NULL, NULL);

				std::string captureFilePath;
				if (GetLatestRenderDocCaptureFilePath(captureFilePath))
				{
					g_Renderer->AddEditorString("Captured RenderDoc frame");

					const std::string captureFileName = StripLeadingDirectories(captureFilePath);
					Print("Captured RenderDoc frame to %s\n", captureFileName.c_str());

					CheckForRenderDocUIRunning();

					if (m_RenderDocUIPID == -1)
					{
						std::string cmdLineArgs = captureFilePath;
						m_RenderDocUIPID = m_RenderDocAPI->LaunchReplayUI(1, cmdLineArgs.c_str());
					}
				}
			}
#endif

			if (!m_bRenderImGui)
			{
				// Prevent mouse from being ignored while hovering over invisible ImGui elements
				ImGui::GetIO().WantCaptureMouse = false;
				ImGui::GetIO().WantCaptureKeyboard = false;
				ImGui::GetIO().WantSetMousePos = false;
			}

			m_SecondsSinceLastCommonSettingsFileSave += g_DeltaTime;
			if (m_SecondsSinceLastCommonSettingsFileSave > m_SecondsBetweenCommonSettingsFileSave)
			{
				m_SecondsSinceLastCommonSettingsFileSave = 0.0f;
				SaveCommonSettingsToDisk(false);
				g_Window->SaveToConfig();
			}

			const bool bProfileFrame = (g_Renderer->GetFramesRenderedCount() > 3);
			if (bProfileFrame)
			{
				Profiler::EndFrame(m_bUpdateProfilerFrame);
			}

			m_bUpdateProfilerFrame = false;

			if (m_bWriteProfilerResultsToFile)
			{
				m_bWriteProfilerResultsToFile = false;
				Profiler::PrintResultsToFile();
			}
		}
	}

	void FlexEngine::SetupImGuiStyles()
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = false;

		//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange; // glfwSetCursor overruns buffer somewhere (currently Window::m_FrameBufferSize.y...)

		std::string fontFilePath(FONT_DIRECTORY "lucon.ttf");
		io.Fonts->AddFontFromFileTTF(fontFilePath.c_str(), 13);

		io.FontGlobalScale = g_Monitor->contentScaleX;

		ImVec4* colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.85f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.10f, 0.90f);
		colors[ImGuiCol_Border] = ImVec4(0.70f, 0.70f, 0.70f, 0.40f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.30f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.80f, 0.80f, 0.40f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.90f, 0.65f, 0.65f, 0.45f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.73f, 0.34f, 0.00f, 0.94f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.76f, 0.46f, 0.19f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.74f, 0.33f, 0.09f, 0.20f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.60f, 0.18f, 0.04f, 0.55f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.25f, 0.30f, 0.60f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.75f, 0.40f, 0.40f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.80f, 0.75f, 0.41f, 0.50f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.92f, 0.82f, 0.29f, 0.60f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.97f, 0.54f, 0.03f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.82f, 0.61f, 0.37f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.95f, 0.53f, 0.22f, 0.60f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.82f, 0.49f, 0.20f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.71f, 0.37f, 0.11f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.66f, 0.32f, 0.17f, 0.76f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.74f, 0.43f, 0.29f, 0.76f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.60f, 0.23f, 0.07f, 0.80f);
		colors[ImGuiCol_Separator] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.70f, 0.62f, 0.60f, 1.00f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.90f, 0.78f, 0.70f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
		colors[ImGuiCol_Tab] = ImVec4(0.59f, 0.32f, 0.09f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.76f, 0.45f, 0.19f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.70f, 0.41f, 0.04f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.50f, 0.28f, 0.08f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.62f, 0.33f, 0.08f, 1.00f);
		//colors[ImGuiCol_DockingPreview] = ImVec4(0.83f, 0.44f, 0.11f, 0.70f);
		//colors[ImGuiCol_DockingBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.039f, 0.238f, 0.616f, 1.000f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 0.57f, 0.31f, 0.35f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	}

	void FlexEngine::DrawImGuiObjects()
	{
		glm::vec2i frameBufferSize = g_Window->GetFrameBufferSize();
		if (frameBufferSize.x == 0 || frameBufferSize.y == 0)
		{
			return;
		}

		if (m_bDemoWindowShowing)
		{
			ImGui::ShowDemoWindow(&m_bDemoWindowShowing);
		}

		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
#if COMPILE_RENDERDOC_API
				if (m_RenderDocAPI != nullptr && ImGui::MenuItem("Capture RenderDoc frame", "F9"))
				{
					m_bRenderDocTriggerCaptureNextFrame = true;
				}
#endif

				if (ImGui::MenuItem("Save scene", "Ctrl+S"))
				{
					g_SceneManager->CurrentScene()->SerializeToFile(false);
				}

				if (ImGui::BeginMenu("Reload"))
				{
					if (ImGui::MenuItem("Scene", "R"))
					{
						g_SceneManager->ReloadCurrentScene();
					}

					if (ImGui::MenuItem("Scene (hard: reload all meshes)"))
					{
						g_ResourceManager->DestroyAllLoadedMeshes();
						g_SceneManager->ReloadCurrentScene();
					}

					if (ImGui::MenuItem("Shaders", "Ctrl+R"))
					{
						g_Renderer->RecompileShaders(false);
					}

					if (ImGui::MenuItem("Shaders (force)"))
					{
						g_Renderer->RecompileShaders(true);
					}

					if (ImGui::MenuItem("Font textures (render SDFs)"))
					{
						g_ResourceManager->LoadFonts(true);
					}

					BaseScene* currentScene = g_SceneManager->CurrentScene();
					if (currentScene->HasPlayers() && ImGui::MenuItem("Player position(s)"))
					{
						if (currentScene->GetPlayer(0))
						{
							currentScene->GetPlayer(0)->GetController()->ResetTransformAndVelocities();
						}
						if (currentScene->GetPlayer(1))
						{
							currentScene->GetPlayer(1)->GetController()->ResetTransformAndVelocities();
						}
					}

					ImGui::EndMenu();
				}

				if (ImGui::MenuItem("Capture reflection probe"))
				{
					g_Renderer->RecaptureReflectionProbe();
				}

				ImGui::EndMenu();
			}

			const u32 buffSize = 256;
			const char* shaderEditorPopup = "Shader editor path##popup";
			static char shaderEditorBuf[buffSize];
			bool bOpenShaderEditorPathPopup = false;

#if COMPILE_RENDERDOC_API
			const char* renderDocDLLEditorPopup = "Renderdoc DLL path##popup";
			static char renderDocDLLBuf[buffSize];
			bool bOpenRenderDocDLLPathPopup = false;
#endif
			if (ImGui::BeginMenu("Edit"))
			{
				BaseScene* scene = g_SceneManager->CurrentScene();
				Player* player = scene->GetPlayer(0);
				if (player != nullptr && ImGui::BeginMenu("Add to inventory"))
				{
					if (ImGui::MenuItem("Cart"))
					{
						CartManager* cartManager = scene->GetCartManager();
						Cart* cart = cartManager->CreateCart(scene->GetUniqueObjectName("Cart_", 2));
						player->AddToInventory(cart);
					}

					if (ImGui::MenuItem("Engine cart"))
					{
						CartManager* cartManager = scene->GetCartManager();
						EngineCart* engineCart = cartManager->CreateEngineCart(scene->GetUniqueObjectName("EngineCart_", 2));
						player->AddToInventory(engineCart);
					}

					if (ImGui::MenuItem("Mobile liquid box"))
					{
						MobileLiquidBox* box = new MobileLiquidBox();
						scene->AddRootObject(box);
						player->AddToInventory(box);
					}

					ImGui::EndMenu();
				}

				if (ImGui::MenuItem("Shader editor path"))
				{
					bOpenShaderEditorPathPopup = true;
					memset(shaderEditorBuf, 0, buffSize);
					strcpy(shaderEditorBuf, m_ShaderEditorPath.c_str());
				}

#if COMPILE_RENDERDOC_API
				if (ImGui::MenuItem("Renderdoc DLL path"))
				{
					bOpenRenderDocDLLPathPopup = true;
					memset(renderDocDLLBuf, 0, buffSize);
					std::string renderDocDLLPath;
					ReadRenderDocSettingsFileFromDisk(renderDocDLLPath);
					strcpy(renderDocDLLBuf, renderDocDLLPath.c_str());
				}
#endif

				ImGui::EndMenu();
			}

			if (bOpenShaderEditorPathPopup)
			{
				ImGui::OpenPopup(shaderEditorPopup);
			}

			ImGui::SetNextWindowSize(ImVec2(500.0f, 80.0f), ImGuiCond_Appearing);
			if (ImGui::BeginPopupModal(shaderEditorPopup, NULL))
			{
				bool bUpdate = false;
				if (ImGui::InputText("", shaderEditorBuf, buffSize, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					bUpdate = true;
				}

				if (ImGui::Button("Cancel"))
				{
					ImGui::CloseCurrentPopup();
				}

				ImGui::SameLine();

				if (ImGui::Button("Confirm"))
				{
					bUpdate = true;
				}

				if (bUpdate)
				{
					m_ShaderEditorPath = std::string(shaderEditorBuf);
					SaveCommonSettingsToDisk(false);
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

#if COMPILE_RENDERDOC_API
			static bool bInvalidDLLLocation = false;
			if (bOpenRenderDocDLLPathPopup)
			{
				ImGui::OpenPopup(renderDocDLLEditorPopup);
				bInvalidDLLLocation = false;
			}

			ImGui::SetNextWindowSize(ImVec2(500.0f, 160.0f), ImGuiCond_Appearing);
			if (ImGui::BeginPopupModal(renderDocDLLEditorPopup, NULL))
			{
				bool bUpdate = false;
				if (ImGui::InputText("", renderDocDLLBuf, buffSize, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					bUpdate = true;
				}

				if (ImGui::Button("Cancel"))
				{
					ImGui::CloseCurrentPopup();
				}

				ImGui::SameLine();

				if (ImGui::Button("Confirm"))
				{
					bUpdate = true;
				}

				if (bInvalidDLLLocation)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
					ImGui::TextWrapped("Invalid path - couldn't find renderdoc.dll at location provided.");
					ImGui::PopStyleColor();
				}

				if (bUpdate)
				{
					bInvalidDLLLocation = false;
					std::string dllPathRaw = std::string(renderDocDLLBuf);
					std::string dllPathClean = dllPathRaw;

					dllPathClean = ReplaceBackSlashesWithForward(dllPathClean);

					if (!dllPathClean.empty() && dllPathClean.find('.') == std::string::npos && *(dllPathClean.end() - 1) != '/')
					{
						dllPathClean += "/";
					}

					dllPathClean = ExtractDirectoryString(dllPathClean);

					if (!dllPathClean.empty() && *(dllPathClean.end() - 1) != '/')
					{
						dllPathClean += "/";
					}

					if (!FileExists(dllPathClean + "renderdoc.dll"))
					{
						bInvalidDLLLocation = true;
					}

					if (!bInvalidDLLLocation)
					{
						SaveRenderDocSettingsFileToDisk(dllPathClean);
						ImGui::CloseCurrentPopup();
					}
				}

				ImGui::EndPopup();
			}
#endif

			if (ImGui::BeginMenu("Window"))
			{
				ImGui::MenuItem("Main Window", nullptr, &m_bMainWindowShowing);
				ImGui::MenuItem("GPU Timings", nullptr, &g_Renderer->bGPUTimingsWindowShowing);
				ImGui::MenuItem("Memory Stats", nullptr, &m_bShowMemoryStatsWindow);
				ImGui::MenuItem("CPU Stats", nullptr, &m_bShowCPUStatsWindow);
				ImGui::MenuItem("Uniform Buffers", nullptr, &g_Renderer->bUniformBufferWindowShowing);
				ImGui::Separator();
				ImGui::MenuItem("Materials", nullptr, &g_ResourceManager->bMaterialWindowShowing);
				ImGui::MenuItem("Shaders", nullptr, &g_ResourceManager->bShaderWindowShowing);
				ImGui::MenuItem("Textures", nullptr, &g_ResourceManager->bTextureWindowShowing);
				ImGui::MenuItem("Meshes", nullptr, &g_ResourceManager->bMeshWindowShowing);
				ImGui::MenuItem("Prefabs", nullptr, &g_ResourceManager->bPrefabsWindowShowing);
				ImGui::MenuItem("Sounds", nullptr, &g_ResourceManager->bSoundsWindowShowing);
				ImGui::Separator();
				ImGui::MenuItem("Key Mapper", nullptr, &m_bInputMapperShowing);
				ImGui::MenuItem("Font Editor", nullptr, &g_ResourceManager->bFontWindowShowing);
#if COMPILE_RENDERDOC_API
				ImGui::MenuItem("Render Doc Captures", nullptr, &m_bShowingRenderDocWindow);
#endif
				ImGui::Separator();
				ImGui::MenuItem("ImGui Demo Window", nullptr, &m_bDemoWindowShowing);

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				bool bPreviewShadows = g_Renderer->GetDisplayShadowCascadePreview();
				if (ImGui::MenuItem("Shadow cascades", nullptr, &bPreviewShadows))
				{
					g_Renderer->SetDisplayShadowCascadePreview(bPreviewShadows);
				}

				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}

		static auto windowSizeCallbackLambda = [](ImGuiSizeCallbackData* data)
		{
			FlexEngine* engine = static_cast<FlexEngine*>(data->UserData);
			engine->m_ImGuiMainWindowWidth = data->DesiredSize.x;
			engine->m_ImGuiMainWindowWidth = glm::min(engine->m_ImGuiMainWindowWidthMax,
				glm::max(engine->m_ImGuiMainWindowWidth, engine->m_ImGuiMainWindowWidthMin));
		};

		bool bIsMainWindowCollapsed = true;

		m_ImGuiMainWindowWidthMax = frameBufferSize.x - 100.0f;
		if (m_bMainWindowShowing)
		{
			static const std::string titleString = (std::string("Flex Engine v") + EngineVersionString());
			static const char* titleCharStr = titleString.c_str();
			ImGuiWindowFlags mainWindowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs;
			ImGui::SetNextWindowSizeConstraints(ImVec2(350, 300),
				ImVec2((real)frameBufferSize.x, (real)frameBufferSize.y),
				windowSizeCallbackLambda, this);
			real menuHeight = 20.0f;
			ImGui::SetNextWindowPos(ImVec2(0.0f, menuHeight), ImGuiCond_Once);
			real frameBufferHeight = (real)frameBufferSize.y;
			ImGui::SetNextWindowSize(ImVec2(m_ImGuiMainWindowWidth, frameBufferHeight - menuHeight),
				ImGuiCond_Always);
			if (ImGui::Begin(titleCharStr, &m_bMainWindowShowing, mainWindowFlags))
			{
				bIsMainWindowCollapsed = ImGui::IsWindowCollapsed();

				if (ImGui::TreeNode("Simulation"))
				{
					ImGui::Checkbox("Paused", &m_bSimulationPaused);

					if (ImGui::Button("Step (F10)"))
					{
						m_bSimulationPaused = true;
						m_bSimulateNextFrame = true;
					}

					if (ImGui::Button("Continue (F5)"))
					{
						m_bSimulationPaused = false;
						m_bSimulateNextFrame = true;
					}

					ImGui::SliderFloat("Speed", &m_SimulationSpeed, 0.0001f, 10.0f);

					if (ImGui::IsItemClicked(1))
					{
						m_SimulationSpeed = 1.0f;
					}

					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Stats"))
				{
					static const std::string rendererNameStringStr = std::string("Current renderer: " + m_RendererName);
					static const char* renderNameStr = rendererNameStringStr.c_str();
					ImGui::TextUnformatted(renderNameStr);
					static ms latestFrameTime;
					static u32 framesSinceUpdate = 0;
					if (framesSinceUpdate++ % 10 == 0)
					{
						latestFrameTime = m_FrameTimes[m_FrameTimes.size() - 1];
					}
					ImGui::Text("Frames time: %.1fms (%d FPS)", latestFrameTime, (u32)((1.0f / latestFrameTime) * 1000));
					ImGui::NewLine();
					ImGui::Text("Frames rendered: %d", g_Renderer->GetFramesRenderedCount());
					ImGui::Text("Unpaused elapsed time: %.2fs", g_SecElapsedSinceProgramStart);
					ImGui::Text("Audio effects loaded: %u", (u32)s_AudioSourceIDs.size());

					ImVec2 p = ImGui::GetCursorScreenPos();
					real width = 300.0f;
					real height = 100.0f;
					real minMS = 0.0f;
					real maxMS = 100.0f;
					ImGui::PlotLines("", m_FrameTimes.data(), (u32)m_FrameTimes.size(), 0, 0, minMS, maxMS, ImVec2(width, height));
					real targetFrameRate = 60.0f;
					p.y += (1.0f - (1000.0f / targetFrameRate) / (maxMS - minMS)) * height;
					ImGui::GetWindowDrawList()->AddLine(p, ImVec2(p.x + width, p.y), IM_COL32(128, 0, 0, 255), 1.0f);

					g_Renderer->DrawImGuiRendererInfo();

					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Misc settings"))
				{
					ImGui::Checkbox("Log to console", &g_bEnableLogToConsole);
					ImGui::Checkbox("Show profiler", &Profiler::s_bDisplayingFrame);
					ImGui::Checkbox("chrome://tracing recording", &Profiler::s_bRecordingTrace);

					if (ImGui::Button("Display latest frame"))
					{
						m_bUpdateProfilerFrame = true;
						Profiler::s_bDisplayingFrame = true;
					}

					ImGui::TreePop();
				}

				g_Renderer->DrawImGuiSettings();
				g_Window->DrawImGuiObjects();
				g_CameraManager->DrawImGuiObjects();
				g_SceneManager->DrawImGuiObjects();
				AudioManager::DrawImGuiObjects();

				if (ImGui::RadioButton("Translate", g_Editor->GetTransformState() == TransformState::TRANSLATE))
				{
					g_Editor->SetTransformState(TransformState::TRANSLATE);
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("Rotate", g_Editor->GetTransformState() == TransformState::ROTATE))
				{
					g_Editor->SetTransformState(TransformState::ROTATE);
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("Scale", g_Editor->GetTransformState() == TransformState::SCALE))
				{
					g_Editor->SetTransformState(TransformState::SCALE);
				}

				BaseScene* currentScene = g_SceneManager->CurrentScene();

				currentScene->DrawImGuiForSelectedObjects();
				currentScene->DrawImGuiForRenderObjectsList();

				ImGui::Spacing();
				ImGui::Spacing();
				ImGui::Spacing();

				ImGui::Text("Debugging");

				currentScene->GetTrackManager()->DrawImGuiObjects();
				currentScene->GetCartManager()->DrawImGuiObjects();

				for (u32 i = 0; i < (u32)SystemType::_NONE; ++i)
				{
					g_Systems[i]->DrawImGui();
				}

				if (ImGui::TreeNode("Spring"))
				{
					real* DR = &m_TestSprings[0].DR;
					real* UAF = &m_TestSprings[0].UAF;

					ImGui::SliderFloat("DR", DR, 0.0f, 2.0f);
					ImGui::SliderFloat("UAF", UAF, 0.0f, 20.0f);

					for (Spring<glm::vec3>& spring : m_TestSprings)
					{
						spring.DR = *DR;
						spring.UAF = *UAF;
					}

					ImGui::TreePop();
				}
			}
			ImGui::End();
		}

		g_SceneManager->DrawImGuiModals();

		g_Renderer->DrawImGuiWindows();

		g_ResourceManager->DrawImGuiWindows();

		if (m_bInputMapperShowing)
		{
			g_InputManager->DrawImGuiKeyMapper(&m_bInputMapperShowing);
		}

		if (m_bShowingConsole)
		{
			const real consoleWindowWidth = 350.0f;
			float fontScale = ImGui::GetIO().FontGlobalScale;
			real consoleWindowHeight = 28.0f + m_CmdAutoCompletions.size() * 16.0f * fontScale;
			const real consoleWindowX = (m_bMainWindowShowing && !bIsMainWindowCollapsed) ? m_ImGuiMainWindowWidth : 0.0f;
			const real consoleWindowY = frameBufferSize.y - consoleWindowHeight;
			ImGui::SetNextWindowPos(ImVec2(consoleWindowX, consoleWindowY), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(consoleWindowWidth, consoleWindowHeight));
			if (ImGui::Begin("Console", &m_bShowingConsole,
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
			{
				if (m_bShouldFocusKeyboardOnConsole)
				{
					m_bShouldFocusKeyboardOnConsole = false;
					ImGui::SetKeyboardFocusHere();
				}
				const bool bWasInvalid = m_bInvalidCmdLine;
				if (bWasInvalid)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
				}
				bool bFocusTextBox = false;
				for (u32 i = 0; i < (u32)m_CmdAutoCompletions.size(); ++i)
				{
					const std::string& str = m_CmdAutoCompletions[i];
					if (ImGui::Selectable(str.c_str(), i == (u32)m_SelectedCmdLineAutoCompleteIndex))
					{
						m_SelectedCmdLineAutoCompleteIndex = (i32)i;
						strncpy(m_CmdLineStrBuf, m_CmdAutoCompletions[i].c_str(), m_CmdAutoCompletions[i].size());
						bFocusTextBox = true;
					}
				}

				char cmdLineStrBufCopy[MAX_CHARS_CMD_LINE_STR];
				memcpy(cmdLineStrBufCopy, m_CmdLineStrBuf, MAX_CHARS_CMD_LINE_STR);
				if (ImGui::InputTextEx("", m_CmdLineStrBuf, MAX_CHARS_CMD_LINE_STR, ImVec2(consoleWindowWidth - 16.0f, consoleWindowHeight - 8.0f),
					ImGuiInputTextFlags_EnterReturnsTrue |
					ImGuiInputTextFlags_CallbackAlways |
					ImGuiInputTextFlags_CallbackHistory |
					ImGuiInputTextFlags_CallbackCompletion |
					ImGuiInputTextFlags_CallbackCharFilter |
					ImGuiInputTextFlags_DeleteCallback,
					[](ImGuiInputTextCallbackData* data) { return g_EngineInstance->ImGuiConsoleInputCallback(data); }))
				{
					m_bInvalidCmdLine = false;
					const std::string cmdLineStrBufClean = ToLower(m_CmdLineStrBuf);
					bool bMatched = false;
					for (const ConsoleCommand& cmd : m_ConsoleCommands)
					{
						if (strcmp(cmdLineStrBufClean.c_str(), cmd.name.c_str()) == 0)
						{
							bMatched = true;
							m_CmdAutoCompletions.clear();
							m_bShowingConsole = false;
							cmd.fun();
							if (m_PreviousCmdLineEntries.empty() ||
								strcmp((m_PreviousCmdLineEntries.end() - 1)->c_str(), m_CmdLineStrBuf) != 0)
							{
								m_PreviousCmdLineEntries.emplace_back(m_CmdLineStrBuf);
							}
							m_PreviousCmdLineIndex = -1;
							memset(m_CmdLineStrBuf, 0, MAX_CHARS_CMD_LINE_STR);
							break;
						}
					}
					if (memcmp(cmdLineStrBufCopy, m_CmdLineStrBuf, MAX_CHARS_CMD_LINE_STR))
					{
						m_bInvalidCmdLine = false;
					}
					if (!bMatched)
					{
						m_bInvalidCmdLine = true;
						m_bShouldFocusKeyboardOnConsole = true;
					}
				}
				if (bFocusTextBox)
				{
					ImGui::SetKeyboardFocusHere();
				}
				if (bWasInvalid)
				{
					ImGui::PopStyleColor();
				}
			}
			ImGui::End();
		}

#if COMPILE_RENDERDOC_API
		if (m_bShowingRenderDocWindow)
		{
			if (ImGui::Begin("RenderDoc", &m_bShowingRenderDocWindow))
			{
				ImGui::Text("RenderDoc connected - API v%i.%i.%i", m_RenderDocAPIVerionMajor, m_RenderDocAPIVerionMinor, m_RenderDocAPIVerionPatch);
				if (ImGui::Button("Trigger capture (F9)"))
				{
					m_bRenderDocTriggerCaptureNextFrame = true;
				}

				if (m_bRenderDocTriggerCaptureNextFrame || m_bRenderDocCapturingFrame)
				{
					ImGui::Text("Capturing frame...");
				}

				if (m_RenderDocUIPID == -1)
				{
					if (ImGui::Button("Launch QRenderDoc"))
					{
						std::string captureFilePath;
						GetLatestRenderDocCaptureFilePath(captureFilePath);

						CheckForRenderDocUIRunning();

						if (m_RenderDocUIPID == -1)
						{
							std::string cmdLineArgs = captureFilePath;
							m_RenderDocUIPID = m_RenderDocAPI->LaunchReplayUI(1, cmdLineArgs.c_str());
						}
					}
				}
				else
				{
					ImGui::Text("QRenderDoc is already running");

					// Only update periodically
					m_SecSinceRenderDocPIDCheck += g_DeltaTime;
					if (m_SecSinceRenderDocPIDCheck > 3.0f)
					{
						CheckForRenderDocUIRunning();
					}
				}
			}
			ImGui::End();
		}
#endif

		if (m_bShowMemoryStatsWindow)
		{
			if (ImGui::Begin("Memory", &m_bShowMemoryStatsWindow))
			{
				ImGui::Text("Memory allocated:       %.3fMB", g_TotalTrackedAllocatedMemory / 1'000'000.0f);
				ImGui::Text("Delta allocation count: %i", (i32)g_TrackedAllocationCount - (i32)g_TrackedDeallocationCount);
			}
			ImGui::End();
		}

		if (m_bShowCPUStatsWindow)
		{
			if (ImGui::Begin("CPU Stats", &m_bShowCPUStatsWindow))
			{
				ImGui::Text("Logical processor count: %u", Platform::cpuInfo.logicalProcessorCount);
				ImGui::Text("Physical core count: %u", Platform::cpuInfo.physicalCoreCount);
				ImGui::Separator();
				ImGui::Text("L1 cache count: %u", Platform::cpuInfo.l1CacheCount);
				ImGui::Text("L2 cache count: %u", Platform::cpuInfo.l2CacheCount);
				ImGui::Text("L3 cache count: %u", Platform::cpuInfo.l3CacheCount);
			}
			ImGui::End();
		}
	}

	i32 FlexEngine::ImGuiConsoleInputCallback(ImGuiInputTextCallbackData* data)
	{
		const i32 cmdHistCount = (i32)m_PreviousCmdLineEntries.size();
		if (data->EventFlag != ImGuiInputTextFlags_CallbackAlways)
		{
			m_bInvalidCmdLine = false;
		}

		auto FillOutAutoCompletions = [this](const char* buffer)
		{
			m_PreviousCmdLineIndex = -1;

			m_CmdAutoCompletions.clear();

			if (!m_bShowAllConsoleCommands && strlen(buffer) == 0)
			{
				// Don't show suggestions with empty buffer to allow navigating history
				return;
			}

			for (const ConsoleCommand& cmd : m_ConsoleCommands)
			{
				if (StartsWith(cmd.name, buffer))
				{
					m_CmdAutoCompletions.push_back(cmd.name);
				}
			}
		};

		if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
		{
			if (m_CmdAutoCompletions.empty())
			{
				if (data->EventKey == ImGuiKey_UpArrow)
				{
					if (cmdHistCount == 0)
					{
						return -1;
					}

					if (m_PreviousCmdLineIndex == cmdHistCount - 1)
					{
						return 0;
					}

					data->DeleteChars(0, data->BufTextLen);
					++m_PreviousCmdLineIndex;
					data->InsertChars(0, m_PreviousCmdLineEntries[cmdHistCount - m_PreviousCmdLineIndex - 1].data());
				}
				else if (data->EventKey == ImGuiKey_DownArrow)
				{
					if (cmdHistCount == 0)
					{
						return -1;
					}

					if (m_PreviousCmdLineIndex == -1)
					{
						return 0;
					}

					data->DeleteChars(0, data->BufTextLen);
					--m_PreviousCmdLineIndex;
					if (m_PreviousCmdLineIndex != -1) // -1 leaves console cleared
					{
						data->InsertChars(0, m_PreviousCmdLineEntries[cmdHistCount - m_PreviousCmdLineIndex - 1].data());
					}
				}
				return 0;
			}
			else // Select auto completion entry
			{
				m_PreviousCmdLineIndex = -1;

				if (data->EventKey == ImGuiKey_UpArrow)
				{
					if (m_SelectedCmdLineAutoCompleteIndex == -1)
					{
						m_SelectedCmdLineAutoCompleteIndex = (i32)m_CmdAutoCompletions.size() - 1;
					}
					else
					{
						--m_SelectedCmdLineAutoCompleteIndex;
						m_SelectedCmdLineAutoCompleteIndex = glm::max(m_SelectedCmdLineAutoCompleteIndex, 0);
					}
				}
				else if (data->EventKey == ImGuiKey_DownArrow)
				{
					if (m_SelectedCmdLineAutoCompleteIndex != -1)
					{
						++m_SelectedCmdLineAutoCompleteIndex;
						if (m_SelectedCmdLineAutoCompleteIndex >= ((i32)m_CmdAutoCompletions.size()))
						{
							m_SelectedCmdLineAutoCompleteIndex = -1;
						}
					}
				}
			}
		}
		else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
		{
			if (data->BufTextLen > 0)
			{
				if (m_SelectedCmdLineAutoCompleteIndex != -1)
				{
					const std::string& cmdStr = m_CmdAutoCompletions[m_SelectedCmdLineAutoCompleteIndex];
					data->DeleteChars(0, data->BufTextLen);
					data->InsertChars(0, cmdStr.data());
				}
				else
				{
					for (const ConsoleCommand& cmd : m_ConsoleCommands)
					{
						if (StartsWith(cmd.name, data->Buf) && strcmp(cmd.name.c_str(), data->Buf) != 0)
						{
							data->DeleteChars(0, data->BufTextLen);
							data->InsertChars(0, cmd.name.data());
							break;
						}
					}
				}

				FillOutAutoCompletions(data->Buf);
			}
			return 0;
		}
		else if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter)
		{
			if (data->EventChar == L'`' ||
				data->EventChar == L'~')
			{
				m_bShowingConsole = false;
				m_SelectedCmdLineAutoCompleteIndex = -1;
				return 1;
			}

			char eventChar = (char)data->EventChar;
			// TODO: Insert at data->CursorPos
			char* cmdLine = strncat(m_CmdLineStrBuf, &eventChar, 1);

			if (m_SelectedCmdLineAutoCompleteIndex != -1)
			{
				if (!StartsWith(m_CmdAutoCompletions[m_SelectedCmdLineAutoCompleteIndex], cmdLine))
				{
					m_SelectedCmdLineAutoCompleteIndex = -1;
				}
			}

			FillOutAutoCompletions(cmdLine);
			if (m_SelectedCmdLineAutoCompleteIndex == -1)
			{
				m_SelectedCmdLineAutoCompleteIndex = 0;
			}
		}
		else if (data->EventFlag == ImGuiInputTextFlags_DeleteCallback)
		{
			char cmdLine[128];
			strncpy(cmdLine, m_CmdLineStrBuf, ARRAY_LENGTH(cmdLine));

			// Delete char
			if (strlen(m_CmdLineStrBuf) == 1)
			{
				memset(cmdLine, 0, ARRAY_LENGTH(cmdLine));
			}
			else
			{
				memmove(&cmdLine[data->CursorPos + 1], &cmdLine[data->CursorPos + 2], strlen(m_CmdLineStrBuf) - data->CursorPos);
			}

			FillOutAutoCompletions(cmdLine);
			if (strlen(cmdLine) == 0)
			{
				m_SelectedCmdLineAutoCompleteIndex = -1;
			}
			else if (m_SelectedCmdLineAutoCompleteIndex == -1)
			{
				m_SelectedCmdLineAutoCompleteIndex = 0;
			}
		}
		else
		{
			if (m_PreviousCmdLineIndex == -1)
			{
				FillOutAutoCompletions(m_CmdLineStrBuf);
			}
		}

		if (m_SelectedCmdLineAutoCompleteIndex > ((i32)m_CmdAutoCompletions.size()) - 1)
		{
			m_SelectedCmdLineAutoCompleteIndex = -1;
		}

		return 0;
	}

	void FlexEngine::Stop()
	{
		m_bRunning = false;
	}

	bool FlexEngine::LoadCommonSettingsFromDisk()
	{
		if (m_CommonSettingsAbsFilePath.empty())
		{
			PrintError("Failed to read common settings to disk: file path is not set!\n");
			return false;
		}

		if (FileExists(m_CommonSettingsAbsFilePath))
		{
			if (g_bEnableLogging_Loading)
			{
				Print("Loading common settings from %s\n", m_CommonSettingsFileName.c_str());
			}

			JSONObject rootObject = {};

			if (JSONParser::ParseFromFile(m_CommonSettingsAbsFilePath, rootObject))
			{
				std::string lastOpenedSceneName = rootObject.GetString("last opened scene");
				if (!lastOpenedSceneName.empty())
				{
					// Don't print errors if not found, file might have been deleted since last session
					if (!g_SceneManager->SetCurrentScene(lastOpenedSceneName, false))
					{
						g_SceneManager->SetNextSceneActive();
					}
				}

				bool bRenderImGui;
				if (rootObject.SetBoolChecked("render imgui", bRenderImGui))
				{
					m_bRenderImGui = bRenderImGui;
				}

				real masterGain;
				if (rootObject.SetFloatChecked("master gain", masterGain))
				{
					AudioManager::SetMasterGain(masterGain);
				}

				bool bMuted;
				if (rootObject.SetBoolChecked("muted", bMuted))
				{
					AudioManager::SetMuted(bMuted);
				}

				rootObject.SetBoolChecked("install shader directory watch", m_bInstallShaderDirectoryWatch);

				std::string shaderEditorPath;
				if (rootObject.SetStringChecked("shader editor path", shaderEditorPath))
				{
					m_ShaderEditorPath = shaderEditorPath;
				}

				return true;
			}
			else
			{
				PrintError("Failed to parse common settings config file %s\n\terror: %s\n", m_CommonSettingsAbsFilePath.c_str(), JSONParser::GetErrorString());
				return false;
			}
		}

		return false;
	}

	// TODO: EZ: Add config window to set common settings
	void FlexEngine::SaveCommonSettingsToDisk(bool bAddEditorStr)
	{
		if (m_CommonSettingsAbsFilePath.empty())
		{
			PrintError("Failed to save common settings to disk: file path is not set!\n");
			return;
		}

		JSONObject rootObject = {};

		std::string lastOpenedSceneName = g_SceneManager->CurrentScene()->GetFileName();
		rootObject.fields.emplace_back("last opened scene", JSONValue(lastOpenedSceneName));

		rootObject.fields.emplace_back("render imgui", JSONValue(m_bRenderImGui));

		rootObject.fields.emplace_back("master gain", JSONValue(AudioManager::GetMasterGain()));
		rootObject.fields.emplace_back("muted", JSONValue(AudioManager::IsMuted()));

		rootObject.fields.emplace_back("install shader directory watch", JSONValue(m_bInstallShaderDirectoryWatch));

		rootObject.fields.emplace_back("shader editor path", JSONValue(m_ShaderEditorPath));

		std::string fileContents = rootObject.Print(0);

		if (!WriteFile(m_CommonSettingsAbsFilePath, fileContents, false))
		{
			PrintError("Failed to write common settings config file\n");
		}

		if (bAddEditorStr)
		{
			g_Renderer->AddEditorString("Saved common settings");
		}
	}

	void FlexEngine::AppendToBootupTimesFile(const std::string& entry)
	{
		std::string newFileContents;
		if (FileExists(m_BootupTimesAbsFilePath))
		{
			if (!ReadFile(m_BootupTimesAbsFilePath, newFileContents, false))
			{
				PrintWarn("Failed to read bootup times file: %s\n", m_BootupTimesAbsFilePath.c_str());
				return;
			}
		}

		newFileContents += entry + std::string("\n");

		if (!WriteFile(m_BootupTimesAbsFilePath, newFileContents, false))
		{
			PrintWarn("Failed to write bootup times file: %s\n", m_BootupTimesAbsFilePath.c_str());
		}
	}

	void FlexEngine::SetFramesToFakeDT(i32 frameCount)
	{
		m_FramesToFakeDT = frameCount;
	}

	real FlexEngine::GetSimulationSpeed() const
	{
		return m_SimulationSpeed;
	}

	std::string FlexEngine::EngineVersionString()
	{
		return IntToString(EngineVersionMajor) + "." +
			IntToString(EngineVersionMinor) + "." +
			IntToString(EngineVersionPatch);
	}

	std::string FlexEngine::GetShaderEditorPath()
	{
		return m_ShaderEditorPath;
	}

	void FlexEngine::CreateCameraInstances()
	{
		FirstPersonCamera* fpCamera = new FirstPersonCamera();
		g_CameraManager->AddCamera(fpCamera, true);

		DebugCamera* debugCamera = new DebugCamera();
		debugCamera->position = glm::vec3(20.0f, 8.0f, -16.0f);
		debugCamera->yaw = glm::radians(130.0f);
		debugCamera->pitch = glm::radians(-10.0f);
		g_CameraManager->AddCamera(debugCamera, false);

		OverheadCamera* overheadCamera = new OverheadCamera();
		g_CameraManager->AddCamera(overheadCamera, false);

		TerminalCamera* terminalCamera = new TerminalCamera();
		g_CameraManager->AddCamera(terminalCamera, false);

		VehicleCamera* vehicleCamera = new VehicleCamera();
		g_CameraManager->AddCamera(vehicleCamera, false);
	}

	EventReply FlexEngine::OnMouseButtonEvent(MouseButton button, KeyAction action)
	{
		FLEX_UNUSED(button);
		FLEX_UNUSED(action);
		return EventReply::UNCONSUMED;
	}

	EventReply FlexEngine::OnKeyEvent(KeyCode keyCode, KeyAction action, i32 modifiers)
	{
		const bool bControlDown = (modifiers & (i32)InputModifier::CONTROL) > 0;
		const bool bAltDown = (modifiers & (i32)InputModifier::ALT) > 0;
		const bool bShiftDown = (modifiers & (i32)InputModifier::SHIFT) > 0;

		if (action == KeyAction::KEY_PRESS)
		{
			if (keyCode == KeyCode::KEY_GRAVE_ACCENT)
			{
				m_bShowingConsole = !m_bShowingConsole;
				m_bShowAllConsoleCommands = bShiftDown;
				m_CmdAutoCompletions.clear();
				m_PreviousCmdLineIndex = -1;
				m_SelectedCmdLineAutoCompleteIndex = -1;
				if (m_bShowingConsole)
				{
					m_bShouldFocusKeyboardOnConsole = true;
				}
			}

#if COMPILE_RENDERDOC_API
			if (m_RenderDocAPI != nullptr &&
				!m_bRenderDocCapturingFrame &&
				!m_bRenderDocTriggerCaptureNextFrame &&
				keyCode == KeyCode::KEY_F9)
			{
				m_bRenderDocTriggerCaptureNextFrame = true;
				return EventReply::CONSUMED;
			}
#endif

			if (keyCode == KeyCode::KEY_F5)
			{
				m_bSimulationPaused = false;
				m_bSimulateNextFrame = true;
				return EventReply::CONSUMED;
			}

			if (keyCode == KeyCode::KEY_G)
			{
				g_Renderer->ToggleRenderGrid();
				return EventReply::CONSUMED;
			}

			// TODO: Handle elsewhere to handle ignoring ImGuiIO::WantCaptureKeyboard?
			if (bShiftDown && keyCode == KeyCode::KEY_1)
			{
				m_bToggleRenderImGui = true;
				return EventReply::CONSUMED;
			}

			if (keyCode == KeyCode::KEY_R)
			{
				if (bControlDown)
				{
					g_Renderer->RecompileShaders(false);
					return EventReply::CONSUMED;
				}
				else
				{
					g_InputManager->ClearAllInputs();

					g_SceneManager->ReloadCurrentScene();
					return EventReply::CONSUMED;
				}
			}

			if (keyCode == KeyCode::KEY_P)
			{
				PhysicsDebuggingSettings& settings = g_Renderer->GetPhysicsDebuggingSettings();
				settings.bDrawWireframe = !settings.bDrawWireframe;
				return EventReply::CONSUMED;
			}

			if (keyCode == KeyCode::KEY_F11 ||
				(bAltDown && keyCode == KeyCode::KEY_ENTER))
			{
				g_Window->ToggleFullscreen();
				return EventReply::CONSUMED;
			}

			if (keyCode == KeyCode::KEY_4)
			{
				AudioManager::PlaySource(s_AudioSourceIDs[(i32)SoundEffect::synthesized_01]);
				return EventReply::CONSUMED;
			}
			if (keyCode == KeyCode::KEY_5)
			{
				AudioManager::PlaySource(s_AudioSourceIDs[(i32)SoundEffect::synthesized_02]);
				return EventReply::CONSUMED;
			}
			if (keyCode == KeyCode::KEY_6)
			{
				AudioManager::PlaySource(s_AudioSourceIDs[(i32)SoundEffect::synthesized_03]);
				return EventReply::CONSUMED;
			}
			if (keyCode == KeyCode::KEY_7)
			{
				AudioManager::PlaySource(s_AudioSourceIDs[(i32)SoundEffect::synthesized_04]);
				return EventReply::CONSUMED;
			}
			if (keyCode == KeyCode::KEY_8)
			{
				AudioManager::PlaySource(s_AudioSourceIDs[(i32)SoundEffect::synthesized_05]);
				return EventReply::CONSUMED;
			}
			if (keyCode == KeyCode::KEY_9)
			{
				AudioManager::PlaySource(s_AudioSourceIDs[(i32)SoundEffect::synthesized_06]);
				return EventReply::CONSUMED;
			}
			if (keyCode == KeyCode::KEY_0)
			{
				AudioManager::PlaySource(s_AudioSourceIDs[(i32)SoundEffect::synthesized_07]);
				return EventReply::CONSUMED;
			}

			if (keyCode == KeyCode::KEY_K)
			{
				m_bWriteProfilerResultsToFile = true;
				return EventReply::CONSUMED;
			}
		}

		if (action == KeyAction::KEY_PRESS || action == KeyAction::KEY_REPEAT)
		{
			if (keyCode == KeyCode::KEY_F10)
			{
				m_bSimulationPaused = true;
				m_bSimulateNextFrame = true;
				return EventReply::CONSUMED;
			}
		}

		return EventReply::UNCONSUMED;
	}

	EventReply FlexEngine::OnActionEvent(Action action)
	{
		if (action == Action::DBG_ENTER_NEXT_SCENE)
		{
			g_SceneManager->SetNextSceneActive();
			g_SceneManager->InitializeCurrentScene();
			g_SceneManager->PostInitializeCurrentScene();
			return EventReply::CONSUMED;
		}
		else if (action == Action::DBG_ENTER_PREV_SCENE)
		{
			g_SceneManager->SetPreviousSceneActive();
			g_SceneManager->InitializeCurrentScene();
			g_SceneManager->PostInitializeCurrentScene();
			return EventReply::CONSUMED;
		}

		return EventReply::UNCONSUMED;
	}

#if COMPILE_RENDERDOC_API
	void FlexEngine::SetupRenderDocAPI()
	{
		std::string dllDirPath;

		if (!ReadRenderDocSettingsFileFromDisk(dllDirPath))
		{
			PrintError("Unable to setup RenderDoc API - settings file missing.\n"
				"Set path to DLL under Edit > Renderdoc DLL path\n");
			return;
		}

		if (!EndsWith(dllDirPath, "/"))
		{
			dllDirPath += "/";
		}

		std::string dllPath = dllDirPath + "renderdoc.dll";

		if (FileExists(dllPath))
		{
			m_RenderDocModule = LoadLibraryA(dllPath.c_str());

			if (m_RenderDocModule == NULL)
			{
#if defined(FLEX_64)
				const char* targetStr = "x64";
#elif defined(FLEX_32)
				const char* targetStr = "x32";
#endif

				PrintWarn("Found render doc dll but failed to load it. Is the bitness correct? (must be %s)\n", targetStr);
				return;
			}

			pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(m_RenderDocModule, "RENDERDOC_GetAPI");
			int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_0, (void**)&m_RenderDocAPI);
			assert(ret == 1);

			m_RenderDocAPI->GetAPIVersion(&m_RenderDocAPIVerionMajor, &m_RenderDocAPIVerionMinor, &m_RenderDocAPIVerionPatch);
			Print("### RenderDoc API v%i.%i.%i connected, F9 to capture ###\n", m_RenderDocAPIVerionMajor, m_RenderDocAPIVerionMinor, m_RenderDocAPIVerionPatch);

			if (m_RenderDocAutoCaptureFrameOffset != -1 &&
				m_RenderDocAutoCaptureFrameCount != -1)
			{
				Print("Auto capturing %i frame(s) starting at frame %i\n", m_RenderDocAutoCaptureFrameCount, m_RenderDocAutoCaptureFrameOffset);
			}

			const std::string dateStr = Platform::GetDateString_YMDHMS();
			const std::string captureFilePath = RelativePathToAbsolute(SAVED_LOCATION "RenderDocCaptures/FlexEngine_" + dateStr);
			m_RenderDocAPI->SetCaptureFilePathTemplate(captureFilePath.c_str());

			m_RenderDocAPI->MaskOverlayBits(eRENDERDOC_Overlay_None, 0);
			m_RenderDocAPI->SetCaptureKeys(nullptr, 0);
			m_RenderDocAPI->SetFocusToggleKeys(nullptr, 0);

			m_RenderDocAPI->SetCaptureOptionU32(eRENDERDOC_Option_DebugOutputMute, 1);
		}
		else
		{
			PrintError("Unable to find renderdoc dll at %s\n", dllPath.c_str());
		}
	}

	void FlexEngine::CheckForRenderDocUIRunning()
	{
		if (m_RenderDocUIPID != -1)
		{
			if (!Platform::IsProcessRunning(m_RenderDocUIPID))
			{
				m_RenderDocUIPID = -1;
			}
		}
	}

	bool FlexEngine::GetLatestRenderDocCaptureFilePath(std::string& outFilePath)
	{
		u32 bufferSize;
		u32 captureIndex = 0;
		m_RenderDocAPI->GetCapture(captureIndex, nullptr, &bufferSize, nullptr);
		std::string captureFilePath(bufferSize, '\0');
		u32 result = m_RenderDocAPI->GetCapture(captureIndex, (char*)captureFilePath.data(), &bufferSize, nullptr);
		if (result != 0 && bufferSize != 0)
		{
			captureFilePath.pop_back(); // Remove trailing null terminator
			outFilePath = captureFilePath;
			return true;
		}

		return false;
	}

	bool FlexEngine::ReadRenderDocSettingsFileFromDisk(std::string& dllDirPathOut)
	{
		if (FileExists(m_RenderDocSettingsAbsFilePath))
		{
			JSONObject rootObject;
			if (JSONParser::ParseFromFile(m_RenderDocSettingsAbsFilePath, rootObject))
			{
				dllDirPathOut = rootObject.GetString("lib path");
				return true;
			}
			else
			{
				PrintError("Failed to parse %s\n\terror: %s\n", m_RenderDocSettingsFileName.c_str(), JSONParser::GetErrorString());
			}
		}
		return false;
	}

	void FlexEngine::SaveRenderDocSettingsFileToDisk(const std::string& dllDir)
	{
		JSONObject rootObject;
		rootObject.fields.emplace_back("lib path", JSONValue(dllDir));
		WriteFile(m_RenderDocSettingsAbsFilePath, rootObject.Print(0), false);
	}
#endif // COMPILE_RENDERDOC_API

	glm::vec3 FlexEngine::CalculateRayPlaneIntersectionAlongAxis(
		const glm::vec3& axis,
		const glm::vec3& rayOrigin,
		const glm::vec3& rayEnd,
		const glm::vec3& planeOrigin,
		const glm::vec3& planeNorm,
		const glm::vec3& startPos,
		const glm::vec3& cameraForward,
		real& inOutOffset,
		bool recalculateOffset,
		glm::vec3& inOutPrevIntersectionPoint,
		glm::vec3* outTrueIntersectionPoint)
	{
		glm::vec3 rayDir = glm::normalize(rayEnd - rayOrigin);
		glm::vec3 planeN = planeNorm;
		if (glm::dot(planeN, cameraForward) > 0.0f)
		{
			planeN = -planeN;
		}
		real intersectionDistance;
		if (glm::intersectRayPlane(rayOrigin, rayDir, planeOrigin, planeN, intersectionDistance))
		{
			glm::vec3 intersectionPoint = rayOrigin + rayDir * intersectionDistance;
			if (outTrueIntersectionPoint)
			{
				*outTrueIntersectionPoint = intersectionPoint;
			}
			if (recalculateOffset) // Mouse was clicked or wrapped
			{
				//real oldOffset = glm::dot(inOutPrevIntersectionPoint - startPos, axis);
				inOutOffset = glm::dot(intersectionPoint - startPos, axis);
				//Print("(%.2f) => (%.2f)\n", oldOffset, inOutOffset);
			}
			inOutPrevIntersectionPoint = intersectionPoint;

			glm::vec3 constrainedPoint = planeOrigin + (glm::dot(intersectionPoint - planeOrigin, axis) - inOutOffset) * axis;
			return constrainedPoint;
		}

		return VEC3_ZERO;
	}

	void FlexEngine::GenerateRayAtMousePos(btVector3& outRayStart, btVector3& outRayEnd)
	{
		PhysicsWorld* physicsWorld = g_SceneManager->CurrentScene()->GetPhysicsWorld();

		const real maxDist = 1000.0f;
		outRayStart = ToBtVec3(g_CameraManager->CurrentCamera()->position);
		glm::vec2 mousePos = g_InputManager->GetMousePosition();
		btVector3 rayDir = physicsWorld->GenerateDirectionRayFromScreenPos((i32)mousePos.x, (i32)mousePos.y);
		outRayEnd = outRayStart + rayDir * maxDist;
	}

	bool FlexEngine::IsSimulationPaused() const
	{
		return m_bSimulationPaused;
	}

	bool FlexEngine::InstallShaderDirectoryWatch() const
	{
#if COMPILE_SHADER_COMPILER
		return m_bInstallShaderDirectoryWatch;
#else
		return false;
#endif
	}

} // namespace flex
