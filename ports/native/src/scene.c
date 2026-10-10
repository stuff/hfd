#include "scene.h"

const int FADE_IN[8] = {7, 6, 5, 4, 3, 2, 1, 0};
const int FADE_OUT[8] = {0, 1, 2, 3, 4, 5, 6, 7};

int fade_at(int t, const int *steps, int count, int before) {
  if (t < 0) return before;
  int i = t / 3;
  return steps[i < count - 1 ? i : count - 1];
}

void app_go(App *app, Scene *next) {
  Scene *old = app->scene;
  app->scene = next;
  if (old && old != next && old != app->text_page && app->dead_count < 8) app->dead[app->dead_count++] = old;
}

void app_reap(App *app) {
  for (int i = 0; i < app->dead_count; i++)
    if (app->dead[i] != app->scene && app->dead[i] != app->text_page) app->dead[i]->free(app->dead[i]);
  app->dead_count = 0;
}
