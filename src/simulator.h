#ifndef PIXELS_SIMULATOR_H
#define PIXELS_SIMULATOR_H

#include "AppContext.h"
#include "partition_table.h"
#include "physics.h"

void process_input(AppContext *app, sim::cell_matrix_t &write_buf);

int physics_thread_start(void *data);

inline void process_physics(AppContext *app, const int colour, physics::rng &rngs, sim::cell_matrix_t &write_buf);

void process_rendering(AppContext *app);

#endif // PIXELS_SIMULATOR_H
