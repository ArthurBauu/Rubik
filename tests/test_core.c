#define _POSIX_C_SOURCE 200809L

#include "rubik.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

static void	test_parser(void)
{
	t_move_list	moves;
	size_t		error_offset;

	assert(parse_moves(" F  R U2 B' L’ D' ", &moves, &error_offset) == 0);
	assert(moves.length == 6);
	assert(moves.items[0].face == FACE_F && moves.items[0].turns == 1);
	assert(moves.items[1].face == FACE_R && moves.items[1].turns == 1);
	assert(moves.items[2].face == FACE_U && moves.items[2].turns == 2);
	assert(moves.items[3].face == FACE_B && moves.items[3].turns == 3);
	assert(moves.items[4].face == FACE_L && moves.items[4].turns == 3);
	assert(moves.items[5].face == FACE_D && moves.items[5].turns == 3);
	free_move_list(&moves);
	assert(parse_moves("F M", &moves, &error_offset) == -1);
	assert(errno == EINVAL && error_offset == 2);
	assert(moves.items == NULL && moves.length == 0);
	assert(parse_moves("FR", &moves, &error_offset) == -1);
	assert(errno == EINVAL && error_offset == 1);
	assert(parse_moves("F2'", &moves, &error_offset) == -1);
}

static void	test_cube_moves(void)
{
	t_cube	cube;
	t_cube	solved;
	t_move	move;
	t_face	face;

	cube_init(&solved);
	assert(cube_is_solved(&solved));
	face = FACE_U;
	while (face <= FACE_B)
	{
		cube = solved;
		move.face = face;
		move.turns = 1;
		assert(cube_apply_move(&cube, move) == 0);
		assert(!cube_is_solved(&cube));
		move.turns = 3;
		assert(cube_apply_move(&cube, move) == 0);
		assert(cube_is_solved(&cube));
		cube = solved;
		move.turns = 1;
		assert(cube_apply_move(&cube, move) == 0);
		assert(cube_apply_move(&cube, move) == 0);
		assert(cube_apply_move(&cube, move) == 0);
		assert(cube_apply_move(&cube, move) == 0);
		assert(cube_is_solved(&cube));
		face++;
	}
}

static void	test_sequence_inverse(void)
{
	t_cube		cube;
	t_move_list	moves;
	t_move		inverse;
	size_t		index;

	assert(parse_moves("F R U2 B' L' D'", &moves, NULL) == 0);
	cube_init(&cube);
	assert(cube_apply_moves(&cube, &moves) == 0);
	index = moves.length;
	while (index > 0)
	{
		inverse = moves.items[index - 1];
		if (inverse.turns != 2)
			inverse.turns = 4 - inverse.turns;
		assert(cube_apply_move(&cube, inverse) == 0);
		index--;
	}
	assert(cube_is_solved(&cube));
	free_move_list(&moves);
}

static void	assert_cubies_equal(const t_cubie_cube *left,
		const t_cubie_cube *right)
{
	size_t	index;

	index = 0;
	while (index < 8)
	{
		assert(left->corners[index] == right->corners[index]);
		assert(left->corner_orientation[index]
			== right->corner_orientation[index]);
		index++;
	}
	index = 0;
	while (index < 12)
	{
		assert(left->edges[index] == right->edges[index]);
		assert(left->edge_orientation[index]
			== right->edge_orientation[index]);
		index++;
	}
}

