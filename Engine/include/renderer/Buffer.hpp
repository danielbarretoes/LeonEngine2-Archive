#pragma once

#include "core/Base.hpp"
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace Leon {

    enum class EShaderDataType { None = 0, Float, Float2, Float3, Float4, Mat3, Mat4, Int, Int2, Int3, Int4, Bool };

    using ShaderDataType = EShaderDataType;

    static unsigned int ShaderDataTypeSize(EShaderDataType InType) {
        switch (InType) {
        case EShaderDataType::Float:
            return 4;
        case EShaderDataType::Float2:
            return 4 * 2;
        case EShaderDataType::Float3:
            return 4 * 3;
        case EShaderDataType::Float4:
            return 4 * 4;
        case EShaderDataType::Mat3:
            return 4 * 3 * 3;
        case EShaderDataType::Mat4:
            return 4 * 4 * 4;
        case EShaderDataType::Int:
            return 4;
        case EShaderDataType::Int2:
            return 4 * 2;
        case EShaderDataType::Int3:
            return 4 * 3;
        case EShaderDataType::Int4:
            return 4 * 4;
        case EShaderDataType::Bool:
            return 1;
        case EShaderDataType::None:
            return 0;
        }
        return 0;
    }

    struct FBufferElement {
        std::string Name;
        EShaderDataType Type;
        unsigned int Size;
        size_t Offset;
        bool bNormalized;

        FBufferElement() = default;

        FBufferElement(EShaderDataType InType, const std::string& InName, bool bInNormalized = false)
            : Name(InName), Type(InType), Size(ShaderDataTypeSize(InType)), Offset(0), bNormalized(bInNormalized) {}

        unsigned int GetComponentCount() const {
            switch (Type) {
            case EShaderDataType::Float:
                return 1;
            case EShaderDataType::Float2:
                return 2;
            case EShaderDataType::Float3:
                return 3;
            case EShaderDataType::Float4:
                return 4;
            case EShaderDataType::Mat3:
                return 3;
            case EShaderDataType::Mat4:
                return 4;
            case EShaderDataType::Int:
                return 1;
            case EShaderDataType::Int2:
                return 2;
            case EShaderDataType::Int3:
                return 3;
            case EShaderDataType::Int4:
                return 4;
            case EShaderDataType::Bool:
                return 1;
            case EShaderDataType::None:
                return 0;
            }
            return 0;
        }
    };

    using BufferElement = FBufferElement;

    class FBufferLayout {
    public:
        FBufferLayout() = default;
        FBufferLayout(std::initializer_list<FBufferElement> InElements) : m_Elements(InElements) {
            CalculateOffsetsAndStride();
        }

        unsigned int GetStride() const { return m_Stride; }
        const std::vector<FBufferElement>& GetElements() const { return m_Elements; }

        std::vector<FBufferElement>::iterator begin() { return m_Elements.begin(); }
        std::vector<FBufferElement>::iterator end() { return m_Elements.end(); }
        std::vector<FBufferElement>::const_iterator begin() const { return m_Elements.begin(); }
        std::vector<FBufferElement>::const_iterator end() const { return m_Elements.end(); }

    private:
        void CalculateOffsetsAndStride() {
            size_t offset = 0;
            m_Stride = 0;
            for (auto& element : m_Elements) {
                element.Offset = offset;
                offset += element.Size;
                m_Stride += element.Size;
            }
        }

    private:
        std::vector<FBufferElement> m_Elements;
        unsigned int m_Stride = 0;
    };

    using BufferLayout = FBufferLayout;

    class FVertexBuffer {
    public:
        virtual ~FVertexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void SetData(const void* InData, unsigned int InSize) = 0;
        virtual const FBufferLayout& GetLayout() const = 0;
        virtual void SetLayout(const FBufferLayout& InLayout) = 0;

        static TRef<FVertexBuffer> Create(unsigned int InSize);
        static TRef<FVertexBuffer> Create(const float* InVertices, unsigned int InSize);
    };

    using VertexBuffer = FVertexBuffer;

    class FIndexBuffer {
    public:
        virtual ~FIndexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual unsigned int GetCount() const = 0;

        static TRef<FIndexBuffer> Create(const uint32_t* InIndices, unsigned int InCount);
    };

    using IndexBuffer = FIndexBuffer;

} // namespace Leon
