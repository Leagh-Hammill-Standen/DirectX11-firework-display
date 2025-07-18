//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------

#include "Scene.h"
#include "Mesh.h"
#include "Model.h"
#include "Camera.h"
#include "State.h"
#include "Shader.h"
#include "Input.h"
#include "Common.h"

#include "CVector2.h" 
#include "CVector3.h" 
#include "CMatrix4x4.h"
#include "MathHelpers.h"     // Helper functions for maths
#include "GraphicsHelpers.h" // Helper functions to unclutter the code here
#include "ColourRGBA.h" 

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <sstream>
#include <memory>
#include <array>



//--------------------------------------------------------------------------------------
// Scene Data
//--------------------------------------------------------------------------------------

// Lock FPS to monitor refresh rate, which will typically set it to 60fps. Press 'p' to toggle to full fps
bool lockFPS = true;

// Meshes, models and cameras, same meaning as TL-Engine. Meshes prepared in InitGeometry function, Models & camera in InitScene
Mesh* gStarsMesh;
Mesh* gGroundMesh;
Mesh* gLightMesh;

Model* gStars;
Model* gGround;

Camera* gCamera;


// Array of lights
const int NUM_LIGHTS = 2;
struct Light
{
    Model*   model;
    CVector3 colour;
    float    strength;
};
Light gLights[NUM_LIGHTS]; 


// Additional light information
CVector3 gAmbientColour = { 0.3f, 0.3f, 0.4f }; // Background level of light (slightly bluish to match the far background, which is dark blue)
float    gSpecularPower = 256; // Specular power controls shininess - same for all models in this app

ColourRGBA gBackgroundColor = { 0.3f, 0.3f, 0.4f, 1.0f };

// Variables controlling light1's orbiting of the cube
const float gLightOrbit = 20.0f;
const float gLightOrbitSpeed = 0.7f;

// Input speed constsnts
const float ROTATION_SPEED = 2;
const float MOVEMENT_SPEED = 50;



//*************************************************************************
// ASSIGNMENT

// Particle Data
const int MaxFireworks = 50000; // Hard cap on number of firework particle allowed at once - vertex buffer is this size and 
                                // spawning more particles will do nothing

const float Gravity = -30.0f; // Tweaked to make getting nice firework settings easier

int numFireworksAtOnce    = 2; // Just for an example ImGui control - how many fireworks to spawn with each button press

float fireworkColourRed = 1.0f; // Colour settings for ImGui controls
float fireworkColourGreen = 1.0f;
float fireworkColourBlue = 1.0f;

float fireworkColourAlpha = 1.0f;