static void	test_cubie_conversion_and_moves(void)
{
	t_cube			facelets;
	t_cubie_cube	cubies;
	t_cubie_cube	converted;
	t_move			move;
	t_move_list		moves;
	t_face			face;
	unsigned char	turns;
	size_t			index;

	cube_init(&facelets);
	assert(cube_to_cubie(&facelets, &cubies) == 0);
	assert(cubie_is_solved(&cubies));
	face = FACE_U;
	while (face <= FACE_B)
	{
		cube_init(&facelets);
		move.face = face;
		move.turns = 1;
		assert(cube_apply_move(&facelets, move) == 0);
		assert(cube_to_cubie(&facelets, &converted) == 0);
		face++;
	}
	face = FACE_U;
	while (face <= FACE_B)
	{
		turns = 1;
		while (turns <= 3)
		{
			cube_init(&facelets);
			assert(cube_to_cubie(&facelets, &cubies) == 0);
			move.face = face;
			move.turns = turns;
			assert(cube_apply_move(&facelets, move) == 0);
			assert(cubie_apply_move(&cubies, move) == 0);
			assert(cube_to_cubie(&facelets, &converted) == 0);
			assert_cubies_equal(&cubies, &converted);
			turns++;
		}
		face++;
	}
	assert(parse_moves("F R U2 B' L' D'", &moves, NULL) == 0);
	cube_init(&facelets);
	assert(cube_to_cubie(&facelets, &cubies) == 0);
	index = 0;
	while (index < moves.length)
	{
		assert(cube_apply_move(&facelets, moves.items[index]) == 0);
		assert(cubie_apply_move(&cubies, moves.items[index]) == 0);
		assert(cube_to_cubie(&facelets, &converted) == 0);
		assert_cubies_equal(&cubies, &converted);
		index++;
	}
	free_move_list(&moves);
	facelets.stickers[0] = '?';
	assert(cube_to_cubie(&facelets, &converted) == -1);
}

static void	test_solver(void)
{
	static const char	*scrambles[] = {
		"F R U2 B' L' D' R2 U F' D2",
		"R2 D' B' D F2 R F2 R2 U L' F2 U' B' L2 R D B' R' B2 L2 F2 L2 R2 U2 D2",
		"U F2 D' L B2 R' U2 F D2 B' L2 R U' F2 D R2 B U2"
	};
	t_cube		cube;
	t_move_list	mix;
	t_move_list	solution;
	size_t		index;
	size_t		move_index;

	cube_init(&cube);
	assert(solve_cube(&cube, &solution) == 0);
	assert(solution.length == 0);
	free_move_list(&solution);
	index = 0;
	while (index < sizeof(scrambles) / sizeof(scrambles[0]))
	{
		assert(parse_moves(scrambles[index], &mix, NULL) == 0);
		cube_init(&cube);
		assert(cube_apply_moves(&cube, &mix) == 0);
		assert(solve_cube(&cube, &solution) == 0);
		assert(solution.length <= 30);
		move_index = 1;
		while (move_index < solution.length)
		{
			assert(solution.items[move_index - 1].face
				!= solution.items[move_index].face);
			move_index++;
		}
		assert(cube_apply_moves(&cube, &solution) == 0);
		assert(cube_is_solved(&cube));
		free_move_list(&solution);
		free_move_list(&mix);
		index++;
	}
}

static void	test_alternative_solvers(void)
{
	t_cube		cube;
	t_cubie_cube	cubies;
	t_move_list	mix;
	t_move_list	solution;

	assert(parse_moves("R U R' U R U2 R'", &mix, NULL) == 0);
	cube_init(&cube);
	assert(cube_apply_moves(&cube, &mix) == 0);
	assert(solve_cube_timed(&cube, &solution, 25) == 0);
	assert(cube_apply_moves(&cube, &solution) == 0);
	assert(cube_is_solved(&cube));
	free_move_list(&solution);
	free_move_list(&mix);
	assert(parse_moves("R U R' U R U2 R' F2", &mix, NULL) == 0);
	cube_init(&cube);
	assert(cube_apply_moves(&cube, &mix) == 0);
	assert(solve_cube_2x2(&cube, &solution) == 0);
	assert(cube_apply_moves(&cube, &solution) == 0);
	assert(cube_to_cubie(&cube, &cubies) == 0);
	assert(cubies.corners[0] == 0 && cubies.corners[1] == 1
		&& cubies.corners[2] == 2 && cubies.corners[3] == 3
		&& cubies.corners[4] == 4 && cubies.corners[5] == 5
		&& cubies.corners[6] == 6 && cubies.corners[7] == 7);
	assert(cubies.corner_orientation[0] == 0
		&& cubies.corner_orientation[1] == 0
		&& cubies.corner_orientation[2] == 0
		&& cubies.corner_orientation[3] == 0
		&& cubies.corner_orientation[4] == 0
		&& cubies.corner_orientation[5] == 0
		&& cubies.corner_orientation[6] == 0
		&& cubies.corner_orientation[7] == 0);
	free_move_list(&solution);
	free_move_list(&mix);
}

