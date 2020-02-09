#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <curses.h>
#include <time.h>
#include <limits.h>
#include "CAM-curses-ascii-matcher/CAM.h"
#include "menu.h"
#include "3d.h"

CAM_screen *screen;
double **z_buffer;
double normal_constant;
double fov_constant;
vec3 normal;
rubiks_cube r_cube;
unsigned int scramble_moves_left = 0;
orientation current_orientation = (orientation) {.real = 0, .i = 0, .j = 0, .k = 1};

unsigned char faces[6][8] = {{0, 1, 2, 5, 8, 7, 6, 3}, {17, 20, 23, 24, 25, 22, 19, 18}, {23, 20, 17, 9, 0, 3, 6, 14}, {19, 22, 25, 16, 8, 5, 2, 11}, {0, 9, 17, 18, 19, 11, 2, 1}, {6, 7, 8, 16, 25, 24, 23, 14}};
unsigned char faces_middles[6][4] = {{1, 5, 7, 3}, {20, 24, 22, 18}, {9, 3, 14, 20}, {11, 22, 16, 5}, {1, 9, 18, 11}, {7, 16, 24, 14}};
unsigned char faces_corners[6][4] = {{0, 2, 8, 6}, {17, 23, 25, 19}, {6, 23, 17, 0}, {8, 2, 19, 25}, {2, 0, 17, 19}, {6, 8, 25, 23}};
unsigned char faces_centers[6] = {4, 21, 12, 13, 10, 15};
vec3 face_vectors[6] = {(vec3) {.x = 0, .y = 1, .z = 0}, (vec3) {.x = 0, .y = -1, .z = 0}, (vec3) {.x = 1, .y = 0, .z = 0}, (vec3) {.x = -1, .y = 0, .z = 0}, (vec3) {.x = 0, .y = 0, .z = -1}, (vec3) {.x = 0, .y = 0, .z = 1}};
unsigned char animation_face;
unsigned char animation_frame;
unsigned char animation_direction;

vec3 cross(vec3 a, vec3 b){
	vec3 output;

	output.x = a.y*b.z - a.z*b.y;
	output.y = a.z*b.x - a.x*b.z;
	output.z = a.x*b.y - a.y*b.x;

	return output;
}

