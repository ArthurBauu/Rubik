#include "visual.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void	test_navigation(void)
{
	t_move_list	mix;
	t_move_list	solution;
	t_move_list	empty;
	t_playback	playback;
	t_cube		expected;

	assert(parse_moves("R U2", &mix, NULL) == 0);
	assert(parse_moves("U2 R'", &solution, NULL) == 0);
	assert(parse_moves("", &empty, NULL) == 0);
	assert(playback_init(&playback, &mix, &solution, 1) == 0);
	cube_init(&expected);
	assert(playback_update(&playback, 300) == 0);
	assert(cube_apply_move(&expected, mix.items[0]) == 0);
	assert(playback_update(&playback, 100) == 0);
	assert(playback_previous(&playback) == 0);
	assert(playback.paused && playback.index == 1 && playback.elapsed_ms == 0);
	assert(memcmp(&playback.cube, &expected, sizeof(expected)) == 0);
	playback_step(&playback);
	assert(playback_update(&playback, 100) == 0);
	assert(playback_reset(&playback) == 0);
	assert(playback.paused && !playback.stepping && playback.phase == 0);
	assert(playback.index == 0 && playback.elapsed_ms == 0);
	assert(cube_is_solved(&playback.cube));
	assert(playback_previous(&playback) == 0);
	assert(playback.index == 0 && cube_is_solved(&playback.cube));
	playback_toggle_pause(&playback);
	assert(playback_update(&playback, 300) == 0);
	assert(playback_update(&playback, 600) == 0);
	assert(playback_current_move(&playback) == &solution.items[0]);
	assert(playback_previous(&playback) == 0);
	assert(playback.phase == 0 && playback.index == 1 && playback.paused);
	assert(memcmp(&playback.cube, &expected, sizeof(expected)) == 0);
	playback_toggle_pause(&playback);
	assert(playback_update(&playback, 600) == 0);
	assert(playback.phase == 1 && playback.paused);
	playback_toggle_pause(&playback);
	assert(playback_update(&playback, 600) == 0);
	assert(playback_update(&playback, 300) == 0);
	assert(cube_is_solved(&playback.cube));
	assert(playback_previous(&playback) == 0);
	assert(playback.phase == 1 && playback.index == 1 && playback.paused);
	assert(memcmp(&playback.cube, &expected, sizeof(expected)) == 0);
	assert(playback_previous(&playback) == 0);
	assert(playback.phase == 1 && playback.index == 0);
	assert(cube_apply_move(&expected, mix.items[1]) == 0);
	assert(memcmp(&playback.cube, &expected, sizeof(expected)) == 0);
	assert(playback_reset(&playback) == 0);
	assert(playback.paused && cube_is_solved(&playback.cube));
	playback_toggle_pause(&playback);
	assert(playback_reset(&playback) == 0);
	assert(!playback.paused && cube_is_solved(&playback.cube));
	assert(playback_init(&playback, &empty, &empty, 1) == 0);
	assert(playback_current_move(&playback) == NULL);
	assert(playback_previous(&playback) == 0);
	assert(playback.paused && cube_is_solved(&playback.cube));
	free_move_list(&empty);
	free_move_list(&solution);
	free_move_list(&mix);
}

static void	test_solution_only(void)
{
	t_move_list	mix;
	t_move_list	solution;
	t_playback	playback;
	t_cube		mixed;

	assert(parse_moves("R U2", &mix, NULL) == 0);
	assert(parse_moves("U2 R'", &solution, NULL) == 0);
	cube_init(&mixed);
	assert(cube_apply_moves(&mixed, &mix) == 0);
	assert(playback_init(&playback, &mix, &solution, 0) == 0);
	assert(playback.phase == 1 && playback.index == 0 && !playback.paused);
	assert(memcmp(&playback.cube, &mixed, sizeof(mixed)) == 0);
	assert(playback_previous(&playback) == 0);
	assert(playback.phase == 1 && playback.index == 0 && playback.paused);
	assert(memcmp(&playback.cube, &mixed, sizeof(mixed)) == 0);
	playback_step(&playback);
	assert(playback_update(&playback, 600) == 0);
	assert(playback.index == 1 && playback.paused);
	assert(playback_reset(&playback) == 0);
	assert(playback.phase == 1 && playback.index == 0 && playback.paused);
	assert(memcmp(&playback.cube, &mixed, sizeof(mixed)) == 0);
	playback_toggle_pause(&playback);
	assert(playback_update(&playback, 600) == 0);
	assert(playback_update(&playback, 300) == 0);
	assert(playback_current_move(&playback) == NULL);
	assert(cube_is_solved(&playback.cube));
	free_move_list(&solution);
	free_move_list(&mix);
}

int	main(void)
{
	t_move_list	mix;
	t_move_list	solution;
	t_playback	playback;
	t_cube		expected;

	assert(parse_moves("R U2", &mix, NULL) == 0);
	assert(parse_moves("U2 R'", &solution, NULL) == 0);
	assert(playback_init(&playback, &mix, &solution, 1) == 0);
	cube_init(&expected);
	playback_toggle_pause(&playback);
	assert(playback_update(&playback, 300) == 0);
	assert(memcmp(&playback.cube, &expected, sizeof(expected)) == 0);
	playback_step(&playback);
	assert(playback_update(&playback, 150) == 0);
	assert(playback.elapsed_ms == 150);
	assert(memcmp(&playback.cube, &expected, sizeof(expected)) == 0);
	assert(playback_update(&playback, 150) == 0);
	assert(cube_apply_move(&expected, mix.items[0]) == 0);
	assert(memcmp(&playback.cube, &expected, sizeof(expected)) == 0);
	assert(playback_update(&playback, 600) == 0);
	assert(playback.index == 1);
	playback_step(&playback);
	assert(playback_update(&playback, 600) == 0);
	assert(cube_apply_move(&expected, mix.items[1]) == 0);
	assert(memcmp(&playback.cube, &expected, sizeof(expected)) == 0);
	assert(playback_current_move(&playback) == &solution.items[0]);
	assert(playback.phase == 1 && playback.index == 0 && playback.paused);
	assert(playback_update(&playback, 600) == 0);
	assert(playback.index == 0 && playback.paused);
	playback_toggle_pause(&playback);
	assert(playback_update(&playback, 600) == 0);
	assert(playback_update(&playback, 300) == 0);
	assert(playback_current_move(&playback) == NULL);
	assert(cube_is_solved(&playback.cube));
	free_move_list(&solution);
	free_move_list(&mix);
	test_navigation();
	test_solution_only();
	puts("playback tests: ok");
	return (0);
}
