#!/usr/bin/env python3
"""
Verify LeonEngine2 has no forbidden legacy naming patterns.

Enforces Docs/NAMING.md sections 1-2 (type prefixes and ECS vs UObject component rules):
  - Unprefixed using-aliases (using Application = FApplication, etc.)
  - FSceneRenderer / GetSceneRenderer / SceneRenderer.hpp leftovers
  - Member prefixes m_ and s_ on typical C++ identifiers
  - OpenGL plugin files still named OpenGL*.hpp/.cpp (must be FOpenGL*)
  - Sandbox gameplay files still named SandboxGameMode* (must be A/U prefixed)
  - Engine/ must not hardcode a product project (Projects/Sandbox, Projects/LeonTournament, or bare product names)
  - EnTT POD components must not use U*Component (`struct UFooComponent` → F*Component)
  - F* types must not inherit U* (`class FFoo : public UBar`)
"""

from __future__ import annotations

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN_ROOTS = ("Engine", "Plugins", "Projects", "Tests", "Tools", "Editor")
SKIP_DIR_NAMES = {
    ".git",
    "build",
    "out",
    "ThirdParty",
    "Content",
    "Intermediate",
    "Cache",
    "Saved",
    "Resources",
}

# Legacy unprefixed aliases that must not remain after the UE naming sweep
RE_USING_ALIAS = re.compile(
    r"^\s*using\s+(?:Application|ApplicationProps|Window|WindowProps|Layer|LayerStack|Log|LogLevel|"
    r"Timestep|Input|ConfigFile|Event|EventType|EventCategory|EventDispatcher|"
    r"WindowResizeEvent|WindowCloseEvent|KeyEvent|KeyPressedEvent|KeyReleasedEvent|"
    r"MouseMovedEvent|MouseScrolledEvent|MouseButtonEvent|MouseButtonPressedEvent|"
    r"MouseButtonReleasedEvent|ShaderDataType|BufferElement|BufferLayout|"
    r"VertexBuffer|IndexBuffer|UniformBuffer|Framebuffer|FramebufferTextureFormat|"
    r"FramebufferTextureSpecification|FramebufferAttachmentSpecification|"
    r"FramebufferSpecification|GraphicsContext|RenderAPI|AssetManager|"
    r"OpenGLContext|OpenGLRenderAPI|OpenGLFramebuffer|OpenGLShader|OpenGLTexture2D|"
    r"OpenGLTextureCube|OpenGLVertexArray|OpenGLVertexBuffer|OpenGLIndexBuffer|"
    r"OpenGLUniformBuffer|OpenGLRenderDriver)\s*=",
    re.MULTILINE,
)
# Docs/NAMING.md §1: using-aliases must be U/A/F/I/E/T prefixed (Ref/Scope are documented exceptions)
RE_USING_ANY = re.compile(r"^\s*using\s+([A-Za-z_]\w*)\s*=", re.MULTILINE)
ALLOWED_UNPREFIXED_ALIASES = {"Ref", "Scope"}
RE_SCENE = re.compile(r"\b(FSceneRenderer|GetSceneRenderer|class SceneRenderer)\b")
RE_MEMBER_M = re.compile(r"\bm_[A-Za-z]\w*")
RE_MEMBER_S = re.compile(r"\bs_[A-Za-z]\w*")
RE_ENGINE_PROJECT_PATH = re.compile(r"Projects[/\\](Sandbox|LeonTournament)")
RE_ENGINE_PRODUCT = re.compile(r"\b(Sandbox|LeonTournament)\b")
# Docs/NAMING.md sections 1-2: EnTT PODs are F*Component; U*Component only for UActorComponent subclasses
RE_U_COMPONENT_STRUCT = re.compile(r"struct\s+U[A-Za-z0-9_]*Component\b")
# Docs/NAMING.md section 1: F* is value/struct prefix — must not inherit UObject-style U*
RE_F_INHERITS_U = re.compile(r"class\s+F\w+\s*:\s*public\s+U")
RE_EXT = {".hpp", ".h", ".cpp", ".c", ".inl"}