float fireworkColour[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // 4th value is alpha transparency - 1.0 is not transparent

float fireworkPosition[3] = { 0.0f, 0.0f, 0.0f }; // Position of the firework - set by the fireworkUpdate struct when the firework is created

int numBurstParticles = 100; // Number of particles in the burst - set by the fireworkUpdate struct when the firework is created
float burstParticleLife = 3.0f; // Life of the burst particles - set by the fireworkUpdate struct when the firework is created
float fireworkScale = 1; // Scale of the firework - set by the fireworkUpdate struct when the firework is created
float fireworkRotation = 0; // Rotation of the firework - set by the fireworkUpdate struct when the firework is created
float fireworkInitialVelocity = 70; // Velocity of the firework - set by the fireworkUpdate struct when the firework is created



//--------------------------------------------------------------------------------------
// Firework types and data
//--------------------------------------------------------------------------------------

// TODO: You will be adding more firework types here as required from the assessment brief
enum class FireworkType { PeonyRocket, FancyPeonyRocket, StarSimple, StarSmallTrail, CometRocket, BrocadeRocket};


// IMPORTANT: We have two equal-size std::vectors of particle data, containing info to render the particle (goes to GPU) and data to update
// the particle (stays on CPU). The update code will use both structures, the GPU rendering only the render structure. This minimises the
// amount of data passed to the GPU each frame. E.g. when updating first particle the C++ code uses both FireworkUpdates[0] and Fireworks[0]

// Data only needed to update a firework. Enough data here already for basic fireworks. You *might* want to add other data members, but it isn't initially necessary
// Each firework is made of parts. E.g a Peony firework starts with a single PeonyRocket particle that shoots into
//                                 the air, when its life runs out it emits a large number of PeonyStar particles in random directions
//                                 The number and colour of PeonyStar particles is held in the payload variables
struct FireworkUpdate
{
	FireworkType type;     // Firework type from enum above
	CVector3     velocity; // World velocity of particle

	float        life;     // Current life of particle (seconds)
	float        timer;    // Internal timer for triggering events in flight, can be unused (see StarSmallTrail for example of use)

	FireworkType payloadTypeA;   // Information about firework payload - usage depends on firework type, can be unused
	FireworkType payloadTypeB;   //
	int          payloadIntA;    // 
	int          payloadIntB;    //
	ColourRGBA   payloadColourA; //
	ColourRGBA   payloadColourB; //
};


// Data to render a firework, updated using data above in C++, then sent over to GPU for rendering. Unlikely to need to change this
struct Firework
{
	CVector3   position; // World position of the firework
	float      scale;    // Scale of firework for rendering
	ColourRGBA colour;   // RGBA colour - A is transparency
	float      rotation; // Z rotation of particle
};

// Equal sized vectors, see comments above
std::vector<FireworkUpdate> FireworkUpdates;
std::vector<Firework>       Fireworks;


// Helper to add new fireworks but not allowing more than the given maximum
bool AddFirework(const Firework& firework, const FireworkUpdate& fireworkUpdate)
{
	if (Fireworks.size() >= MaxFireworks)  return false;
	Fireworks      .push_back(firework      );
	FireworkUpdates.push_back(fireworkUpdate);
	return true;
}


// An array of element descriptions to create the firework vertex buffer, If you don't change the Firework struct above this can
// be left alone. If you do change the Firework struct then this data must be updated to match. 
// Changing the FireworkUpdate struct has no relevance to this data.
// Contents explained using the third line as an example: that line indicates that 16 bytes into each vertex (into the Firework
// structure) there is a "colour" value which is four floats
D3D11_INPUT_ELEMENT_DESC ParticleElts[] =
{
	// Semantic  &  Index,   Type (eg. 2nd one is float3),     Slot,   Byte Offset,  Instancing information (not relevant here), --"--
	{ "position",   0,       DXGI_FORMAT_R32G32B32_FLOAT,      0,      0,            D3D11_INPUT_PER_VERTEX_DATA,    0 },
	{ "scale",      0,       DXGI_FORMAT_R32_FLOAT,            0,      12,           D3D11_INPUT_PER_VERTEX_DATA,    0 },
	{ "colour",     0,       DXGI_FORMAT_R32G32B32A32_FLOAT,   0,      16,           D3D11_INPUT_PER_VERTEX_DATA,    0 },
	{ "rotation",   0,       DXGI_FORMAT_R32_FLOAT,            0,      52,           D3D11_INPUT_PER_VERTEX_DATA,    0 },
};
const unsigned int NumParticleElts = sizeof(ParticleElts) / sizeof(D3D11_INPUT_ELEMENT_DESC);


// Variables for DirectX objects that will hold the vertex layout and buffers for fireworks
ID3D11InputLayout* FireworkLayout;
ID3D11Buffer*      FireworkBuffer;


//*************************************************************************


//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
// Variables sent over to the GPU each frame
// The structures are now in Common.h
// IMPORTANT: Any new data you add in C++ code (CPU-side) is not automatically available to the GPU
//            Anything the shaders need (per-frame or per-model) needs to be sent via a constant buffer

PerFrameConstants gPerFrameConstants;      // The constants that need to be sent to the GPU each frame (see common.h for structure)
ID3D11Buffer*     gPerFrameConstantBuffer; // The GPU buffer that will recieve the constants above

PerModelConstants gPerModelConstants;      // As above, but constant that change per-model (e.g. world matrix)
ID3D11Buffer*     gPerModelConstantBuffer; // --"--


//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------

// DirectX objects controlling textures used in this lab
ID3D11Resource*           gStarsDiffuseSpecularMap    = nullptr;
ID3D11ShaderResourceView* gStarsDiffuseSpecularMapSRV = nullptr;

ID3D11Resource*           gGroundDiffuseSpecularMap    = nullptr;
ID3D11ShaderResourceView* gGroundDiffuseSpecularMapSRV = nullptr;

ID3D11Resource*           gLightDiffuseMap    = nullptr;
ID3D11ShaderResourceView* gLightDiffuseMapSRV = nullptr;

ID3D11Resource*           gFireworkDiffuseMap    = nullptr;
ID3D11ShaderResourceView* gFireworkDiffuseMapSRV = nullptr;


//--------------------------------------------------------------------------------------
// Initialise scene geometry, constant buffers and states
//--------------------------------------------------------------------------------------

// Prepare the geometry required for the scene
// Returns true on success
bool InitGeometry()
{
	////--------------- Load meshes ---------------////
	
	// Load mesh geometry data, just like TL-Engine this doesn't create anything in the scene. Create a Model for that.
    try 
    {
        gStarsMesh  = new Mesh("Stars.x");
		gGroundMesh = new Mesh("Ground.x");
		gLightMesh  = new Mesh("Light.x");
    }
    catch (std::runtime_error e)  // Constructors cannot return error messages so use exceptions to catch mesh errors (fairly standard approach this)
    {
        gLastError = e.what(); // This picks up the error message put in the exception (see Mesh.cpp)
        return false;
    }


	////--------------- Load / prepare textures & GPU states ---------------////

	// Load textures and create DirectX objects for them
	// The LoadTexture function requires you to pass a ID3D11Resource* (e.g. &gCubeDiffuseMap), which manages the GPU memory for the
	// texture and also a ID3D11ShaderResourceView* (e.g. &gCubeDiffuseMapSRV), which allows us to use the texture in shaders
	// The function will fill in these pointers with usable data. The variables used here are globals found near the top of the file.
	if (!LoadTexture("Stars.jpg",               &gStarsDiffuseSpecularMap,  &gStarsDiffuseSpecularMapSRV ) ||
		!LoadTexture("WoodDiffuseSpecular.dds", &gGroundDiffuseSpecularMap, &gGroundDiffuseSpecularMapSRV) ||
		!LoadTexture("Flare.jpg",               &gLightDiffuseMap,          &gLightDiffuseMapSRV         ) ||
		!LoadTexture("Flare.jpg",               &gFireworkDiffuseMap,       &gFireworkDiffuseMapSRV      ))
	{
		gLastError = "Error loading textures";
		return false;
	}


	// Create all filtering modes, blending modes etc. used by the app (see State.cpp/.h)
	if (!CreateStates())
	{
		gLastError = "Error creating states";
		return false;
	}


	////--------------- Prepare shaders and constant buffers to communicate with them ---------------////

	// Load the shaders required for the geometry we will use (see Shader.cpp / .h)
    if (!LoadShaders())
    {
        gLastError = "Error loading shaders";
        return false;
    }

    // Create GPU-side constant buffers to receive the gPerFrameConstants and gPerModelConstants structures from Common.h
    // These allow us to pass data from CPU to shaders such as lighting information or matrices
    // See the comments above where these variable are declared and also the UpdateScene function
    gPerFrameConstantBuffer = CreateConstantBuffer(sizeof(gPerFrameConstants));
    gPerModelConstantBuffer = CreateConstantBuffer(sizeof(gPerModelConstants));
	if (gPerFrameConstantBuffer == nullptr || gPerModelConstantBuffer == nullptr)
    {
        gLastError = "Error creating constant buffers";
        return false;
    }


	//*************************************************************************
	// Initialise firework vertex buffer for GPU (initially empty)

	// Create the vertex layout for the particle data structure declared near the top of the file. This step creates
	// an object (ParticleLayout) that is used to describe to the GPU the data used for each particle
	auto signature = CreateSignatureForVertexLayout(ParticleElts, NumParticleElts);
	gD3DDevice->CreateInputLayout(ParticleElts, NumParticleElts, signature->GetBufferPointer(), signature->GetBufferSize(), &FireworkLayout);

	// Create / initialise particle vertex buffer on the GPU. Initially empty
	// We are going to update this vertex buffer every frame, so it must be defined as "dynamic" and writable (D3D11_USAGE_DYNAMIC & D3D11_CPU_ACCESS_WRITE)
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = MaxFireworks * sizeof(Firework); // Buffer size
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateBuffer(&bufferDesc, nullptr, &FireworkBuffer)))
	{
		gLastError = "Error creating particle vertex buffer";
		return false;
	}

	// Reserve space in the CPU-side vectors, so vector won't need to reallocate when we push_back new fireworks.
	// However, reserve only sets the capacity of the vector, the sizes of these vectors at first is 0
	FireworkUpdates.reserve(MaxFireworks);
	Fireworks.reserve(MaxFireworks);


	//*************************************************************************

	return true;
}


