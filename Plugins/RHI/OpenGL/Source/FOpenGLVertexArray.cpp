#include "FOpenGLVertexArray.hpp"
#include "FOpenGLBuffer.hpp"
#include "Core/FLog.hpp"
#include "RHI/FRenderer.hpp"

#include <glad/glad.h>

namespace Leon {

    static GLenum ShaderDataTypeToOpenGLBaseType(EShaderDataType InType) {
        switch (InType) {
        case EShaderDataType::Float:
        case EShaderDataType::Float2:
        case EShaderDataType::Float3:
        case EShaderDataType::Float4:
        case EShaderDataType::Mat3:
        case EShaderDataType::Mat4:
            return GL_FLOAT;
        case EShaderDataType::Int:
        case EShaderDataType::Int2:
        case EShaderDataType::Int3:
        case EShaderDataType::Int4:
            return GL_INT;
        case EShaderDataType::Bool:
            return GL_BOOL;
        case EShaderDataType::None:
            return 0;
        }
        return 0;
    }

    FOpenGLVertexArray::FOpenGLVertexArray() {
        glCreateVertexArrays(1, &RendererID);
    }

    FOpenGLVertexArray::~FOpenGLVertexArray() {
        glDeleteVertexArrays(1, &RendererID);
    }

    void FOpenGLVertexArray::Bind() const {
        glBindVertexArray(RendererID);
        FRenderer::GetStatsMutable().VAOBinds++;
    }

    void FOpenGLVertexArray::Unbind() const {
        glBindVertexArray(0);
    }

    void FOpenGLVertexArray::AddVertexBuffer(const TRef<FVertexBuffer>& InVertexBuffer) {
        LE_CORE_ASSERT(!InVertexBuffer->GetLayout().GetElements().empty(), "Vertex Buffer has no layout!");

        auto glVBO = std::dynamic_pointer_cast<FOpenGLVertexBuffer>(InVertexBuffer);
        LE_CORE_ASSERT(glVBO, "Invalid Vertex Buffer implementation!");

        const auto& layout = InVertexBuffer->GetLayout();
        uint32_t bindingIndex = static_cast<uint32_t>(VertexBuffers.size());

        glVertexArrayVertexBuffer(RendererID, bindingIndex, glVBO->GetRendererID(), 0, layout.GetStride());

        for (const auto& element : layout) {
            switch (element.Type) {
            case EShaderDataType::Float:
            case EShaderDataType::Float2:
            case EShaderDataType::Float3:
            case EShaderDataType::Float4: {
                glEnableVertexArrayAttrib(RendererID, VertexBufferIndex);
                glVertexArrayAttribFormat(RendererID, VertexBufferIndex, element.GetComponentCount(),
                                          ShaderDataTypeToOpenGLBaseType(element.Type),
                                          element.bNormalized ? GL_TRUE : GL_FALSE,
                                          static_cast<GLuint>(element.Offset));
                glVertexArrayAttribBinding(RendererID, VertexBufferIndex, bindingIndex);
                VertexBufferIndex++;
                break;
            }
            case EShaderDataType::Int:
            case EShaderDataType::Int2:
            case EShaderDataType::Int3:
            case EShaderDataType::Int4:
            case EShaderDataType::Bool: {
                glEnableVertexArrayAttrib(RendererID, VertexBufferIndex);
                glVertexArrayAttribIFormat(RendererID, VertexBufferIndex, element.GetComponentCount(),
                                           ShaderDataTypeToOpenGLBaseType(element.Type),
                                           static_cast<GLuint>(element.Offset));
                glVertexArrayAttribBinding(RendererID, VertexBufferIndex, bindingIndex);
                VertexBufferIndex++;
                break;
            }
            case EShaderDataType::Mat3:
            case EShaderDataType::Mat4: {
                uint8_t count = element.GetComponentCount();
                for (uint8_t i = 0; i < count; i++) {
                    glEnableVertexArrayAttrib(RendererID, VertexBufferIndex);
                    glVertexArrayAttribFormat(RendererID, VertexBufferIndex, count,
                                              ShaderDataTypeToOpenGLBaseType(element.Type),
                                              element.bNormalized ? GL_TRUE : GL_FALSE,
                                              static_cast<GLuint>(element.Offset + sizeof(float) * count * i));
                    glVertexArrayAttribBinding(RendererID, VertexBufferIndex, bindingIndex);
                    VertexBufferIndex++;
                }
                break;
            }
            case EShaderDataType::None:
                break;
            }
        }

        VertexBuffers.push_back(InVertexBuffer);
    }

    void FOpenGLVertexArray::SetIndexBuffer(const TRef<FIndexBuffer>& InIndexBuffer) {
        auto glIBO = std::dynamic_pointer_cast<FOpenGLIndexBuffer>(InIndexBuffer);
        LE_CORE_ASSERT(glIBO, "Invalid Index Buffer implementation!");

        glVertexArrayElementBuffer(RendererID, glIBO->GetRendererID());
        IndexBuffer = InIndexBuffer;
    }

} // namespace Leon
