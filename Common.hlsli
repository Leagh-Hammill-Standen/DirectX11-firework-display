//--------------------------------------------------------------------------------------
// Common include file for all shaders
//--------------------------------------------------------------------------------------
// Using include files to define the type of data passed between the shaders


//--------------------------------------------------------------------------------------
// Shader input for firework particles
//--------------------------------------------------------------------------------------

// Structure containing data to render a firework particle. Must exactly match Firework struct declared in Scene.cpp
struct Firework
{
    float3 position : position; // World position of particle
    float  scale    : scale;    // Current size of particle (1 is default size)
    float4 colour   : colour;   // RGBA colour - A is transparency
    float  rotation : rotation; // Z rotation of particle
};

 
//--------------------------------------------------------------------------------------
// Shader input / output for models
//--------------------------------------------------------------------------------------

// Vertex data to be sent into the vertex shader for most models
struct BasicVertex
{
    float3 position : position;
    float3 normal   : normal;
    float2 uv       : uv;
};


// Data sent from vertex shader to the lighting pixel shader for lit models
struct LightingPixelShaderInput
{
    float4 projectedPosition : SV_Position; // Required output from vertex shader 2D position of pixel + depth (in z & w)
    
    float3 worldPosition : worldPosition; // World position and normal for lighting
    float3 worldNormal   : worldNormal;
   
    float2 uv : uv; // UV for texturing
};


// Data sent from vertex shader to the pixel shader for textured objects with per-primitive colour-tint, without lighting (e.g. particles)
struct ColourTexturePixelShaderInput
{
    float4 projectedPosition : SV_Position;
    float4 colour            : colour;
    float2 uv                : uv;
};

// Data sent from vertex shader to the pixel shader for textured objects without lighting (e.g. skybox)
struct TexturePixelShaderInput
{
    float4 projectedPosition : SV_Position;
    float2 uv : uv;
};



//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------

// These structures are "constant buffers" - a way of passing variables over from C++ to the GPU
// They are called constants but that only means they are constant for the duration of a single GPU draw call.
// These "constants" correspond to variables in C++ that we will change per-model, or per-frame etc.
// These variables must match exactly the gPerFrameConstants structure in the C++ Common.h file
cbuffer PerFrameConstants : register(b0) // The b0 gives this constant buffer the number 0 - used in the C++ code
{
	float4x4 gCameraMatrix;         // World matrix for the camera (inverse of the ViewMatrix below) - used in particle rendering geometry shader
	float4x4 gViewMatrix;
    float4x4 gProjectionMatrix;
    float4x4 gViewProjectionMatrix; // The above two matrices multiplied together to combine their effects

    float3   gLight1Position; // 3 floats: x, y z
    float    padding1;        // Pad above variable to float4 (HLSL requirement - copied in the the C++ version of this structure)
    float3   gLight1Colour;
    float    padding2;

    float3   gLight2Position;
    float    padding3;
    float3   gLight2Colour;
    float    padding4;

    float3   gAmbientColour;
    float    gSpecularPower;

    float3   gCameraPosition;
	float    gFrameTime;      // This app does updates on the GPU so we pass over the frame update time
}
// Note constant buffers are not structs: we don't use the name of the constant buffer, these are really just a collection of global variables (hence the 'g')



static const int MAX_BONES = 64; // For skinning support - not used in this project

// If we have multiple models then we need to update the world matrix from C++ to GPU multiple times per frame because we
// only have one world matrix here. Because this data is updated more frequently it is kept in a different buffer for better performance.
// We also keep other data that changes per-model here
// These variables must match exactly the gPerModelConstants structure in the C++ Common.h file

cbuffer PerModelConstants : register(b1) // The b1 gives this constant buffer the number 1 - used in the C++ code
{
    float4x4 gWorldMatrix; // Per-object position/rotation/scaling

    float3   gObjectColour;  // Useed for tinting light models

	float4x4 gBoneMatrices[MAX_BONES]; // Skinning is not used in this project, but the support has been left in - can be ignored
}