// Prepare the scene
// Returns true on success
bool InitScene()
{
    ////--------------- Set up scene ---------------////

	gStars  = new Model(gStarsMesh);
    gGround = new Model(gGroundMesh);

	// Initial positions
	gStars->SetScale(8000.0f);


    // Light set-up - using an array this time
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        gLights[i].model = new Model(gLightMesh);
    }
    
	gLights[0].colour = { 0.8f, 0.8f, 1.0f };
	gLights[0].strength = 10;
	gLights[0].model->SetPosition({ 30, 10, 0 });
	gLights[0].model->SetScale(pow(gLights[0].strength, 0.7f)); // Convert light strength into a nice value for the scale of the light - equation is ad-hoc.

	gLights[1].colour = { 0.7f, 0.7f, 1.0f };
	gLights[1].strength = 200;
	gLights[1].model->SetPosition({ -170, 300, 200 });
	gLights[1].model->SetScale(pow(gLights[1].strength, 0.7f));


    ////--------------- Set up camera ---------------////

    gCamera = new Camera();
    gCamera->SetPosition({ -0, 50, -200 });
    gCamera->SetRotation({ ToRadians(-7.5), 0.0f, 0.0f});

	return true;
}


// Release the geometry and scene resources created above
void ReleaseResources()
{
    ReleaseStates();

	if (FireworkLayout)  FireworkLayout->Release();
	if (FireworkBuffer)  FireworkBuffer->Release();

	if (gFireworkDiffuseMapSRV)        gFireworkDiffuseMapSRV      ->Release();
	if (gFireworkDiffuseMap)           gFireworkDiffuseMap         ->Release();
	if (gLightDiffuseMapSRV)           gLightDiffuseMapSRV         ->Release();
	if (gLightDiffuseMap)              gLightDiffuseMap            ->Release();
	if (gGroundDiffuseSpecularMapSRV)  gGroundDiffuseSpecularMapSRV->Release();
    if (gGroundDiffuseSpecularMap)     gGroundDiffuseSpecularMap   ->Release();
	if (gStarsDiffuseSpecularMapSRV)   gStarsDiffuseSpecularMapSRV ->Release();
	if (gStarsDiffuseSpecularMap)      gStarsDiffuseSpecularMap    ->Release();

    if (gPerModelConstantBuffer)  gPerModelConstantBuffer->Release();
    if (gPerFrameConstantBuffer)  gPerFrameConstantBuffer->Release();

    ReleaseShaders();

    // See note in InitGeometry about why we're not using unique_ptr and having to manually delete
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        delete gLights[i].model;  gLights[i].model = nullptr;
    }
    delete gCamera;  gCamera = nullptr;
    delete gGround;  gGround = nullptr;
	delete gStars;   gStars  = nullptr;

    delete gLightMesh;   gLightMesh  = nullptr;
    delete gGroundMesh;  gGroundMesh = nullptr;
	delete gStarsMesh;   gStarsMesh  = nullptr;
}



