#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <curses.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include "CAM-curses-ascii-matcher/CAM.h"
#include "menu.h"
#include "3d.h"

#define ORIENT_FRAMES 5

#ifndef M_PI

#define M_PI 3.14159265358979323846

#endif

struct sigaction curses_old_action;
unsigned int z_buffer_cols;
unsigned int z_buffer_lines;

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

unsigned char reorient_frame;
orientation reorient_orientation;
orientation original_orientation;

vec3 corner_vectors[4] = {
	(vec3) {.x = -1, .y = 1, .z = -1},
	(vec3) {.x = -1, .y = 1, .z = 1},
	(vec3) {.x = 1, .y = 1, .z = -1},
	(vec3) {.x = 1, .y = 1, .z = 1}
};

vec3 edge_vectors[6] = {
	(vec3) {.x = 0, .y = 1, .z = -1},
	(vec3) {.x = 0, .y = 1, .z = 1},
	(vec3) {.x = 1, .y = 1, .z = 0},
	(vec3) {.x = -1, .y = 1, .z = 0},
	(vec3) {.x = 1, .y = 0, .z = -1},
	(vec3) {.x = -1, .y = 0, .z = -1}
};

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

orientation normalize_quaternion(orientation a){
	orientation output;
	double length;

	length = sqrt(a.real*a.real + a.i*a.i + a.j*a.j + a.k*a.k);
	output.real = a.real/length;
	output.i = a.i/length;
	output.j = a.j/length;
	output.k = a.k/length;

	return output;
}

