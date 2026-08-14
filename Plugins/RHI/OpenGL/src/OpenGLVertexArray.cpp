#include "OpenGLVertexArray.hpp"
#include "core/Log.hpp"

#include <glad/glad.h>

namespace Leon {

    static GLenum ShaderDataTypeToOpenGLBaseType(EShaderDataType InType) {
        switch (InType) {
        case EShaderDataType::Float:
            return GL_FLOAT;
        case EShaderDataType::Float2:
            return GL_FLOAT;
        case EShaderDataType::Float3:
            return GL_FLOAT;
        case EShaderDataType::Float4:
            return GL_FLOAT;
        case EShaderDataType::Mat3:
            return GL_FLOAT;
        case EShaderDataType::Mat4:
            return GL_FLOAT;
        case EShaderDataType::Int:
            return GL_INT;
        case EShaderDataType::Int2:
            return GL_INT;
        case EShaderDataType::Int3:
            return GL_INT;
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
        glGenVertexArrays(1, &m_RendererID);
    }

    FOpenGLVertexArray::~FOpenGLVertexArray() {
        glDeleteVertexArrays(1, &m_RendererID);
    }

    void FOpenGLVertexArray::Bind() const {
        glBindVertexArray(m_RendererID);
    }

    void FOpenGLVertexArray::Unbind() const {
        glBindVertexArray(0);
    }

    void FOpenGLVertexArray::AddVertexBuffer(const TRef<FVertexBuffer>& InVertexBuffer) {
        glBindVertexArray(m_RendererID);
        InVertexBuffer->Bind();

        const auto& layout = InVertexBuffer->GetLayout();
        for (const auto& element : layout) {
            switch (element.Type) {
            case EShaderDataType::Float:
            case EShaderDataType::Float2:
            case EShaderDataType::Float3:
            case EShaderDataType::Float4: {
                glEnableVertexAttribArray(m_VertexBufferIndex);
                glVertexAttribPointer(m_VertexBufferIndex, element.GetComponentCount(),
                                      ShaderDataTypeToOpenGLBaseType(element.Type),
                                      element.bNormalized ? GL_TRUE : GL_FALSE, layout.GetStride(),
                                      reinterpret_cast<const void*>(static_cast<uintptr_t>(element.Offset)));
                m_VertexBufferIndex++;
                break;
            }
            case EShaderDataType::Int:
            case EShaderDataType::Int2:
            case EShaderDataType::Int3:
            case EShaderDataType::Int4:
            case EShaderDataType::Bool: {
                glEnableVertexAttribArray(m_VertexBufferIndex);
                glVertexAttribIPointer(m_VertexBufferIndex, element.GetComponentCount(),
                                       ShaderDataTypeToOpenGLBaseType(element.Type), layout.GetStride(),
                                       reinterpret_cast<const void*>(static_cast<uintptr_t>(element.Offset)));
                m_VertexBufferIndex++;
                break;
            }
            case EShaderDataType::Mat3:
            case EShaderDataType::Mat4: {
                uint8_t count = element.GetComponentCount();
                for (uint8_t i = 0; i < count; i++) {
                    glEnableVertexAttribArray(m_VertexBufferIndex);
                    glVertexAttribPointer(m_VertexBufferIndex, count, ShaderDataTypeToOpenGLBaseType(element.Type),
                                          element.bNormalized ? GL_TRUE : GL_FALSE, layout.GetStride(),
                                          reinterpret_cast<const void*>(static_cast<uintptr_t>(
                                              element.Offset + sizeof(float) * count * i)));
                    glVertexAttribDivisor(m_VertexBufferIndex, 1);
                    m_VertexBufferIndex++;
                }
                break;
            }
            case EShaderDataType::None:
                break;
            }
        }

        m_VertexBuffers.push_back(InVertexBuffer);
    }

    void FOpenGLVertexArray::SetIndexBuffer(const TRef<FIndexBuffer>& InIndexBuffer) {
        glBindVertexArray(m_RendererID);
        InIndexBuffer->Bind();

        m_IndexBuffer = InIndexBuffer;
    }

} // namespace Leon