//--------------------------------------------------------------------------------------
// Scene Rendering
//--------------------------------------------------------------------------------------

// Render everything in the scene from the given camera
void RenderSceneFromCamera(Camera* camera)
{
    // Set camera matrices in the constant buffer and send over to GPU
	gPerFrameConstants.cameraMatrix         = camera->WorldMatrix();
	gPerFrameConstants.viewMatrix           = camera->ViewMatrix();
    gPerFrameConstants.projectionMatrix     = camera->ProjectionMatrix();
    gPerFrameConstants.viewProjectionMatrix = camera->ViewProjectionMatrix();
    UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

    // Indicate that the constant buffer we just updated is for use in the vertex shader (VS), geometry shader (GS) and pixel shader (PS)
    gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer); // First parameter must match constant buffer number in the shader 
	gD3DContext->GSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);
	gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);
	
	gD3DContext->GSSetConstantBuffers(2, 1, &gPerFrameConstantBuffer);

	gD3DContext->PSSetShader(gPixelLightingPixelShader, nullptr, 0);



    ////--------------- Render ordinary models ---------------///

    // Select which shaders to use next
    gD3DContext->VSSetShader(gPixelLightingVertexShader, nullptr, 0); // Only need to change the vertex shader from skinning
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  ////// Switch off geometry shader when not using it (pass nullptr for first parameter)
	gD3DContext->PSSetShader(gPixelLightingPixelShader, nullptr, 0);

	// States - no blending, normal depth buffer and back-face culling (standard set-up for opaque models)
	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
	gD3DContext->RSSetState(gCullBackState);

	// Render lit models, only change textures for each onee
	gD3DContext->PSSetShaderResources(0, 1, &gGroundDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shader
	gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

	gGround->Render();



	////--------------- Render sky ---------------////

	// Select which shaders to use next
	gD3DContext->VSSetShader(gBasicTransformVertexShader,    nullptr, 0);
	gD3DContext->PSSetShader(gSingleColourTexturePixelShader,nullptr, 0);

	// Using a pixel shader that tints the texture - don't need a tint on the sky so set it to white
	gPerModelConstants.objectColour = { 1, 1, 1 }; 

    // Stars point inwards
    gD3DContext->RSSetState(gCullNoneState);

	// Render sky
	gD3DContext->PSSetShaderResources(0, 1, &gStarsDiffuseSpecularMapSRV);
	gStars->Render();



	////--------------- Render lights ---------------////

    // Select which shaders to use next (actually same as before, so we could skip this)
    gD3DContext->VSSetShader(gBasicTransformVertexShader,     nullptr, 0);
    gD3DContext->PSSetShader(gSingleColourTexturePixelShader, nullptr, 0);

    // Select the texture and sampler to use in the pixel shader
    gD3DContext->PSSetShaderResources(0, 1, &gLightDiffuseMapSRV); // First parameter must match texture slot number in the shaer

    // States - additive blending, read-only depth buffer and no culling (standard set-up for blending)
    gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
    gD3DContext->RSSetState(gCullNoneState);

    // Render all the lights in the array
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        gPerModelConstants.objectColour = gLights[i].colour; // Set any per-model constants apart from the world matrix just before calling render (light colour here)
        gLights[i].model->Render();
    }


	//*************************************************************************
	// ASSIGNMENT

	// Unlikely to need to change this DirectX code that passes over and renders the fireworks on the GPU
	// However, in case you try something advanced...

	////--------------- Pass firework data to GPU ---------------////

	// Allow CPU access to GPU-side firework vertex buffer
	D3D11_MAPPED_SUBRESOURCE mappedData;
	gD3DContext->Map(FireworkBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);

	// Copy current firework rendering data 
	// Keep this process as fast as possible since the GPU may stall (have to wait, doing nothing) during this period
	Firework* vertexBufferData = (Firework*)mappedData.pData;
	memcpy(vertexBufferData, Fireworks.data(), Fireworks.size() * sizeof(Firework));

	// Remove CPU access to firework vertex buffer again so it can be used for rendering
	gD3DContext->Unmap(FireworkBuffer, 0);


	////--------------- Render fireworks ---------------////

	// Set shaders for firework particle rendering - the vertex shader just passes the data to the 
	// geometry shader, which generates a camera-facing 2D quad from the particle world position 
	// The pixel shader is very simple and just draws a tinted texture for each particle
	gD3DContext->VSSetShader(gFireworkPassThruVertexShader, nullptr, 0);
	gD3DContext->GSSetShader(gFireworkRenderGeometryShader, nullptr, 0);
	gD3DContext->PSSetShader(gColourTexturePixelShader,     nullptr, 0);

	// Select the texture and sampler to use in the pixel shader
	gD3DContext->PSSetShaderResources(0, 1, &gFireworkDiffuseMapSRV);

	// States - additive blending, read-only depth buffer and no culling (standard set-up for blending)
	gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
	gD3DContext->RSSetState(gCullNoneState);

	// Set up firework vertex buffer / layout
	unsigned int particleVertexSize = sizeof(Firework);
	unsigned int offset = 0;
	gD3DContext->IASetVertexBuffers(0, 1, &FireworkBuffer, &particleVertexSize, &offset);
	gD3DContext->IASetInputLayout(FireworkLayout);

	// Indicate that this is a point list and render all current fireworks
	gD3DContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
	gD3DContext->Draw((UINT)Fireworks.size(), 0);

	//*************************************************************************
}



