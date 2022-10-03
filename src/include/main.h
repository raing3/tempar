#ifndef MAIN_H
#define MAIN_H

void button_callback(int curr_but, int last_but, void *arg);
void gamePause(SceUID thread_id);
void gameResume(SceUID thread_id);
int module_stop(int argc, char *argv[]);

#endif