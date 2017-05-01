#include "stdafx.h"

#include "TechDemo.h"
#include "Logger.h"
#include "FreeCamera.h"
#include "GameContext.h"
#include "InputManager.h"
#include "Scene/SceneManager.h"
#include "Scene/TestScene.h"
#include "Typedefs.h"

using namespace glm;

TechDemo::TechDemo()
{
}

TechDemo::~TechDemo()
{
	Destroy();
}

void TechDemo::Initialize()
{
	m_GameContext = {};
	m_GameContext.mainApp = this;

//#if COMPILE_VULKAN
//	m_Window = new VulkanWindowWrapper("Tech Demo - Vulkan", vec2i(1920, 1080), m_GameContext);
//	VulkanRenderer* renderer = new VulkanRenderer(m_GameContext);
//#elif COMPILE_OPEN_GL
//	m_Window = new GLWindowWrapper("Tech Demo - OpenGL", vec2i(1920, 1080), m_GameContext);
//	GLRenderer* renderer = new GLRenderer(m_GameContext);
//#elif COMPILE_D3D
//	m_Window = new D3DWindowWrapper("Tech Demo - Direct3D", vec2i(1920, 1080), m_GameContext);
//	D3DRenderer* renderer = new D3DRenderer(m_GameContext);
//#endif

	const vec2i windowSize = vec2i(1920, 1080);

#if COMPILE_VULKAN
	if (m_RendererIndex == 0)
	{
		VulkanWindowWrapper* vulkanWindow = new VulkanWindowWrapper("Tech Demo - Vulkan", windowSize, m_GameContext);
		m_Window = vulkanWindow;
		VulkanRenderer* vulkanRenderer = new VulkanRenderer(m_GameContext);
		m_GameContext.renderer = vulkanRenderer;
	}
#endif
#if COMPILE_OPEN_GL
	if (m_RendererIndex == 1)
	{
		GLWindowWrapper* glWindow = new GLWindowWrapper("Tech Demo - OpenGL", windowSize, m_GameContext);
		m_Window = glWindow;
		GLRenderer* glRenderer = new GLRenderer(m_GameContext);
		m_GameContext.renderer = glRenderer;
	}
#endif
#if COMPILE_D3D
	if (m_RendererIndex == 2)
	{
		D3DWindowWrapper* d3dWindow = new D3DWindowWrapper("Tech Demo - Direct3D", windowSize, m_GameContext);
		m_Window = d3dWindow;
		D3DRenderer* d3dRenderer = new D3DRenderer(m_GameContext);
		m_GameContext.renderer = d3dRenderer;
	}
#endif

	m_Window->SetUpdateWindowTitleFrequency(0.4f);

	m_GameContext.renderer->SetVSyncEnabled(false);
	m_GameContext.renderer->SetClearColor(0.08f, 0.13f, 0.2f);

	m_SceneManager = new SceneManager();
	TestScene* testScene = new TestScene(m_GameContext);
	m_SceneManager->AddScene(testScene);

	m_DefaultCamera = new FreeCamera(m_GameContext);
	m_DefaultCamera->SetPosition(vec3(-10.0f, 3.0f, -5.0f));
	m_GameContext.camera = m_DefaultCamera;

	m_GameContext.inputManager = new InputManager();

	m_GameContext.renderer->PostInitialize();
}

void TechDemo::Destroy()
{
	m_SceneManager->Destroy(m_GameContext);

	delete m_GameContext.inputManager;
	delete m_DefaultCamera;
	delete m_SceneManager;
	delete m_Window;
	delete m_GameContext.renderer;
}

void TechDemo::UpdateAndRender()
{
	m_Running = true;
	float previousTime = (float)m_Window->GetTime();
	while (m_Running)
	{
		float currentTime = (float)m_Window->GetTime();
		float dt = (currentTime - previousTime);
		previousTime = currentTime;
		if (dt < 0.0f) dt = 0.0f;
	
		m_GameContext.deltaTime = dt;
		m_GameContext.elapsedTime = currentTime;
	
		m_GameContext.window->PollEvents();	 
	
		m_GameContext.inputManager->Update();
		m_GameContext.camera->Update(m_GameContext);
		m_GameContext.renderer->Clear((int)Renderer::ClearFlag::COLOR | (int)Renderer::ClearFlag::DEPTH | (int)Renderer::ClearFlag::STENCIL, m_GameContext);
	
		m_GameContext.window->Update(m_GameContext);
	
		m_SceneManager->UpdateAndRender(m_GameContext);

		if (m_GameContext.inputManager->GetKeyDown(InputManager::KeyCode::KEY_T))
		{
			++m_RendererIndex;
			if (m_RendererIndex > 2) m_RendererIndex = 0;
			Destroy();
			Initialize();
			continue;
		}
	
		m_GameContext.inputManager->PostUpdate();

	
		m_GameContext.renderer->SwapBuffers(m_GameContext);
	}
}

void TechDemo::Stop()
{
	m_Running = false;
}