// Rendering the scene
void RenderScene(float frameTime)
{
	//*************************************************************************
	// ASSIGNMENT

	// ImGui is already started for each frame here - don't need to do this yourself

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	//*************************************************************************


    //// Common settings ////

    // Set up the light information in the constant buffer
    // Don't send to the GPU yet, the function RenderSceneFromCamera will do that
    gPerFrameConstants.light1Colour   = gLights[0].colour * gLights[0].strength;
    gPerFrameConstants.light1Position = gLights[0].model->Position();
    gPerFrameConstants.light2Colour   = gLights[1].colour * gLights[1].strength;
    gPerFrameConstants.light2Position = gLights[1].model->Position();

    gPerFrameConstants.ambientColour  = gAmbientColour;
    gPerFrameConstants.specularPower  = gSpecularPower;
    gPerFrameConstants.cameraPosition = gCamera->Position();
	
	gPerFrameConstants.frameTime      = frameTime;


    ////--------------- Main scene rendering ---------------////

    // Set the back buffer as the target for rendering and select the main depth buffer.
    // When finished the back buffer is sent to the "front buffer" - which is the monitor.
    gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);

    // Clear the back buffer to a fixed colour and the depth buffer to the far distance
    gD3DContext->ClearRenderTargetView(gBackBufferRenderTarget, &gBackgroundColor.r);
    gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Setup the viewport to the size of the main window
    D3D11_VIEWPORT vp;
    vp.Width  = static_cast<FLOAT>(gViewportWidth);
    vp.Height = static_cast<FLOAT>(gViewportHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    gD3DContext->RSSetViewports(1, &vp);


	//*************************************************************************
	// ASSIGNMENT MAIN

	////--------------- ImGui Rendering ---------------////

	// Draw any ImGui elements you add for the assignment here. There are simple example controls already
	// 
	// Look at the particle tool lab (CO3301) for intro to ImGui
	// The wiki https://github.com/ocornut/imgui/wiki and github documentation: https://github.com/ocornut/imgui?tab=readme-ov-file
	// Also in the imgui folder imgui_demo.cpp has code examples of every feature
	// There is a nice online interactive version of the demo that shows the relevant code when you use the feature:
	// https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html  -> start by clicking on Widgets/Basic
	
	ImGui::SetNextWindowSize(ImVec2(0,0)); // Setting 0 size makes next window ("Controls") size automatically to fit its content
	ImGui::Begin("Controls"); // Header for ImGui window

	ImGui::SliderInt("Number of Fireworks at Once", &numFireworksAtOnce, 1, 5);  // int slider range 1-5

	//colour settings
	ImGui::ColorEdit4("colour", fireworkColour);  // int slider range 1-5

	//position settings
	ImGui::InputFloat3("position", fireworkPosition); // float3 input box

	//burst settings
	ImGui::SliderInt("Burst Particles", &numBurstParticles, 0, 360);  // int slider range 1-5
	ImGui::SliderFloat("Burst Particle Life", &burstParticleLife, 0.0f, 5.0f);  // int slider range 1-5
	ImGui::SliderFloat("Firework Scale", &fireworkScale, 0.0f, 5.0f);  // int slider range 1-5
	ImGui::SliderFloat("Firework Rotation", &fireworkRotation, 0.0f, 360.0f);  // int slider range 1-5
	ImGui::SliderFloat("Firework Initial Velocity", &fireworkInitialVelocity, 70.0f, 100.0f);  // int slider range 1-5

	if (ImGui::Button("Fire Peony"))
	{
		for (int i = 0; i < numFireworksAtOnce; ++i)
		{
			// Launches in a random direction within 15 degrees of up (0,1,0)
			// This helper function (from MathHelpers.h) is useful for various aspects of fireworks - read its comment
			CVector3 fireworkDirection = RandomVectorInCone( CVector3{0, 1, 0}, 15 ); 

			Firework firework;
			firework.position = fireworkPosition;
			firework.scale    = fireworkScale;
			firework.colour   = { fireworkColour[0], fireworkColour[1], fireworkColour[2], fireworkColour[3] }; // 4th value is alpha transparency - 1.0 is not transparent
			firework.rotation = fireworkRotation;

			FireworkUpdate fireworkUpdate;
			fireworkUpdate.type = FireworkType::PeonyRocket;
			fireworkUpdate.velocity       = fireworkDirection * fireworkInitialVelocity; // Initial velocity of rocket (speed 70 to 100)
			fireworkUpdate.life           = burstParticleLife;                     // How long before rocket bursts
			fireworkUpdate.payloadTypeA   = FireworkType::StarSimple; // Payload of a peony rocket
			fireworkUpdate.payloadIntA    = numBurstParticles;                      // How many stars the emit at the burst
			fireworkUpdate.payloadColourA = { fireworkColour[0], fireworkColour[1], fireworkColour[2], fireworkColour[3] }; // Colour of stars when it bursts
		
			AddFirework(firework, fireworkUpdate);
		}
	}

	if (ImGui::Button("Fire Comet"))
	{
		for (int i = 0; i < numFireworksAtOnce; ++i)
		{
			// Launches in a random direction within 15 degrees of up (0,1,0)
			// This helper function (from MathHelpers.h) is useful for various aspects of fireworks - read its comment
			CVector3 fireworkDirection = RandomVectorInCone(CVector3{ 0, 1, 0 }, 15);

			Firework firework;
			firework.position = fireworkPosition;
			firework.scale = fireworkScale;
			firework.colour = { fireworkColour[0], fireworkColour[1], fireworkColour[2], fireworkColour[3] }; // 4th value is alpha transparency - 1.0 is not transparent
			firework.rotation = fireworkRotation;

			FireworkUpdate fireworkUpdate;
			fireworkUpdate.type = FireworkType::CometRocket;
			fireworkUpdate.velocity = fireworkDirection * fireworkInitialVelocity; // Initial velocity of rocket (speed 70 to 100)
			fireworkUpdate.life = burstParticleLife;                     // How long before rocket bursts
			fireworkUpdate.payloadTypeA = FireworkType::StarSimple; // trail of commet
			fireworkUpdate.payloadIntA = numBurstParticles;                      // How many stars the emit at the burst
			fireworkUpdate.payloadColourA = { fireworkColour[0], fireworkColour[1], fireworkColour[2], fireworkColour[3] }; // Colour of stars when it bursts

			AddFirework(firework, fireworkUpdate);
		}
	}

	if (ImGui::Button("Fire Brocade"))
	{
		for (int i = 0; i < numFireworksAtOnce; ++i)
		{
			CVector3 fireworkDirection = RandomVectorInCone(CVector3{ 0, 1, 0 }, 180);

			Firework firework;
			firework.position = fireworkPosition;
			firework.scale = fireworkScale;
			firework.colour = { fireworkColour[0], fireworkColour[1], fireworkColour[2], fireworkColour[3] }; // 4th value is alpha transparency - 1.0 is not transparent
			firework.rotation = fireworkRotation;

			FireworkUpdate fireworkUpdate;
			fireworkUpdate.type = FireworkType::BrocadeRocket;
			fireworkUpdate.velocity = fireworkDirection * fireworkInitialVelocity;
			fireworkUpdate.life = burstParticleLife;                     // How long before rocket bursts
			fireworkUpdate.payloadTypeA = FireworkType::StarSimple; // Payload: stars
			fireworkUpdate.payloadIntA = numBurstParticles;                      // How many stars the emit at the burst
			fireworkUpdate.payloadColourA = { fireworkColour[0], fireworkColour[1], fireworkColour[2], fireworkColour[3] }; // Colour of stars when it bursts

			AddFirework(firework, fireworkUpdate);
		}
	}
	
	if (ImGui::Button("Fire Peony with Small Trails"))
	{
		for (int i = 0; i < numFireworksAtOnce; ++i)
		{
			CVector3 fireworkDirection = RandomVectorInCone(CVector3{ 0, 1, 0 }, 15);

			Firework firework;
			firework.position = fireworkPosition;
			firework.scale = fireworkScale;
			firework.colour = { fireworkColour[0], fireworkColour[1], fireworkColour[2], fireworkColour[3] }; // 4th value is alpha transparency - 1.0 is not transparent
			firework.rotation = fireworkRotation;

			FireworkUpdate fireworkUpdate;
			fireworkUpdate.type = FireworkType::PeonyRocket;
			fireworkUpdate.velocity       = fireworkDirection * fireworkInitialVelocity; // Initial velocity of rocket (speed 70 to 100)
			fireworkUpdate.life = burstParticleLife;                     // How long before rocket bursts
			fireworkUpdate.payloadTypeA   = FireworkType::StarSmallTrail; // Payload of a peony rocket
			fireworkUpdate.payloadIntA = numBurstParticles;                      // How many stars the emit at the burst
			fireworkUpdate.payloadColourA = { fireworkColour[0], fireworkColour[1], fireworkColour[2], fireworkColour[3] }; // Colour of stars when it bursts

			AddFirework(firework, fireworkUpdate);
		}
	}

	ImGui::End();

	//*************************************************************************


	////--------------- Scene Rendering ---------------////

	// Render the scene from the main camera
	RenderSceneFromCamera(gCamera);



    ////--------------- Scene completion ---------------////

	//*************************************************************************
	// ASSIGNMENT

	// ImGui is tied off for the frame here - you do not need to do anything. 
	// Don't add any ImGui controls outside the start / stop code in this function

	ImGui::Render();
	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, nullptr);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	//*************************************************************************


    // When drawing to the off-screen back buffer is complete, we "present" the image to the front buffer (the screen)
	// Set first parameter to 1 to lock to vsync
    gSwapChain->Present(lockFPS ? 1 : 0, 0);
}


