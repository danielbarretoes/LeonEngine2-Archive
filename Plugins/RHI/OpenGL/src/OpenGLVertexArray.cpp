#include "opengl/OpenGLVertexArray.hpp"
#include "engine/core/Log.hpp"

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
        unsigned int index = 0;
        for (const auto& element : layout) {
            glEnableVertexAttribArray(index);
            glVertexAttribPointer(index, element.GetComponentCount(), ShaderDataTypeToOpenGLBaseType(element.Type),
                                  element.bNormalized ? GL_TRUE : GL_FALSE, layout.GetStride(),
                                  (const void*)element.Offset);
            index++;
        }

        m_VertexBuffers.push_back(InVertexBuffer);
    }

    void FOpenGLVertexArray::SetIndexBuffer(const TRef<FIndexBuffer>& InIndexBuffer) {
        glBindVertexArray(m_RendererID);
        InIndexBuffer->Bind();

        m_IndexBuffer = InIndexBuffer;
    }

} // namespace Leon
