#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curses.h>
#include "menu.h"

menu create_menu(char *title, char **items, unsigned int num_items, unsigned char color, unsigned char select_color){
	menu output;

	output.title = title;
	output.items = items;
	output.num_items = num_items;
	output.current_choice = 0;
	output.win = newwin(5, COLS/2, (LINES - 6)/2, COLS/4);
	output.width = COLS/2;
	output.color = color;
	output.select_color = select_color;
	output.selected = 0;
	keypad(output.win, 1);
	wbkgd(output.win, COLOR_PAIR(color));

	return output;
}

text_entry create_text_entry(char *title, unsigned char color, unsigned char select_color){
	text_entry output;

	output.title = title;
	output.win = newwin(5, COLS/2, (LINES - 6)/2, COLS/4);
	output.width = COLS/2;
	output.color = color;
	output.select_color = select_color;
	output.done = 0;
	output.cursor_pos = 0;
	wbkgd(output.win, COLOR_PAIR(color));

	return output;
}

void free_menu(menu m){
	delwin(m.win);
}

void free_text_entry(text_entry t){
	delwin(t.win);
}

void render_menu(menu m){
	unsigned int i;

	wmove(m.win, 0, 0);
	wattron(m.win, COLOR_PAIR(m.color));
	wprintw(m.win, "%s\n", m.title);
	for(i = 0; i < m.width; i++){
		wprintw(m.win, "_");
	}
	if(m.current_choice > 0){
		wprintw(m.win, "%s\n", m.items[m.current_choice - 1]);
	} else {
		wprintw(m.win, "\n");
	}
	wattron(m.win, COLOR_PAIR(m.select_color));
	wprintw(m.win, "%s\n", m.items[m.current_choice]);
	wattron(m.win, COLOR_PAIR(m.color));
	if(m.current_choice != m.num_items - 1){
		wprintw(m.win, "%s\n", m.items[m.current_choice + 1]);
	} else {
		wprintw(m.win, "\n");
	}
}

void render_text_entry(text_entry t, char *buffer){
	unsigned int i;

	wmove(t.win, 0, 0);
	wattron(t.win, COLOR_PAIR(t.color));
	wprintw(t.win, "%s\n", t.title);
	for(i = 0; i < t.width; i++){
		wprintw(t.win, "_");
	}
	wattron(t.win, COLOR_PAIR(t.select_color));
	wprintw(t.win, "\n%s\n\n", buffer);
	wattron(t.win, COLOR_PAIR(t.color));
	wrefresh(t.win);
}

void do_menu(menu *m){
	nodelay(m->win, 0);
	while(1){
		render_menu(*m);
		wrefresh(m->win);
		switch(wgetch(m->win)){
			case KEY_UP:
				if(m->current_choice > 0){
					m->current_choice--;
				}
				break;
			case KEY_DOWN:
				if(m->current_choice < m->num_items - 1){
					m->current_choice++;
				}
				break;
			case '\n':
				return;
		}
	}
}

void do_text_entry(text_entry *t, char *buffer, unsigned int buffer_size){
	int key;

	nodelay(t->win, 0);
	buffer[0] = '\0';
	while(1){
		render_text_entry(*t, buffer);
		key = wgetch(t->win);
		if((key == 8 || key == 127) && t->cursor_pos){
			t->cursor_pos--;
			buffer[t->cursor_pos] = '\0';
		} else if(key == '\n'){
			t->done = 1;
			return;
		} else if(key >= ' ' && key < 127 && t->cursor_pos + 1 < buffer_size){
			buffer[t->cursor_pos] = key;
			t->cursor_pos++;
			buffer[t->cursor_pos] = '\0';
		}
	}
}

void iterate_menu(menu *m){
	render_menu(*m);
	nodelay(m->win, 1);
	switch(wgetch(m->win)){
		case KEY_UP:
			if(m->current_choice > 0){
				m->current_choice--;
			}
			break;
		case KEY_DOWN:
			if(m->current_choice < m->num_items - 1){
				m->current_choice++;
			}
			break;
		case '\n':
			m->selected = 1;
	}
}

void iterate_text_entry(text_entry *t, char *buffer, unsigned int buffer_size){
	int key;

	werase(t->win);
	render_text_entry(*t, buffer);
	nodelay(t->win, 1);
	key = wgetch(t->win);
	if((key == 8 || key == 127) && t->cursor_pos){
		t->cursor_pos--;
		buffer[t->cursor_pos] = 0;
	} else if(key == '\n'){
		t->done = 1;
	} else if(key >= ' ' && key < 127 && t->cursor_pos + 1 < buffer_size){
		buffer[t->cursor_pos] = key;
		t->cursor_pos++;
		buffer[t->cursor_pos] = 0;
	}
}