//--------------------------------------------------------------------------------------
// Scene Update
//--------------------------------------------------------------------------------------


//*************************************************************************
// ASSIGNMENT MAIN

// Code to update fireworks each frame

// Helper function for UpdateFireworks - you may need to use this function yourself if you write more advanced code,
// but to start with skip over this and look at the next function UpdateFireworks
// 
// Removes the firework if its life is <= 0 and updates the given iterators to point at the next firework
// DOES NOT DECREASE LIFE, you need to do that in the update code
// 
// Detail: As we walk through the vector of fireworks to update them, some will die. Removing things in a container when
// you are in the middle of iterating through it always needs to be done carefully. Process:
// - If life reaches 0, we remove the current firework by overwriting it with the last firework in the vector.
// - Then remove that last firework from the end of the vector
// - That means we don't need to step forward in the vector, because the current firework becomes a new one to update
// - However, if the firework is still alive, just step forward normally
void RemoveFireworkIfDeadAndMoveToNext(std::vector<Firework>::iterator& fireworkIt, std::vector<FireworkUpdate>::iterator& fireworkUpdateIt)
{
	if (fireworkUpdateIt->life <= 0)
	{
		if (fireworkIt + 1 == Fireworks.end())
		{
			Fireworks      .pop_back();
			FireworkUpdates.pop_back();
			fireworkIt       = Fireworks.end();
			fireworkUpdateIt = FireworkUpdates.end();
			return;
		}
		else
		{
			*fireworkIt       = Fireworks.back(); // Dead firework, copy last one over it
			*fireworkUpdateIt = FireworkUpdates.back();
		}
		Fireworks      .pop_back(); // Remove last firework after copying
		FireworkUpdates.pop_back();
	}
	else
	{
		// Firework wasn't removed so step to next firework
		++fireworkIt;
		++fireworkUpdateIt;
	}
}


