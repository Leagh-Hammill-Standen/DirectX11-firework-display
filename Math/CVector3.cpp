//--------------------------------------------------------------------------------------
// Vector3 class (cut down version), to hold points and vectors
//--------------------------------------------------------------------------------------

#include "CVector3.h"


/*-----------------------------------------------------------------------------------------
    Operators
-----------------------------------------------------------------------------------------*/

// Addition of another vector to this one, e.g. Position += Velocity
CVector3& CVector3::operator+= (const CVector3& v)
{
    x += v.x;
    y += v.y;
    z += v.z;
    return *this;
}

// Subtraction of another vector from this one, e.g. Velocity -= Gravity
CVector3& CVector3::operator-= (const CVector3& v)
{
    x -= v.x;
    y -= v.y;
    z -= v.z;
    return *this;
}

// Negate this vector (e.g. Velocity = -Velocity)
CVector3& CVector3::operator- ()
{
    x = -x;
    y = -y;
    z = -z;
    return *this;
}

// Plus sign in front of vector - called unary positive and usually does nothing. Included for completeness (e.g. Velocity = +Velocity)
CVector3& CVector3::operator+ ()
{
    return *this;
}


// Multiply vector by scalar (scales vector);
CVector3& CVector3::operator*= (const float s)
{
    x *= s;
    y *= s;
    z *= s;
    return *this;
}


// Vector-vector addition
CVector3 operator+ (const CVector3& v, const CVector3& w)
{
    return CVector3{ v.x + w.x, v.y + w.y, v.z + w.z };
}

// Vector-vector subtraction
CVector3 operator- (const CVector3& v, const CVector3& w)
{
    return CVector3{ v.x - w.x, v.y - w.y, v.z - w.z };
}

// Vector-scalar multiplication
CVector3 operator* (const CVector3& v, float s)
{
    return CVector3{ v.x * s, v.y * s, v.z * s };
}
CVector3 operator* (float s, const CVector3& v)
{
    return CVector3{ v.x * s, v.y * s, v.z * s };
}

/*-----------------------------------------------------------------------------------------
    Non-member functions
-----------------------------------------------------------------------------------------*/

// Dot product of two given vectors (order not important) - non-member version
float Dot(const CVector3& v1, const CVector3& v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

// Cross product of two given vectors (order is important) - non-member version
CVector3 Cross(const CVector3& v1, const CVector3& v2)
{
    return CVector3{ v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x };
}

// Return unit length vector in the same direction as given one
CVector3 Normalise(const CVector3& v)
{
    float lengthSq = v.x*v.x + v.y*v.y + v.z*v.z;

    // Ensure vector is not zero length (use BaseMath.h float approx. fn with default epsilon)
    if (IsZero(lengthSq))
    {
        return CVector3{ 0.0f, 0.0f, 0.0f };
    }
    else
    {
        float invLength = InvSqrt(lengthSq);
        return CVector3{ v.x * invLength, v.y * invLength, v.z * invLength };
    }
}


// Returns length of a vector
float Length(const CVector3& v)
{
    return sqrt(Dot(v, v));
}



/// Creates a random vector within a cone of "angle" degrees of the given "direction"
// E.g. CVector3 randomDir = RandomVector(myDir, 15); // Get a random vector going roughly in the same directions as "myDir", within 15 degrees
// or   CVector3 anyDir = RandomVector(CVector3(0, 1, 0), 180); // Get a random vector in any direction
CVector3 RandomVectorInCone(const CVector3& direction, float angle)
{
	// Maths here is not important for the assignment - useful function to have in your toolbox
	// 
	// Normalise vector, angles to radians
	CVector3 n = Normalise(direction);
	float radians = ToRadians(angle);

	// Random deviation and rotation
	float a = Random(0.0f, ToRadians(angle));
	float b = Random(0.0f, ToRadians(360.0f));

	// Create two perpendicular unit vectors that are orthogonal to dir
	CVector3 u;

	// Find the first perpendicular vector
	if (std::abs(n.x) < std::abs(n.y) && std::abs(n.x) < std::abs(n.z))
		u = Normalise(CVector3{ 0, -n.z, n.y }); // If x is the smallest component, create a perpendicular vector in the x direction
	else if (std::abs(n.y) < std::abs(n.z))
		u = Normalise(CVector3{ -n.z, 0, n.x }); // If y is the smallest component, create a perpendicular vector in the y direction
	else
		u = Normalise(CVector3{ -n.y, n.x, 0 }); // If z is the smallest component, create a perpendicular vector in the z direction

	// Find the second perpendicular vector using cross product
	CVector3 v = { n.y * u.z - n.z * u.y,
				   n.z * u.x - n.x * u.z,
				   n.x * u.y - n.y * u.x };

	// Step 4: Compute the random vector by rotating n by "a" in a random direction determined by "b"
	float sin_a = std::sin(a);
	float cos_a = std::cos(a);
	float sin_b = std::sin(b);
	float cos_b = std::cos(b);

	CVector3 result = { n.x * cos_a + (u.x * cos_b + v.x * sin_b) * sin_a,
						n.y * cos_a + (u.y * cos_b + v.y * sin_b) * sin_a,
						n.z * cos_a + (u.z * cos_b + v.z * sin_b) * sin_a };

	return result;
}

