#pragma once

bool load_passwords(void);
bool save_passwords(void);
void add_password(const char *passwd);
void free_passwords(void);
int get_password_count(void);
buffer *get_password(int num);