orientation positive_orientation(orientation a){
	return (orientation) {.real = fabs(a.real), .i = fabs(a.i), .j = fabs(a.j), .k = fabs(a.k)};
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

double orientation_inner_product(orientation a, orientation b){
	return a.real*b.real + a.i*b.i + a.j*b.j + a.k*b.k;
}

double orientation_score(orientation a, orientation b){
	double prod;

	prod = orientation_inner_product(a, b);
	return acos(2*prod*prod - 1);
}

orientation average_orientations(orientation a, orientation b, double weight){
	return (orientation) {.real = a.real*weight + b.real*(1 - weight), .i = a.i*weight + b.i*(1 - weight), .j = a.j*weight + b.j*(1 - weight), .k = a.k*weight + b.k*(1 - weight)};
}

orientation orientation_from_to(vec3 a, vec3 b){
	orientation output;
	double a_len_squared;
	double b_len_squared;
	double d;
	vec3 c;

	d = dot(a, b);
	if(d < -0.9999)
		return (orientation) {.real = 0, .i = 1, .j = 0, .k = 0};
	c = cross(a, b);
	output.i = c.x;
	output.j = c.y;
	output.k = c.z;
	a_len_squared = dot(a, a);
	b_len_squared = dot(b, b);
	output.real = sqrt(a_len_squared*b_len_squared) + d;

	return normalize_quaternion(output);
}

unsigned char create_z_buffer(){
	unsigned int i;
	unsigned int j;

	z_buffer_cols = COLS;
	z_buffer_lines = LINES;

	z_buffer = malloc(sizeof(double *)*(z_buffer_cols - 1)*8);
	if(!z_buffer){
		return 1;
	}
	for(i = 0; i < (z_buffer_cols - 1)*8; i++){
		z_buffer[i] = malloc(sizeof(double)*z_buffer_lines*13);
		if(!z_buffer[i]){
			for(j = 0; j < i; j++){
				free(z_buffer[j]);
			}
			free(z_buffer);
			return 1;
		}
		for(j = 0; j < z_buffer_lines*13; j++){
			z_buffer[i][j] = INFINITY;
		}
	}

	return 0;
}

void clear_z_buffer(){
	unsigned int i;
	unsigned int j;

	for(i = 0; i < (z_buffer_cols - 1)*8; i++){
		for(j = 0; j < z_buffer_lines*13; j++){
			z_buffer[i][j] = INFINITY;
		}
	}
}

void free_z_buffer(){
	unsigned int i;
	for(i = 0; i < (z_buffer_cols - 1)*8; i++){
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
		return;
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

void start_reorientation(){
	double best_score;
	double score;
	unsigned int i;
	unsigned int j;
	orientation check_orientation;

	original_orientation = current_orientation;
	reorient_orientation = (orientation) {.real = 1, .i = 0, .j = 0, .k = 0};
	best_score = orientation_score(original_orientation, reorient_orientation);

	for(i = 0; i < 3; i++){
		for(j = 1; j < 4; j++){
			check_orientation = create_orientation(face_vectors[i*2], j*M_PI/2);
			score = orientation_score(original_orientation, check_orientation);
			if(score < best_score){
				best_score = score;
				reorient_orientation = check_orientation;
			}
		}
	}

	for(i = 0; i < 4; i++){
		for(j = 1; j < 3; j++){
			check_orientation = create_orientation(corner_vectors[i], j*2*M_PI/3);
			score = orientation_score(original_orientation, check_orientation);
			if(score < best_score){
				best_score = score;
				reorient_orientation = check_orientation;
			}
		}
	}

	for(i = 0; i < 6; i++){
		check_orientation = create_orientation(edge_vectors[i], M_PI);
		score = orientation_score(original_orientation, check_orientation);
		if(score < best_score){
			best_score = score;
			reorient_orientation = check_orientation;
		}
	}

	reorient_frame = 0;
}

void do_reorient_frame(){
	orientation next_rotation;
	orientation total_rotation;
	vec3 vec;
	double total_angle;

	if(reorient_frame < ORIENT_FRAMES){
		reorient_frame++;
		total_rotation = multiply_quaternions(reorient_orientation, inverse_quaternion(original_orientation));
		vec = (vec3) {.x = total_rotation.i, .y = total_rotation.j, .z = total_rotation.k};
		total_angle = 2*acos(total_rotation.real);
		if(total_angle > M_PI)
			total_angle = total_angle - 2*M_PI;
		next_rotation = create_orientation(vec, total_angle/ORIENT_FRAMES);
		current_orientation = multiply_quaternions(next_rotation, current_orientation);
		rotate_cube(next_rotation);
	}
}

unsigned char check_r_cube(){
	unsigned int i;

	for(i = 0; i < 26; i++){
		if(!r_cube.cubies[i].triangles)
			return 1;
	}

	return 0;
}

void free_r_cube(){
	unsigned int i;

	for(i = 0; i < 26; i++){
		free(r_cube.cubies[i].triangles);
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

	if(check_r_cube()){
		free_r_cube();
		free_z_buffer();
		CAM_screen_free(screen);
		endwin();
		fprintf(stderr, "Error: not enough memory\n");
		exit(1);
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
	menu unable_menu;
	text_entry seed_entry;
	char *menu_items[5] = {"Resume", "Scramble", "Save", "Load", "Exit"};
	char *ok_item = "Ok";
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
			if(animation_frame < 10 || scramble_moves_left){
				unable_menu = create_menu("Unable to save during animation", &ok_item, 1, 1, 2);
				do_menu(&unable_menu);
				free_menu(unable_menu);
				return 0;
			}
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
			if(animation_frame < 10 || scramble_moves_left){
				unable_menu = create_menu("Unable to load during animation", &ok_item, 1, 1, 2);
				do_menu(&unable_menu);
				free_menu(unable_menu);
				return 0;
			}
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

void resize(int sig, siginfo_t *sinfo, void *v){
	if(curses_old_action.sa_flags&SA_SIGINFO)
		curses_old_action.sa_sigaction(sig, sinfo, v);
	else
		curses_old_action.sa_handler(sig);
	endwin();
	refresh();
	CAM_screen_free(screen);
	screen = CAM_screen_create(stdscr, COLS - 1, LINES);
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_CYAN);
	CAM_init(3);
	if(screen->width > screen->height){
		fov_constant = screen->height*4/5;
	} else {
		fov_constant = screen->width*4/5;
	}
	free_z_buffer();
	create_z_buffer();
}

void install_resize_handler(){
	struct sigaction new_action;

	new_action.sa_sigaction = resize;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = SA_SIGINFO;
	new_action.sa_restorer = NULL;

	sigaction(SIGWINCH, &new_action, &curses_old_action);
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
	sigset_t ignore_winch;

	sigemptyset(&ignore_winch);
	sigaddset(&ignore_winch, SIGWINCH);

	animation_frame = 10;
	reorient_frame = ORIENT_FRAMES;
	memset(&r_cube, 0, sizeof(rubiks_cube));
	initscr();
	cbreak();
	curs_set(0);
	nodelay(stdscr, 1);
	keypad(stdscr, 1);
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_CYAN);
	if(CAM_init(3)){
		endwin();
		fprintf(stderr, "Error: terminal does not support colors\n");
		return 1;
	}

	if(create_z_buffer()){
		endwin();
		fprintf(stderr, "Error: not enough memory\n");
		return 1;
	}
	create_r_cube();

	x_rotate = create_orientation((vec3) {.x = 1, .y = 0, .z = 0}, M_PI/16);
	y_rotate = create_orientation((vec3) {.x = 0, .y = 1, .z = 0}, M_PI/16);
	z_rotate = create_orientation((vec3) {.x = 0, .y = 0, .z = 1}, M_PI/16);
	screen = CAM_screen_create(stdscr, COLS - 1, LINES);
	if(screen->width > screen->height){
		fov_constant = screen->height*4/5;
	} else {
		fov_constant = screen->width*4/5;
	}

	install_resize_handler();

	clock_gettime(CLOCK_MONOTONIC, &current_time);
	last_nanoseconds = current_time.tv_sec*1000000000 + current_time.tv_nsec;
	sigprocmask(SIG_BLOCK, &ignore_winch, NULL);
	while(!quit){
		sigprocmask(SIG_UNBLOCK, &ignore_winch, NULL);
		sigprocmask(SIG_BLOCK, &ignore_winch, NULL);
		if(!first_frame){
			do_update = 0;
		} else {
			do_update = 1;
			first_frame = 0;
		}
		key = getch();
		switch(key){
			case KEY_UP:
				if(reorient_frame >= ORIENT_FRAMES){
					rotate_cube(x_rotate);
					current_orientation = multiply_quaternions(x_rotate, current_orientation);
					do_update = 1;
				}
				break;
			case KEY_DOWN:
				if(reorient_frame >= ORIENT_FRAMES){
					rotate_cube(inverse_quaternion(x_rotate));
					current_orientation = multiply_quaternions(inverse_quaternion(x_rotate), current_orientation);
					do_update = 1;
				}
				break;
			case KEY_LEFT:
				if(reorient_frame >= ORIENT_FRAMES){
					rotate_cube(inverse_quaternion(y_rotate));
					current_orientation = multiply_quaternions(inverse_quaternion(y_rotate), current_orientation);
					do_update = 1;
				}
				break;
			case KEY_RIGHT:
				if(reorient_frame >= ORIENT_FRAMES){
					rotate_cube(y_rotate);
					current_orientation = multiply_quaternions(y_rotate, current_orientation);
					do_update = 1;
				}
				break;
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
			case ' ':
				if(reorient_frame >= ORIENT_FRAMES){
					start_reorientation();
				}
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
		if(animation_frame < 10 || reorient_frame < ORIENT_FRAMES){
			do_update = 1;
		}
		do_animation_frame(current_orientation);
		do_reorient_frame();
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