double dot(vec3 a, vec3 b){
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

vec3 subtract(vec3 a, vec3 b){
	return (vec3) {.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
}

vec3 add(vec3 a, vec3 b){
	return (vec3) {.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z};
}

orientation multiply_quaternions(orientation a, orientation b){
	orientation output;

	output.real = a.real*b.real - a.i*b.i - a.j*b.j - a.k*b.k;
	output.i = a.real*b.i + a.i*b.real + a.j*b.k - a.k*b.j;
	output.j = a.real*b.j - a.i*b.k + a.j*b.real + a.k*b.i;
	output.k = a.real*b.k + a.i*b.j - a.j*b.i + a.k*b.real;

	return output;
}

orientation inverse_quaternion(orientation a){
	double d;

	d = a.real*a.real + a.i*a.i + a.j*a.j + a.k*a.k;
	return (orientation) {.real = a.real/d, .i = -a.i/d, .j = -a.j/d, .k = -a.k/d};
}

vec3 apply_orientation(vec3 v, orientation q){
	orientation p;
	orientation r;

	p.real = 0;
	p.i = v.x;
	p.j = v.y;
	p.k = v.z;
	r = multiply_quaternions(multiply_quaternions(q, p), inverse_quaternion(q));
	return (vec3) {.x = r.i, .y = r.j, .z = r.k};
}

orientation create_orientation(vec3 v, double angle){
	double d;

	d = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
	return (orientation) {.real = cos(angle/2), .i = v.x*sin(angle/2)/d, .j = v.y*sin(angle/2)/d, .k = v.z*sin(angle/2)/d};
}

unsigned char create_z_buffer(){
	unsigned int i;
	unsigned int j;

	z_buffer = malloc(sizeof(double *)*(COLS - 1)*8);
	if(!z_buffer){
		return 1;
	}
	for(i = 0; i < (COLS - 1)*8; i++){
		z_buffer[i] = malloc(sizeof(double)*LINES*13);
		if(!z_buffer[i]){
			for(j = 0; j < i; j++){
				free(z_buffer[j]);
			}
			free(z_buffer);
			return 1;
		}
		for(j = 0; j < LINES*13; j++){
			z_buffer[i][j] = INFINITY;
		}
	}

	return 0;
}

void clear_z_buffer(){
	unsigned int i;
	unsigned int j;

	for(i = 0; i < (COLS - 1)*8; i++){
		for(j = 0; j < LINES*13; j++){
			z_buffer[i][j] = INFINITY;
		}
	}
}

void free_z_buffer(){
	unsigned int i;
	for(i = 0; i < (COLS - 1)*8; i++){
		free(z_buffer[i]);
	}
	free(z_buffer);
}

vec2 get_point_3D(CAM_screen *s, vec3 p){
	vec2 output;

	output.x = p.x*fov_constant/p.z + ((double) s->width)/2;
	output.y = p.y*fov_constant/p.z + ((double) s->height)/2;

	return output;
}

unsigned char pixel_handler(unsigned int x, unsigned int y, unsigned char color){
	double z3d;

	z3d = normal_constant*fov_constant/dot(normal, (vec3) {.x = x - ((double) screen->width)/2, .y = y - ((double) screen->height)/2, .z = fov_constant});
	if(z_buffer[x][y] >= z3d){
		z_buffer[x][y] = z3d;
		return 1;
	} else {
		return 0;
	}
}

void create_cube(shape *s, vec3 center, double width, unsigned char color0, unsigned char color1, unsigned char color2, unsigned char color3, unsigned char color4, unsigned char color5){
	vec3 p0, p1, p2, p3, p4, p5, p6, p7;

	s->num_triangles = 12;
	s->triangles = malloc(sizeof(triangle)*12);
	if(!s->triangles){
		free_z_buffer();
		fprintf(stderr, "Error: not enough memory\n");
		exit(1);
	}

	p0 = (vec3) {.x = -width/2, .y = width/2, .z = -width/2};
	p1 = (vec3) {.x = width/2, .y = width/2, .z = -width/2};
	p2 = (vec3) {.x = width/2, .y = width/2, .z = width/2};
	p3 = (vec3) {.x = -width/2, .y = width/2, .z = width/2};
	p4 = (vec3) {.x = -width/2, .y = -width/2, .z = -width/2};
	p5 = (vec3) {.x = width/2, .y = -width/2, .z = -width/2};
	p6 = (vec3) {.x = width/2, .y = -width/2, .z = width/2};
	p7 = (vec3) {.x = -width/2, .y = -width/2, .z = width/2};

	s->triangles[0] = (triangle) {.p0 = p1, .p1 = p0, .p2 = p2, .color = color0};
	s->triangles[1] = (triangle) {.p0 = p3, .p1 = p2, .p2 = p0, .color = color0};
	s->triangles[2] = (triangle) {.p0 = p4, .p1 = p5, .p2 = p6, .color = color1};
	s->triangles[3] = (triangle) {.p0 = p6, .p1 = p7, .p2 = p4, .color = color1};
	s->triangles[4] = (triangle) {.p0 = p0, .p1 = p1, .p2 = p4, .color = color4};
	s->triangles[5] = (triangle) {.p0 = p1, .p1 = p5, .p2 = p4, .color = color4};
	s->triangles[6] = (triangle) {.p0 = p3, .p1 = p7, .p2 = p2, .color = color5};
	s->triangles[7] = (triangle) {.p0 = p7, .p1 = p6, .p2 = p2, .color = color5};
	s->triangles[8] = (triangle) {.p0 = p0, .p1 = p4, .p2 = p7, .color = color2};
	s->triangles[9] = (triangle) {.p0 = p7, .p1 = p3, .p2 = p0, .color = color2};
	s->triangles[10] = (triangle) {.p0 = p1, .p1 = p2, .p2 = p5, .color = color3};
	s->triangles[11] = (triangle) {.p0 = p5, .p1 = p2, .p2 = p6, .color = color3};

	s->center = center;
	s->shape_orientation = (orientation) {.real = 1, .i = 0, .j = 0, .k = 0};
};

void apply_orientation_triangle(triangle *t, orientation p){
	t->p0 = apply_orientation(t->p0, p);
	t->p1 = apply_orientation(t->p1, p);
	t->p2 = apply_orientation(t->p2, p);
}

void apply_orientation_shape(shape *s){
	unsigned int i;

	for(i = 0; i < s->num_triangles; i++){
		apply_orientation_triangle(s->triangles + i, s->shape_orientation);
	}

	s->shape_orientation = (orientation) {.real = 1, .i = 0, .j = 0, .k = 0};
}

void draw_triangle_3D(CAM_screen *s, triangle t, orientation p, vec3 center){
	vec3 p0;
	vec3 p1;
	vec3 p2;
	vec2 screen_p0;
	vec2 screen_p1;
	vec2 screen_p2;

	p0 = add(center, apply_orientation(t.p0, p));
	p1 = add(center, apply_orientation(t.p1, p));
	p2 = add(center, apply_orientation(t.p2, p));

	screen_p0 = get_point_3D(s, p0);
	screen_p1 = get_point_3D(s, p1);
	screen_p2 = get_point_3D(s, p2);

	normal = cross(subtract(p0, p1), subtract(p0, p2));
	if(dot(normal, p0) < 0){
		normal_constant = dot(normal, p0);

		CAM_set_pixel_handler(pixel_handler);
		CAM_triangle(s, screen_p0.x, screen_p0.y, screen_p1.x, screen_p1.y, screen_p2.x, screen_p2.y, t.color);
		CAM_unset_pixel_handler();
	}
}

void draw_shape_3D(CAM_screen *screen, shape s){
	unsigned int i;

	for(i = 0; i < s.num_triangles; i++){
		draw_triangle_3D(screen, s.triangles[i], s.shape_orientation, s.center);
	}
}

void translate_triangle(triangle *t, vec3 translation){
	t->p0 = add(t->p0, translation);
	t->p1 = add(t->p1, translation);
	t->p2 = add(t->p2, translation);
}

void translate_shape(shape *s, vec3 translation){
	unsigned int i;

	for(i = 0; i < s->num_triangles; i++){
		translate_triangle(s->triangles + i, translation);
	}
}

void rotate_cube(orientation p){
	unsigned char i;

	for(i = 0; i < 26; i++){
		r_cube.cubies[i].shape_orientation = multiply_quaternions(p, r_cube.cubies[i].shape_orientation);
	}
}

void render_cube(){
	unsigned char i;

	for(i = 0; i < 26; i++){
		draw_shape_3D(screen, r_cube.cubies[i]);
	}
}

unsigned char get_face(orientation p, vec3 v){
	vec3 transformed_vector;
	double best_product;
	double product;
	unsigned char best_face;
	unsigned char face;

	transformed_vector = apply_orientation(v, inverse_quaternion(p));
	for(face = 0; face < 6; face++){
		product = dot(transformed_vector, face_vectors[face]);
		if(!face || product < best_product){
			best_face = face;
			best_product = product;
		}
	}

	return best_face;
}

void do_animation_frame(orientation current_orientation){
	unsigned char i;
	orientation rotation;
	shape temp_shape;

	if(animation_frame < 10){
		animation_frame++;
		if(!animation_direction){
			rotation = create_orientation(apply_orientation(face_vectors[animation_face], current_orientation), M_PI/20);
		} else {
			rotation = create_orientation(apply_orientation(face_vectors[animation_face], current_orientation), -M_PI/20);
		}
		for(i = 0; i < 8; i++){
			r_cube.cubies[faces[animation_face][i]].shape_orientation = multiply_quaternions(rotation, r_cube.cubies[faces[animation_face][i]].shape_orientation);
		}
		r_cube.cubies[faces_centers[animation_face]].shape_orientation = multiply_quaternions(rotation, r_cube.cubies[faces_centers[animation_face]].shape_orientation);
		if(animation_frame == 10){
			if(!animation_direction){
				temp_shape = r_cube.cubies[faces_middles[animation_face][3]];
				for(i = 3; i > 0; i--){
					r_cube.cubies[faces_middles[animation_face][i]] = r_cube.cubies[faces_middles[animation_face][i - 1]];
				}
				r_cube.cubies[faces_middles[animation_face][0]] = temp_shape;

				temp_shape = r_cube.cubies[faces_corners[animation_face][3]];
				for(i = 3; i > 0; i--){
					r_cube.cubies[faces_corners[animation_face][i]] = r_cube.cubies[faces_corners[animation_face][i - 1]];
				}
				r_cube.cubies[faces_corners[animation_face][0]] = temp_shape;
			} else {
				temp_shape = r_cube.cubies[faces_middles[animation_face][0]];
				for(i = 0; i < 3; i++){
					r_cube.cubies[faces_middles[animation_face][i]] = r_cube.cubies[faces_middles[animation_face][i + 1]];
				}
				r_cube.cubies[faces_middles[animation_face][3]] = temp_shape;

				temp_shape = r_cube.cubies[faces_corners[animation_face][0]];
				for(i = 0; i < 3; i++){
					r_cube.cubies[faces_corners[animation_face][i]] = r_cube.cubies[faces_corners[animation_face][i + 1]];
				}
				r_cube.cubies[faces_corners[animation_face][3]] = temp_shape;
			}
		}
	}
}

void create_r_cube(){
	//Top slice
	create_cube(r_cube.cubies, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_RED, COLOR_BLUE, COLOR_BLACK, COLOR_YELLOW, COLOR_BLACK);
	translate_shape(r_cube.cubies, (vec3) {.x = -1.1, .y = -1.1, .z = -1.1});
	create_cube(r_cube.cubies + 1, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_RED, COLOR_BLACK, COLOR_BLACK, COLOR_YELLOW, COLOR_BLACK);
	translate_shape(r_cube.cubies + 1, (vec3) {.x = 0, .y = -1.1, .z = -1.1});
	create_cube(r_cube.cubies + 2, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_RED, COLOR_BLACK, COLOR_GREEN, COLOR_YELLOW, COLOR_BLACK);
	translate_shape(r_cube.cubies + 2, (vec3) {.x = 1.1, .y = -1.1, .z = -1.1});
	create_cube(r_cube.cubies + 3, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_RED, COLOR_BLUE, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
	translate_shape(r_cube.cubies + 3, (vec3) {.x = -1.1, .y = -1.1, .z = 0});
	create_cube(r_cube.cubies + 4, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_RED, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
	translate_shape(r_cube.cubies + 4, (vec3) {.x = 0, .y = -1.1, .z = 0});
	create_cube(r_cube.cubies + 5, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_RED, COLOR_BLACK, COLOR_GREEN, COLOR_BLACK, COLOR_BLACK);
	translate_shape(r_cube.cubies + 5, (vec3) {.x = 1.1, .y = -1.1, .z = 0});
	create_cube(r_cube.cubies + 6, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_RED, COLOR_BLUE, COLOR_BLACK, COLOR_BLACK, COLOR_CYAN);
	translate_shape(r_cube.cubies + 6, (vec3) {.x = -1.1, .y = -1.1, .z = 1.1});
	create_cube(r_cube.cubies + 7, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_RED, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_CYAN);
	translate_shape(r_cube.cubies + 7, (vec3) {.x = 0, .y = -1.1, .z = 1.1});
	create_cube(r_cube.cubies + 8, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_RED, COLOR_BLACK, COLOR_GREEN, COLOR_BLACK, COLOR_CYAN);
	translate_shape(r_cube.cubies + 8, (vec3) {.x = 1.1, .y = -1.1, .z = 1.1});

	//Middle slice
	create_cube(r_cube.cubies + 9, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_BLACK, COLOR_BLUE, COLOR_BLACK, COLOR_YELLOW, COLOR_BLACK);
	translate_shape(r_cube.cubies + 9, (vec3) {.x = -1.1, .y = 0, .z = -1.1});
	create_cube(r_cube.cubies + 10, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_YELLOW, COLOR_BLACK);
	translate_shape(r_cube.cubies + 10, (vec3) {.x = 0, .y = 0, .z = -1.1});
	create_cube(r_cube.cubies + 11, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_GREEN, COLOR_YELLOW, COLOR_BLACK);
	translate_shape(r_cube.cubies + 11, (vec3) {.x = 1.1, .y = 0, .z = -1.1});
	create_cube(r_cube.cubies + 12, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_BLACK, COLOR_BLUE, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
	translate_shape(r_cube.cubies + 12, (vec3) {.x = -1.1, .y = 0, .z = 0});
	create_cube(r_cube.cubies + 13, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_GREEN, COLOR_BLACK, COLOR_BLACK);
	translate_shape(r_cube.cubies + 13, (vec3) {.x = 1.1, .y = 0, .z = 0});
	create_cube(r_cube.cubies + 14, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_BLACK, COLOR_BLUE, COLOR_BLACK, COLOR_BLACK, COLOR_CYAN);
	translate_shape(r_cube.cubies + 14, (vec3) {.x = -1.1, .y = 0, .z = 1.1});
	create_cube(r_cube.cubies + 15, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_CYAN);
	translate_shape(r_cube.cubies + 15, (vec3) {.x = 0, .y = 0, .z = 1.1});
	create_cube(r_cube.cubies + 16, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_GREEN, COLOR_BLACK, COLOR_CYAN);
	translate_shape(r_cube.cubies + 16, (vec3) {.x = 1.1, .y = 0, .z = 1.1});

	//Bottom slice
	create_cube(r_cube.cubies + 17, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_MAGENTA, COLOR_BLACK, COLOR_BLUE, COLOR_BLACK, COLOR_YELLOW, COLOR_BLACK);
	translate_shape(r_cube.cubies + 17, (vec3) {.x = -1.1, .y = 1.1, .z = -1.1});
	create_cube(r_cube.cubies + 18, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_MAGENTA, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_YELLOW, COLOR_BLACK);
	translate_shape(r_cube.cubies + 18, (vec3) {.x = 0, .y = 1.1, .z = -1.1});
	create_cube(r_cube.cubies + 19, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_MAGENTA, COLOR_BLACK, COLOR_BLACK, COLOR_GREEN, COLOR_YELLOW, COLOR_BLACK);
	translate_shape(r_cube.cubies + 19, (vec3) {.x = 1.1, .y = 1.1, .z = -1.1});
	create_cube(r_cube.cubies + 20, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_MAGENTA, COLOR_BLACK, COLOR_BLUE, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
	translate_shape(r_cube.cubies + 20, (vec3) {.x = -1.1, .y = 1.1, .z = 0});
	create_cube(r_cube.cubies + 21, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_MAGENTA, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
	translate_shape(r_cube.cubies + 21, (vec3) {.x = 0, .y = 1.1, .z = 0});
	create_cube(r_cube.cubies + 22, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_MAGENTA, COLOR_BLACK, COLOR_BLACK, COLOR_GREEN, COLOR_BLACK, COLOR_BLACK);
	translate_shape(r_cube.cubies + 22, (vec3) {.x = 1.1, .y = 1.1, .z = 0});
	create_cube(r_cube.cubies + 23, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_MAGENTA, COLOR_BLACK, COLOR_BLUE, COLOR_BLACK, COLOR_BLACK, COLOR_CYAN);
	translate_shape(r_cube.cubies + 23, (vec3) {.x = -1.1, .y = 1.1, .z = 1.1});
	create_cube(r_cube.cubies + 24, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_MAGENTA, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_CYAN);
	translate_shape(r_cube.cubies + 24, (vec3) {.x = 0, .y = 1.1, .z = 1.1});
	create_cube(r_cube.cubies + 25, (vec3) {.x = 0, .y = 0, .z = 5.5}, 1, COLOR_MAGENTA, COLOR_BLACK, COLOR_BLACK, COLOR_GREEN, COLOR_BLACK, COLOR_CYAN);
	translate_shape(r_cube.cubies + 25, (vec3) {.x = 1.1, .y = 1.1, .z = 1.1});
}

void free_r_cube(){
	unsigned int i;

	for(i = 0; i < 26; i++){
		free(r_cube.cubies[i].triangles);
	}
}

orientation create_random_rotation(){
	vec3 v;

	v = (vec3) {.x = rand()%100 - 50, .y = rand()%100 - 50, .z = rand()%100 - 50};

	return create_orientation(v, 0.005);
}

unsigned int get_random_seed(){
	struct timespec current_time;

	clock_gettime(CLOCK_MONOTONIC, &current_time);
	return current_time.tv_nsec%UINT_MAX;
}

unsigned int get_seed_from_string(char *string){
	unsigned int seed = 0;

	while(*string){
		seed = (seed<<7) | *string;
		string++;
	}

	return seed;
}

unsigned char write_shape(shape s, unsigned int max_triangles, FILE *fp){
	if(!fwrite(&(s.num_triangles), sizeof(unsigned int), 1, fp) || s.num_triangles > max_triangles){
		return 1;
	}
	if(fwrite(s.triangles, sizeof(triangle), s.num_triangles, fp) < s.num_triangles){
		return 1;
	}
	if(!fwrite(&(s.center), sizeof(vec3), 1, fp)){
		return 1;
	}
	if(!fwrite(&(s.shape_orientation), sizeof(orientation), 1, fp)){
		return 1;
	}

	return 0;
}

unsigned char read_shape(shape *s, unsigned int max_triangles, FILE *fp){
	if(!fread(&(s->num_triangles), sizeof(unsigned int), 1, fp) || s->num_triangles > max_triangles){
		return 1;
	}
	if(fread(s->triangles, sizeof(triangle), s->num_triangles, fp) < s->num_triangles){
		return 1;
	}
	if(!fread(&(s->center), sizeof(vec3), 1, fp)){
		return 1;
	}
	if(!fread(&(s->shape_orientation), sizeof(orientation), 1, fp)){
		return 1;
	}

	return 0;
}

unsigned char write_rubiks_cube(FILE *fp){
	unsigned char i;

	for(i = 0; i < 26; i++){
		if(write_shape(r_cube.cubies[i], 12, fp)){
			return 1;
		}
	}
	if(!fwrite(&current_orientation, sizeof(orientation), 1, fp)){
		return 1;
	}

	return 0;
}

unsigned char read_rubiks_cube(FILE *fp){
	unsigned char i;

	for(i = 0; i < 26; i++){
		if(read_shape(r_cube.cubies + i, 12, fp)){
			return 1;
		}
	}
	if(!fread(&current_orientation, sizeof(orientation), 1, fp)){
		return 1;
	}

	return 0;
}

unsigned char open_menu(){
	menu main_menu;
	text_entry seed_entry;
	char *menu_items[5] = {"Resume", "Scramble", "Save", "Load", "Exit"};
	char input_buffer[32];
	unsigned int seed;
	FILE *fp;

	main_menu = create_menu("Options", menu_items, 5, 1, 2);

	do_menu(&main_menu);

	switch(main_menu.current_choice){
		case 0:
			free_menu(main_menu);
			break;
		case 1:
			free_menu(main_menu);
			seed_entry = create_text_entry("Enter Seed", 1, 2);
			do_text_entry(&seed_entry, input_buffer, 32);
			free_text_entry(seed_entry);
			if(input_buffer[0]){
				seed = get_seed_from_string(input_buffer);
			} else {
				seed = get_random_seed();
			}
			srand(seed);
			scramble_moves_left = 30;
			break;
		case 2:
			free_menu(main_menu);
			seed_entry = create_text_entry("Enter save name", 1, 2);
			do_text_entry(&seed_entry, input_buffer, 32);
			free_text_entry(seed_entry);
			fp = fopen(input_buffer, "wb");
			if(!fp){
				main_menu = create_menu("Error saving file", menu_items, 1, 1, 2);
				do_menu(&main_menu);
				free_menu(main_menu);
				return 0;
			}
			if(write_rubiks_cube(fp)){
				fclose(fp);
				main_menu = create_menu("Error saving file", menu_items, 1, 1, 2);
				do_menu(&main_menu);
				free_menu(main_menu);
				return 0;
			}
			fclose(fp);
			return 0;
		case 3:
			free_menu(main_menu);
			seed_entry = create_text_entry("Enter load name", 1, 2);
			do_text_entry(&seed_entry, input_buffer, 32);
			free_text_entry(seed_entry);
			fp = fopen(input_buffer, "rb");
			if(!fp){
				main_menu = create_menu("Error loading file", menu_items, 1, 1, 2);
				do_menu(&main_menu);
				free_menu(main_menu);
				return 0;
			}
			if(read_rubiks_cube(fp)){
				fclose(fp);
				free_r_cube();
				create_r_cube();
				main_menu = create_menu("Error loading file", menu_items, 1, 1, 2);
				do_menu(&main_menu);
				free_menu(main_menu);
				return 0;
			}
			fclose(fp);
			return 0;
		case 4:
			free_menu(main_menu);
			return 1;
	}

	return 0;
}

int main(int argc, char **argv){
	orientation x_rotate;
	orientation y_rotate;
	orientation z_rotate;
	struct timespec current_time;
	struct timespec sleep_time;
	unsigned long int last_nanoseconds;
	unsigned long int current_nanoseconds;
	unsigned long int sleep_nanoseconds;
	unsigned char do_update;
	unsigned char quit = 0;
	unsigned char first_frame = 1;
	int key;

	animation_frame = 10;
	memset(&r_cube, 0, sizeof(rubiks_cube));
	initscr();
	cbreak();
	curs_set(0);
	nodelay(stdscr, 1);
	keypad(stdscr, 1);
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_CYAN);
	CAM_init(3);

	if(create_z_buffer()){
		fprintf(stderr, "Error: not enough memory\n");
		exit(1);
	}
	create_r_cube();

	x_rotate = create_orientation((vec3) {.x = 1, .y = 0, .z = 0}, 0.2);
	y_rotate = create_orientation((vec3) {.x = 0, .y = 1, .z = 0}, 0.2);
	z_rotate = create_orientation((vec3) {.x = 0, .y = 0, .z = 1}, 0.2);
	screen = CAM_screen_create(stdscr, COLS - 1, LINES);
	if(screen->width > screen->height){
		fov_constant = screen->height*4/5;
	} else {
		fov_constant = screen->width*4/5;
	}

	clock_gettime(CLOCK_MONOTONIC, &current_time);
	last_nanoseconds = current_time.tv_sec*1000000000 + current_time.tv_nsec;
	while(!quit){
		if(!first_frame){
			do_update = 0;
		} else {
			do_update = 1;
			first_frame = 0;
		}
		key = getch();
		switch(key){
			case KEY_DOWN:
				rotate_cube(x_rotate);
				current_orientation = multiply_quaternions(x_rotate, current_orientation);
				do_update = 1;
				break;
			case KEY_UP:
				rotate_cube(inverse_quaternion(x_rotate));
				current_orientation = multiply_quaternions(inverse_quaternion(x_rotate), current_orientation);
				do_update = 1;
				break;
			case KEY_RIGHT:
				rotate_cube(inverse_quaternion(y_rotate));
				current_orientation = multiply_quaternions(inverse_quaternion(y_rotate), current_orientation);
				do_update = 1;
				break;
			case KEY_LEFT:
				rotate_cube(y_rotate);
				current_orientation = multiply_quaternions(y_rotate, current_orientation);
				do_update = 1;
				break;
			case 'm':
				quit = open_menu();
				werase(stdscr);
				do_update = 1;
				clock_gettime(CLOCK_MONOTONIC, &current_time);
				last_nanoseconds = current_time.tv_sec*1000000000 + current_time.tv_nsec;
				break;
		}
		if(scramble_moves_left){
			if(animation_frame == 10){
				animation_frame = 0;
				animation_face = (animation_face + rand()%5 + 1)%6;
				animation_direction = rand()&1;
				scramble_moves_left--;
			}
		} else {
			switch(key){
				case 'z':
					rotate_cube(z_rotate);
					current_orientation = multiply_quaternions(z_rotate, current_orientation);
					do_update = 1;
					break;
				case 'Z':
					rotate_cube(inverse_quaternion(z_rotate));
					current_orientation = multiply_quaternions(inverse_quaternion(z_rotate), current_orientation);
					do_update = 1;
					break;
				case 'f':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = 0, .y = 0, .z = 1});
						animation_frame = 0;
						animation_direction = 0;
					}
					break;
				case 'F':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = 0, .y = 0, .z = 1});
						animation_frame = 0;
						animation_direction = 1;
					}
					break;
				case 'u':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = 0, .y = 1, .z = 0});
						animation_frame = 0;
						animation_direction = 0;
					}
					break;
				case 'U':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = 0, .y = 1, .z = 0});
						animation_frame = 0;
						animation_direction = 1;
					}
					break;
				case 'd':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = 0, .y = -1, .z = 0});
						animation_frame = 0;
						animation_direction = 0;
					}
					break;
				case 'D':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = 0, .y = -1, .z = 0});
						animation_frame = 0;
						animation_direction = 1;
					}
					break;
				case 'l':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = 1, .y = 0, .z = 0});
						animation_frame = 0;
						animation_direction = 0;
					}
					break;
				case 'L':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = 1, .y = 0, .z = 0});
						animation_frame = 0;
						animation_direction = 1;
					}
					break;
				case 'r':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = -1, .y = 0, .z = 0});
						animation_frame = 0;
						animation_direction = 0;
					}
					break;
				case 'R':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = -1, .y = 0, .z = 0});
						animation_frame = 0;
						animation_direction = 1;
					}
					break;
				case 'b':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = 0, .y = 0, .z = -1});
						animation_frame = 0;
						animation_direction = 0;
					}
					break;
				case 'B':
					if(animation_frame >= 10){
						animation_face = get_face(current_orientation, (vec3) {.x = 0, .y = 0, .z = -1});
						animation_frame = 0;
						animation_direction = 1;
					}
					break;
			}
		}
		if(animation_frame < 10){
			do_update = 1;
		}
		do_animation_frame(current_orientation);
		if(do_update){
			clear_z_buffer();
			CAM_fill(screen, COLOR_WHITE);
			render_cube();
			CAM_update(screen);
			refresh();
		}

		clock_gettime(CLOCK_MONOTONIC, &current_time);
		current_nanoseconds = current_time.tv_sec*1000000000 + current_time.tv_nsec;
		if(current_nanoseconds - last_nanoseconds < 50000000){
			sleep_nanoseconds = 50000000 + last_nanoseconds - current_nanoseconds;
			sleep_time.tv_sec = sleep_nanoseconds/1000000000;
			sleep_time.tv_nsec = sleep_nanoseconds - sleep_time.tv_sec*1000000000;
			nanosleep(&sleep_time, NULL);
		}
		last_nanoseconds += 50000000;
	}

	CAM_screen_free(screen);
	endwin();
	free_z_buffer();
	free_r_cube();

	return 0;
}