// Update all the fireworks in the two vectors Fireworks and FireworkUpdates. Much of the assignment work will be in this function.
void UpdateFireworks(float frameTime)
{
	// Two matching vectors of data for fireworks (see comment on declaration near top for reason)
	// We will step through both vectors at the same time
	auto fireworkIt       = Fireworks      .begin(); // Using iterators rather than indexes for this code
	auto fireworkUpdateIt = FireworkUpdates.begin();
	while (fireworkIt != Fireworks.end())
	{
		// Handle firework movement
		fireworkIt->position += fireworkUpdateIt->velocity * frameTime;
		fireworkUpdateIt->velocity.y += Gravity * frameTime;

		// Decrease life, but don't remove firework if dead yet - function at end of loop does that
		fireworkUpdateIt->life -= frameTime;

		//-------------------------------
		// SIMPLE STAR - UPDATE IN FLIGHT
		//-------------------------------
		// Simple stars just shrink, fade out and slightly slow down (whilst falling)
		if (fireworkUpdateIt->type == FireworkType::StarSimple)
		{
			fireworkIt->colour.a -= 0.5f * frameTime; // Decrease alpha of stars, they fade out as they fly
			fireworkIt->scale    -= 1.0f * frameTime; // Decrease scale of stars, they get smaller as they fly 
			
			fireworkUpdateIt->velocity *= powf(0.5f, frameTime); // Stars slow down a little, correct way to do frameTime when using *= instead of +=
		}

		//------------------------------------
		// SMALL TRAIL STAR - UPDATE IN FLIGHT
		//------------------------------------
		// Trail stars work like simple stars but emit lots of little, short-lived simple stars behind them, leaving a trail
		if (fireworkUpdateIt->type == FireworkType::StarSmallTrail)
		{
			// First do same update as simple star
			fireworkIt->colour.a -= 0.5f * frameTime; // Decrease alpha of stars, they fade out as they fly
			fireworkIt->scale    -= 1.0f * frameTime; // Decrease scale of stars, they get smaller as they fly 

			fireworkUpdateIt->velocity *= powf(0.5f, frameTime);

			// Stars with trails launch simple stars frequently as they move, use the firework's timer member for this kind of thing
			fireworkUpdateIt->timer -= frameTime;
			while (fireworkUpdateIt->timer <= 0) // Use a while loop in case frame time is slow and we need to emit multiple particles at once
			{
				Firework firework;
				firework.position = fireworkIt->position; // Further fireworks emit from the trail star's position
				firework.scale = 0.75f;                   // Quite small
				firework.colour = fireworkIt->colour;     // Same colour as trail star
				firework.rotation = 0;

				FireworkUpdate fireworkUpdate;
				fireworkUpdate.type     = FireworkType::StarSimple;
				fireworkUpdate.velocity = fireworkUpdateIt->velocity * 0.5f + // Add *half* the trail-star's velocity and they will lag behind leaving a trail
				                          CVector3{ Random(-5.0f, 5.0f), Random(-5.0f, 5.0f), Random(-5.0f, 5.0f) };
				fireworkUpdate.life = 0.4f; // Very short-lived
				AddFirework(firework, fireworkUpdate);

				fireworkUpdateIt->timer += 0.05f;
				// For longer trails have them lag more behind, live longer and emit more frequently
			}
		}

		if (fireworkUpdateIt->type == FireworkType::CometRocket)
		{
			Firework firework;
			firework.position = fireworkIt->position; // Further fireworks emit from the trail star's position
			firework.scale = 0.75f;                   // Quite small
			firework.colour = fireworkIt->colour;     // Same colour as trail star
			firework.rotation = 0;

			FireworkUpdate fireworkUpdate;
			fireworkUpdate.type = FireworkType::StarSimple;
			fireworkUpdate.velocity = fireworkUpdateIt->velocity * 0.5f + // Add *half* the trail-star's velocity and they will lag behind leaving a trail
				CVector3{ Random(-5.0f, 5.0f), Random(-5.0f, 5.0f), Random(-5.0f, 5.0f) };
			fireworkUpdate.life = 0.4f; // Very short-lived
			fireworkUpdate.timer = 0.05f;
			AddFirework(firework, fireworkUpdate);
		}

		//------------------------------------
		// ADD YOUR FIREWORKS IN-FLIGHT UPDATE
		//------------------------------------

	
		//-------------------------------

		// Handle fireworks burst when dead
		if (fireworkUpdateIt->life <= 0)
		{
			//-------------------------------
			// PEONY ROCKET BURST
			//-------------------------------
			if (fireworkUpdateIt->type == FireworkType::PeonyRocket)
			{
				// For PeonyRockets: payloadTypeA is the type of star to launch on burst - random in all directions.
				//                   payloadIntA is the number of stars to launch when it bursts, and payloadColourA is the colour of those stars
				for (int i = 0; i < fireworkUpdateIt->payloadIntA; ++i)
				{
					Firework firework;
					firework.position = fireworkIt->position; // Stars emit from where the rocket is when it burst (life reached 0)
					firework.scale = 1.5f;
					firework.colour = fireworkUpdateIt->payloadColourA; // Rocket contains star colour in its payload
					firework.rotation = 0;

					FireworkUpdate fireworkUpdate;
					fireworkUpdate.type     = fireworkUpdateIt->payloadTypeA;
					fireworkUpdate.velocity = fireworkUpdateIt->velocity + // Add the rocket's velocity at burst to the initial star velocity for more realism
						                      CVector3{ Random(-50.0f, 50.0f), Random(-50.0f, 50.0f), Random(-50.0f, 50.0f) };
					fireworkUpdate.life     = 1.4f; // How long stars last
					fireworkUpdate.timer    = 0;
					AddFirework(firework, fireworkUpdate);
				}
			}

			//-------------------------------
			// ADD YOUR FIREWORK BURSTS
			//-------------------------------
			// E.g. burst that launches two payloads, or burst that launches only in certain directions

			if (fireworkUpdateIt->type == FireworkType::BrocadeRocket)
			{
				for (int i = 0; i < fireworkUpdateIt->payloadIntA; ++i)
				{
					Firework firework;
					firework.position = fireworkIt->position;
					firework.scale = 1.5f;
					firework.colour = fireworkUpdateIt->payloadColourA;
					firework.rotation = 0;

					FireworkUpdate fireworkUpdate;
					fireworkUpdate.type = FireworkType::StarSmallTrail; // <<< MAKE THEM TRAIL STARS
					fireworkUpdate.velocity = fireworkUpdateIt->velocity +
						CVector3{ Random(-60.0f, 60.0f), Random(-60.0f, 60.0f), Random(-60.0f, 60.0f) };
					fireworkUpdate.life = 7.0f; // Live longer so they fall
					fireworkUpdate.timer = 0.05f; // <<< Start emitting small glitter right away!
					AddFirework(firework, fireworkUpdate);
				}
			}

		}

		// Removes firework if it is dead and moves to next firework - made a helper function in case you need it for your code
		RemoveFireworkIfDeadAndMoveToNext(fireworkIt, fireworkUpdateIt);
	}
}

