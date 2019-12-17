typedef struct vec2 vec2;

struct vec2{
	double x;
	double y;
};

typedef struct vec3 vec3;

struct vec3{
	double x;
	double y;
	double z;
};

typedef struct orientation orientation;

struct orientation{
	double real;
	double i;
	double j;
	double k;
};

typedef struct triangle triangle;

struct triangle{
	vec3 p0;
	vec3 p1;
	vec3 p2;
	unsigned char color;
};

typedef struct shape shape;

struct shape{
	unsigned int num_triangles;
	triangle *triangles;
	vec3 center;
	orientation shape_orientation;
};

typedef struct rubiks_cube rubiks_cube;

struct rubiks_cube{
	shape cubies[26];
};
