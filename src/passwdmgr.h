#pragma once

bool load_passwords(void);
bool save_passwords();
void add_password();
void free_passwords();
int get_password_count(void);
buffer *get_password(int num);
