struct vec3d
{
    float x, y, z;

    vec3d()
        : x(0.0), y(0.0), z(0.0)
    {}

    vec3d(float _x, float _y, float _z)
        : x(_x), y(_y), z(_z)
    {}

    // vector addition
    vec3d operator +(const vec3d& other)
    {
        return vec3d(x + other.x, y + other.y, z + other.z);
    }

    void operator +=(const vec3d& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
    }

    // vector subtraction
    vec3d operator -(const vec3d& other)
    {
        return vec3d(x - other.x, y - other.y, z - other.z);
    }

    void operator -=(const vec3d& other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
    }

    // vector multiplication with scalar value
    vec3d operator *(float d)
    {
        return vec3d(x * d, y * d, z * d);
    }

    void operator *=(float d)
    {
        x *= d;
        y *= d;
        z *= d;
    }

    // dot product
    float dot(const vec3d& other)
    {
        return x * other.x + y * other.y + z * other.z;
    }

    // cross product
    vec3d cross(const vec3d& other)
    {
        return vec3d((y * other.z) - (z * other.y),
                     (z * other.x) - (x * other.z),
                     (x * other.y) - (y * other.x));
    }

    float length()
    {
        return sqrt(x * x + y * y + z * z);
    }

    void normalize()
    {
        float len1 = 1.0 / length();
        x *= len1;
        y *= len1;
        z *= len1;
    }
};
