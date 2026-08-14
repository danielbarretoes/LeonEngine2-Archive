#include "OpenGLVertexArray.hpp"
#include "OpenGLBuffer.hpp"
#include "core/Log.hpp"

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
        glCreateVertexArrays(1, &m_RendererID);
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
        LE_CORE_ASSERT(!InVertexBuffer->GetLayout().GetElements().empty(), "Vertex Buffer has no layout!");

        auto glVBO = std::dynamic_pointer_cast<FOpenGLVertexBuffer>(InVertexBuffer);
        LE_CORE_ASSERT(glVBO, "Invalid Vertex Buffer implementation!");

        const auto& layout = InVertexBuffer->GetLayout();
        uint32_t bindingIndex = static_cast<uint32_t>(m_VertexBuffers.size());

        glVertexArrayVertexBuffer(m_RendererID, bindingIndex, glVBO->GetRendererID(), 0, layout.GetStride());

        for (const auto& element : layout) {
            switch (element.Type) {
            case EShaderDataType::Float:
            case EShaderDataType::Float2:
            case EShaderDataType::Float3:
            case EShaderDataType::Float4: {
                glEnableVertexArrayAttrib(m_RendererID, m_VertexBufferIndex);
                glVertexArrayAttribFormat(m_RendererID, m_VertexBufferIndex,
                                          element.GetComponentCount(),
                                          ShaderDataTypeToOpenGLBaseType(element.Type),
                                          element.bNormalized ? GL_TRUE : GL_FALSE,
                                          static_cast<GLuint>(element.Offset));
                glVertexArrayAttribBinding(m_RendererID, m_VertexBufferIndex, bindingIndex);
                m_VertexBufferIndex++;
                break;
            }
            case EShaderDataType::Int:
            case EShaderDataType::Int2:
            case EShaderDataType::Int3:
            case EShaderDataType::Int4:
            case EShaderDataType::Bool: {
                glEnableVertexArrayAttrib(m_RendererID, m_VertexBufferIndex);
                glVertexArrayAttribIFormat(m_RendererID, m_VertexBufferIndex,
                                           element.GetComponentCount(),
                                           ShaderDataTypeToOpenGLBaseType(element.Type),
                                           static_cast<GLuint>(element.Offset));
                glVertexArrayAttribBinding(m_RendererID, m_VertexBufferIndex, bindingIndex);
                m_VertexBufferIndex++;
                break;
            }
            case EShaderDataType::Mat3:
            case EShaderDataType::Mat4: {
                uint8_t count = element.GetComponentCount();
                for (uint8_t i = 0; i < count; i++) {
                    glEnableVertexArrayAttrib(m_RendererID, m_VertexBufferIndex);
                    glVertexArrayAttribFormat(m_RendererID, m_VertexBufferIndex,
                                              count,
                                              ShaderDataTypeToOpenGLBaseType(element.Type),
                                              element.bNormalized ? GL_TRUE : GL_FALSE,
                                              static_cast<GLuint>(element.Offset + sizeof(float) * count * i));
                    glVertexArrayAttribBinding(m_RendererID, m_VertexBufferIndex, bindingIndex);
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
        auto glIBO = std::dynamic_pointer_cast<FOpenGLIndexBuffer>(InIndexBuffer);
        LE_CORE_ASSERT(glIBO, "Invalid Index Buffer implementation!");

        glVertexArrayElementBuffer(m_RendererID, glIBO->GetRendererID());
        m_IndexBuffer = InIndexBuffer;
    }

} // namespace Leon