static void	test_timed_search_budget(void)
{
	t_cube			cube;
	t_move_list		mix;
	t_move_list		solution;
	t_move_list		standard;
	struct timespec	start;
	struct timespec	end;
	double			elapsed;

	assert(parse_moves("D2 F2 L2 U' R2 D2 F2 R' F' U' L' U' D U2 D2 "
			"L' R' B U' L2 B2 U' B L2 D", &mix, NULL) == 0);
	cube_init(&cube);
	assert(cube_apply_moves(&cube, &mix) == 0);
	assert(solve_cube(&cube, &standard) == 0);
	assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
	assert(solve_cube_timed(&cube, &solution, 100) == 0);
	assert(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
	elapsed = (double)(end.tv_sec - start.tv_sec)
		+ (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
	assert(elapsed >= 0.18);
	assert(solution.length <= standard.length);
	assert(cube_apply_moves(&cube, &solution) == 0);
	assert(cube_is_solved(&cube));
	free_move_list(&solution);
	free_move_list(&standard);
	free_move_list(&mix);
}

static unsigned int	next_random(unsigned int *state)
{
	*state = *state * 1664525U + 1013904223U;
	return (*state);
}

static void	test_solver_random_states(void)
{
	t_cube			cube;
	t_move			mix_moves[25];
	t_move_list		mix;
	t_move_list		solution;
	unsigned int	random_state;
	unsigned int	value;
	size_t			test;
	size_t			index;

	mix.items = mix_moves;
	mix.length = 25;
	random_state = 0x42C0FFEEU;
	test = 0;
	while (test < 20)
	{
		index = 0;
		while (index < mix.length)
		{
			value = next_random(&random_state);
			mix_moves[index].face = (t_face)(value % 6);
			if (index != 0 && mix_moves[index].face == mix_moves[index - 1].face)
				mix_moves[index].face
					= (t_face)((mix_moves[index].face + 1) % 6);
			mix_moves[index].turns
				= (unsigned char)(next_random(&random_state) % 3 + 1);
			index++;
		}
		cube_init(&cube);
		assert(cube_apply_moves(&cube, &mix) == 0);
		assert(solve_cube(&cube, &solution) == 0);
		assert(solution.length <= 30);
		index = 1;
		while (index < solution.length)
		{
			assert(solution.items[index - 1].face != solution.items[index].face);
			index++;
		}
		assert(cube_apply_moves(&cube, &solution) == 0);
		assert(cube_is_solved(&cube));
		free_move_list(&solution);
		test++;
	}
}

static void	test_generated_scrambles(void)
{
	t_cube		cube;
	t_move_list	scramble;
	t_move_list	solution;
	size_t		test;
	size_t		index;

	test = 0;
	while (test < 5)
	{
		assert(generate_scramble(25, &scramble) == 0);
		assert(scramble.length == 25);
		index = 1;
		while (index < scramble.length)
		{
			assert(scramble.items[index - 1].face != scramble.items[index].face);
			index++;
		}
		cube_init(&cube);
		assert(cube_apply_moves(&cube, &scramble) == 0);
		assert(solve_cube(&cube, &solution) == 0);
		assert(cube_apply_moves(&cube, &solution) == 0);
		assert(cube_is_solved(&cube));
		free_move_list(&solution);
		free_move_list(&scramble);
		test++;
	}
}

int	main(void)
{
	test_parser();
	test_cube_moves();
	test_sequence_inverse();
	test_cubie_conversion_and_moves();
	test_solver();
	test_alternative_solvers();
	test_timed_search_budget();
	test_solver_random_states();
	test_generated_scrambles();
	puts("core tests: ok");
	return (0);
}