//*************************************************************************


// Update models and camera. frameTime is the time passed since the last frame
void UpdateScene(float frameTime)
{
    // Orbit one light - a bit of a cheat with the static variable
	static float rotate = 0.0f;
    static bool go = true;
	gLights[0].model->SetPosition(CVector3{ cos(rotate) * gLightOrbit, 10, sin(rotate) * gLightOrbit });
    if (go)  rotate -= gLightOrbitSpeed * frameTime;
    if (KeyHit(Key_1))  go = !go;

	// Control camera (will update its view matrix)
	gCamera->Control(frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D );

	// Assignment function
	UpdateFireworks(frameTime);

	// Toggle FPS limiting
	if (KeyHit(Key_P))  lockFPS = !lockFPS;

    // Show frame time / FPS in the window title //
    const float fpsUpdateTime = 0.5f; // How long between updates (in seconds)
    static float totalFrameTime = 0;
    static int frameCount = 0;
    totalFrameTime += frameTime;
    ++frameCount;
    if (totalFrameTime > fpsUpdateTime)
    {
        // Displays FPS rounded to nearest int, and frame time (more useful for developers) in milliseconds to 2 decimal places
        float avgFrameTime = totalFrameTime / frameCount;
        std::ostringstream frameTimeMs;
        frameTimeMs.precision(2);
        frameTimeMs << std::fixed << avgFrameTime * 1000;
        std::string windowTitle = "Fireworks Assessment:  " + frameTimeMs.str() +
                                  "ms, FPS: " + std::to_string(static_cast<int>(1 / avgFrameTime + 0.5f));
        SetWindowTextA(gHWnd, windowTitle.c_str());
        totalFrameTime = 0;
        frameCount = 0;
    }
}
