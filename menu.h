#ifndef MENU_INCLUDED
#define MENU_INCLUDED
typedef struct menu menu;

struct menu{
	char *title;
	char **items;
	unsigned int num_items;
	unsigned int current_choice;
	unsigned int width;
	unsigned char color;
	unsigned char select_color;
	unsigned char selected;
	WINDOW *win;
};

typedef struct text_entry text_entry;

struct text_entry{
	char *title;
	unsigned char cursor_pos;
	unsigned int width;
	unsigned char color;
	unsigned char select_color;
	unsigned char done;
	WINDOW *win;
};

menu create_menu(char *title, char **items, unsigned int num_items, unsigned char color, unsigned char select_color);

text_entry create_text_entry(char *title, unsigned char color, unsigned char select_color);

void free_menu(menu m);

void free_text_entry(text_entry t);

void render_menu(menu m);

void render_text_entry(text_entry t, char *buffer);

void do_menu(menu *m);

void do_text_entry(text_entry *t, char *buffer, unsigned int buffer_size);

void iterate_menu(menu *m);

void iterate_text_entry(text_entry *t, char *buffer, unsigned int buffer_size);
#endif