FORBIDDEN_FILENAMES = {
    "OpenGLBuffer.hpp",
    "OpenGLBuffer.cpp",
    "OpenGLContext.hpp",
    "OpenGLContext.cpp",
    "OpenGLFramebuffer.hpp",
    "OpenGLFramebuffer.cpp",
    "OpenGLRenderAPI.hpp",
    "OpenGLRenderAPI.cpp",
    "OpenGLRenderDriver.hpp",
    "OpenGLRenderDriver.cpp",
    "OpenGLShader.hpp",
    "OpenGLShader.cpp",
    "OpenGLTexture2D.hpp",
    "OpenGLTexture2D.cpp",
    "OpenGLTextureCube.hpp",
    "OpenGLTextureCube.cpp",
    "OpenGLUniformBuffer.hpp",
    "OpenGLUniformBuffer.cpp",
    "OpenGLVertexArray.hpp",
    "OpenGLVertexArray.cpp",
    "SandboxGameMode.hpp",
    "SandboxGameMode.cpp",
    "SandboxHUD.hpp",
    "SandboxHUD.cpp",
    "SandboxMainMenuWidget.hpp",
    "SandboxMainMenuWidget.cpp",
    "SceneRenderer.hpp",
    "SceneRenderer.cpp",
}


def should_skip_dir(name: str) -> bool:
    return name in SKIP_DIR_NAMES or name.startswith(".")


def iter_sources():
    for top in SCAN_ROOTS:
        base = os.path.join(ROOT, top)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames if not should_skip_dir(d)]
            for fn in filenames:
                yield os.path.join(dirpath, fn)


def main() -> int:
    violations: list[str] = []
    for path in iter_sources():
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        fn = os.path.basename(path)
        if fn in FORBIDDEN_FILENAMES:
            violations.append(f"{rel}: forbidden legacy filename (use UE-prefixed name)")
        ext = os.path.splitext(fn)[1].lower()
        if ext not in RE_EXT:
            continue
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError as exc:
            violations.append(f"{rel}: read error {exc}")
            continue
        for m in RE_USING_ALIAS.finditer(text):
            violations.append(f"{rel}: unprefixed using-alias `{m.group(0).strip()}`")
        for m in RE_USING_ANY.finditer(text):
            name = m.group(1)
            if name in ALLOWED_UNPREFIXED_ALIASES:
                continue
            if name and name[0] in "UAFITE" and (len(name) == 1 or name[1].isupper() or name[1].isdigit()):
                continue
            violations.append(f"{rel}: unprefixed using-alias `{m.group(0).strip()}`")
        for m in RE_SCENE.finditer(text):
            violations.append(f"{rel}: legacy Scene API `{m.group(1)}`")
        for m in RE_F_INHERITS_U.finditer(text):
            violations.append(
                f"{rel}: F* inherits U* `{m.group(0)}` (see Docs/NAMING.md sections 1-2)"
            )
        # Engine must stay product-agnostic (no Sandbox / Projects/Sandbox defaults)
        if rel.startswith("Engine/"):
            for m in RE_ENGINE_PROJECT_PATH.finditer(text):
                violations.append(f"{rel}: engine hardcodes product path `{m.group(0)}`")
            for line_no, line in enumerate(text.splitlines(), 1):
                if RE_ENGINE_PRODUCT.search(line):
                    violations.append(f"{rel}:{line_no}: engine references product name")
            for m in RE_U_COMPONENT_STRUCT.finditer(text):
                violations.append(
                    f"{rel}: EnTT POD uses U*Component `{m.group(0)}` "
                    f"(use F*Component; see Docs/NAMING.md sections 1-2)"
                )
        for m in RE_MEMBER_M.finditer(text):
            violations.append(f"{rel}: member prefix `{m.group(0)}`")
        for m in RE_MEMBER_S.finditer(text):
            if len(m.group(0)) > 2 and (m.group(0)[2].isupper() or m.group(0)[2] == "b"):
                violations.append(f"{rel}: static prefix `{m.group(0)}`")

    if violations:
        print(f"[verify_ue_naming] {len(violations)} violation(s):")
        for line in violations[:80]:
            print(f"  {line}")
        if len(violations) > 80:
            print(f"  ... and {len(violations) - 80} more")
        return 1

    print("[verify_ue_naming] OK — no legacy naming violations found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
