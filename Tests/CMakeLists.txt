add_executable(RendererTests
    Main.cpp
    Math/VectorTBNTests.cpp
    Math/HammersleyTests.cpp
    Math/HemispherePDFTests.cpp
    IBL/IrradianceConvolutionTests.cpp
    IBL/ReferenceIntegratorTests.cpp
    IBL/SolarHDRRegressionTests.cpp
    IBL/IrradianceContinuityTests.cpp
    IBL/SpecularPrefilterTests.cpp
    PBR/BRDFLUTTests.cpp
    PBR/PBRBrdfTests.cpp
    PBR/EnergyConservationTests.cpp
    HDR/HDRDecodingTests.cpp
    HDR/MipChainTests.cpp
    Cache/SerializationTests.cpp
    Cache/CacheInvalidationTests.cpp
    GPU/CubemapSeamTests.cpp
    GPU/TextureUploadTests.cpp
    IBL/DeterminismTests.cpp
    Shader/PBRShaderFresnelTests.cpp
    Shader/PBRShaderCookTorranceTests.cpp
    Shader/PBRShaderGGXSmithTests.cpp
    Shader/PBRShaderDirectLightsTests.cpp
    Shader/PBRShaderIBLTests.cpp
    Shader/PBRShaderShadowTests.cpp
    Shader/PBRShaderPlanarReflectionTests.cpp
    Shader/PBRShaderDeterminismTests.cpp
)

target_include_directories(RendererTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine/include
    ${CMAKE_SOURCE_DIR}/Plugins/RHI/OpenGL/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/glad/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/stb
    ${CMAKE_SOURCE_DIR}/ThirdParty/doctest
    ${CMAKE_SOURCE_DIR}/ThirdParty
)

target_link_libraries(RendererTests PRIVATE
    LeonEngineCore
    LeonEngine_OpenGL
    doctest
    glad
    glfw
    glm
    stb_image
)

enable_testing()
add_test(NAME RendererMathematicalTests COMMAND RendererTests)
