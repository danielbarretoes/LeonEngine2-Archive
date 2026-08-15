#pragma once

// Core Subsystems
#include "core/Application.hpp"
#include "core/Base.hpp"
#include "core/ConfigFile.hpp"
#include "core/Input.hpp"
#include "core/Layer.hpp"
#include "core/LayerStack.hpp"
#include "core/Log.hpp"
#include "core/PlatformMemory.hpp"
#include "core/Timestep.hpp"
#include "core/Window.hpp"

// Events
#include "core/events/ApplicationEvent.hpp"
#include "core/events/Event.hpp"
#include "core/events/KeyEvent.hpp"
#include "core/events/MouseEvent.hpp"

// Renderer Abstraction (RHI) & Camera
#include "renderer/Buffer.hpp"
#include "renderer/DebugOverlay.hpp"
#include "renderer/DebugRenderer.hpp"
#include "renderer/Framebuffer.hpp"
#include "renderer/GraphicsContext.hpp"
#include "renderer/Light.hpp"
#include "renderer/MeshPrimitives.hpp"
#include "renderer/PerspectiveCamera.hpp"
#include "renderer/PerspectiveCameraController.hpp"
#include "renderer/RenderAPI.hpp"
#include "renderer/RenderCommand.hpp"
#include "renderer/RenderStats.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/Shader.hpp"
#include "renderer/SceneRenderer.hpp"
#include "renderer/TextRenderer.hpp"
#include "renderer/Texture.hpp"
#include "renderer/VertexArray.hpp"

// Scene & Entity Component System (ECS)
#include "scene/Components.hpp"
#include "scene/Entity.hpp"
#include "scene/Scene.hpp"
#include "scene/LevelSerializer.hpp"

// Entry Point
#include "core/EntryPoint.hpp"
