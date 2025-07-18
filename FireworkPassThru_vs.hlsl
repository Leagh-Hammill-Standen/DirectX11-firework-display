//--------------------------------------------------------------------------------------
// Pass-Through Vertex Shader for Fireworks
//--------------------------------------------------------------------------------------
// Vertex shader that passes on firework data to the geometry shader without change

#include "Common.hlsli"


//-----------------------------------------------------------------------------
// Main function
//-----------------------------------------------------------------------------

// Main vertex shader function
Firework main(Firework input)
{
	return input;
}
