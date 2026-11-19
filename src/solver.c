#define _POSIX_C_SOURCE 200809L

#include "rubik.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TWIST_COUNT 2187
#define FLIP_COUNT 2048
#define SLICE_COUNT 495
#define CORNER_PERM_COUNT 40320
#define EDGE_PERM_COUNT 40320
#define SLICE_PERM_COUNT 24
#define PHASE1_MOVE_COUNT 18
#define PHASE2_MOVE_COUNT 10
#define PHASE1_MAX_DEPTH 12
#define PHASE2_MAX_DEPTH 18
#define MAX_SOLUTION_LENGTH (PHASE1_MAX_DEPTH + PHASE2_MAX_DEPTH)
#define TIMED_PHASE1_MAX_DEPTH 20
#define PHASE1_CANDIDATE_LIMIT 16384
#define PHASE1_SHORT_CANDIDATES 4096
#define TWO_MAX_DEPTH 14

typedef struct s_cubie_move
{
	unsigned char	corners[8];
	unsigned char	corner_orientation[8];
	unsigned char	edges[12];
	unsigned char	edge_orientation[12];
}	t_cubie_move;
typedef struct s_search
{
	int		path[MAX_SOLUTION_LENGTH];
	int		answer[MAX_SOLUTION_LENGTH];
	size_t	answer_length;
	size_t	answer_metric;
}	t_search;

typedef struct s_phase1_candidate
{
	t_cubie_cube	cube;
	int				path[TIMED_PHASE1_MAX_DEPTH];
	size_t			length;
} t_phase1_candidate;

typedef struct s_timed_search
{
	struct timespec	deadline;
	unsigned long	nodes;
	int				timed_out;
} t_timed_search;
static const int	g_phase2_move_ids[PHASE2_MOVE_COUNT] = {
	0, 1, 2, 4, 7, 9, 10, 11, 13, 16
};
static t_cubie_move	g_moves[PHASE1_MOVE_COUNT];
static uint16_t	g_twist_moves[TWIST_COUNT][PHASE1_MOVE_COUNT];
static uint16_t	g_flip_moves[FLIP_COUNT][PHASE1_MOVE_COUNT];
static uint16_t	g_slice_moves[SLICE_COUNT][PHASE1_MOVE_COUNT];
static uint16_t	g_corner_perm_moves[CORNER_PERM_COUNT][PHASE2_MOVE_COUNT];
static uint16_t	g_edge_perm_moves[EDGE_PERM_COUNT][PHASE2_MOVE_COUNT];
static uint16_t	g_slice_perm_moves[SLICE_PERM_COUNT][PHASE2_MOVE_COUNT];
static uint8_t	g_twist_slice_pruning[TWIST_COUNT * SLICE_COUNT];
static uint8_t	g_flip_slice_pruning[FLIP_COUNT * SLICE_COUNT];
static uint8_t	g_corner_slice_pruning[CORNER_PERM_COUNT * SLICE_PERM_COUNT];
static uint8_t	g_edge_slice_pruning[EDGE_PERM_COUNT * SLICE_PERM_COUNT];
static uint16_t	g_slice_coordinate[4096];
static uint16_t	g_slice_masks[SLICE_COUNT];
static uint16_t	g_solved_slice_coordinate;
static uint16_t	g_two_twist_moves[TWIST_COUNT][PHASE1_MOVE_COUNT];
static uint16_t	g_two_corner_moves[CORNER_PERM_COUNT][PHASE1_MOVE_COUNT];
static uint8_t	g_two_twist_distance[TWIST_COUNT];
static uint8_t	g_two_corner_distance[CORNER_PERM_COUNT];
static int		g_two_tables_state;
static int		g_tables_state;

static void	init_solved_cubies(t_cubie_cube *cube)
{
	size_t	position;

	position = 0;
	while (position < 8)
	{
		cube->corners[position] = (unsigned char)position;
		cube->corner_orientation[position] = 0;
		position++;
	}
	position = 0;
	while (position < 12)
	{
		cube->edges[position] = (unsigned char)position;
		cube->edge_orientation[position] = 0;
		position++;
	}
}

static unsigned int	factorial(unsigned int value)
{
	unsigned int	result;

	result = 1;
	while (value > 1)
		result *= value--;
	return (result);
}

static unsigned int	rank_permutation(const unsigned char *permutation,
		unsigned int length)
{
	unsigned int	rank;
	unsigned int	smaller;
	unsigned int	left;
	unsigned int	right;

	rank = 0;
	left = 0;
	while (left < length)
	{
		smaller = 0;
		right = left + 1;
		while (right < length)
		{
			if (permutation[right] < permutation[left])
				smaller++;
			right++;
		}
		rank = rank * (length - left) + smaller;
		left++;
	}
	return (rank);
}

static void	unrank_permutation(unsigned int rank, unsigned int length,
		unsigned char *permutation)
{
	unsigned char	available[12];
	unsigned int	digits[12];
	unsigned int	index;
	unsigned int	factor;
	unsigned int	choice;
	unsigned int	remaining;
	unsigned int	cursor;

	index = 0;
	while (index < length)
	{
		available[index] = (unsigned char)index;
		index++;
	}
	index = 0;
	while (index < length)
	{
		factor = factorial(length - index - 1);
		digits[index] = rank / factor;
		rank %= factor;
		index++;
	}
	remaining = length;
	index = 0;
	while (index < length)
	{
		choice = digits[index];
		permutation[index] = available[choice];
		cursor = choice;
		while (cursor + 1 < remaining)
		{
			available[cursor] = available[cursor + 1];
			cursor++;
		}
		remaining--;
		index++;
	}
}

static unsigned int	encode_twist(const t_cubie_cube *cube)
{
	unsigned int	coordinate;
	size_t			index;

	coordinate = 0;
	index = 0;
	while (index < 7)
		coordinate = coordinate * 3 + cube->corner_orientation[index++];
	return (coordinate);
}

static void	decode_twist(unsigned int coordinate, t_cubie_cube *cube)
{
	unsigned int	sum;
	size_t			index;

	sum = 0;
	index = 7;
	while (index > 0)
	{
		index--;
		cube->corner_orientation[index] = (unsigned char)(coordinate % 3);
		sum += cube->corner_orientation[index];
		coordinate /= 3;
	}
	cube->corner_orientation[7] = (unsigned char)((3 - sum % 3) % 3);
}

static unsigned int	encode_flip(const t_cubie_cube *cube)
{
	unsigned int	coordinate;
	size_t			index;

	coordinate = 0;
	index = 0;
	while (index < 11)
		coordinate = coordinate * 2 + cube->edge_orientation[index++];
	return (coordinate);
}

static void	decode_flip(unsigned int coordinate, t_cubie_cube *cube)
{
	unsigned int	sum;
	size_t			index;

	sum = 0;
	index = 11;
	while (index > 0)
	{
		index--;
		cube->edge_orientation[index] = (unsigned char)(coordinate % 2);
		sum += cube->edge_orientation[index];
		coordinate /= 2;
	}
	cube->edge_orientation[11] = (unsigned char)(sum % 2);
}

static void	build_slice_coordinates(void)
{
	unsigned int	mask;
	unsigned int	bits;
	unsigned int	position;
	size_t			count;

	memset(g_slice_coordinate, 0xFF, sizeof(g_slice_coordinate));
	count = 0;
	mask = 0;
	while (mask < 4096)
	{
		bits = 0;
		position = 0;
		while (position < 12)
			bits += (mask >> position++) & 1U;
		if (bits == 4)
		{
			g_slice_masks[count] = (uint16_t)mask;
			g_slice_coordinate[mask] = (uint16_t)count;
			count++;
		}
		mask++;
	}
	g_solved_slice_coordinate = g_slice_coordinate[0xF00];
}

static int	apply_move_id(t_cubie_cube *cube, int move_id)
{
	t_move	move;

	move.face = (t_face)(move_id / 3);
	move.turns = (unsigned char)(move_id % 3 + 1);
	return (cubie_apply_move(cube, move));
}

static int	build_cubie_moves(void)
{
	t_cubie_cube	state;
	size_t			move_id;
	size_t			index;

	move_id = 0;
	while (move_id < PHASE1_MOVE_COUNT)
	{
		init_solved_cubies(&state);
		if (apply_move_id(&state, (int)move_id) != 0)
			return (-1);
		index = 0;
		while (index < 8)
		{
			g_moves[move_id].corners[index] = state.corners[index];
			g_moves[move_id].corner_orientation[index]
				= state.corner_orientation[index];
			index++;
		}
		index = 0;
		while (index < 12)
		{
			g_moves[move_id].edges[index] = state.edges[index];
			g_moves[move_id].edge_orientation[index]
				= state.edge_orientation[index];
			index++;
		}
		move_id++;
	}
	return (0);
}

static int	build_phase1_move_tables(void)
{
	t_cubie_cube	state;
	t_cubie_cube	next;
	unsigned int	coordinate;
	unsigned int	mask;
	unsigned int	next_mask;
	size_t			move_id;
	size_t			position;

	coordinate = 0;
	while (coordinate < TWIST_COUNT)
	{
		init_solved_cubies(&state);
		decode_twist(coordinate, &state);
		move_id = 0;
		while (move_id < PHASE1_MOVE_COUNT)
		{
			next = state;
			if (apply_move_id(&next, (int)move_id) != 0)
				return (-1);
			g_twist_moves[coordinate][move_id] = (uint16_t)encode_twist(&next);
			move_id++;
		}
		coordinate++;
	}
	coordinate = 0;
	while (coordinate < FLIP_COUNT)
	{
		init_solved_cubies(&state);
		decode_flip(coordinate, &state);
		move_id = 0;
		while (move_id < PHASE1_MOVE_COUNT)
		{
			next = state;
			if (apply_move_id(&next, (int)move_id) != 0)
				return (-1);
			g_flip_moves[coordinate][move_id] = (uint16_t)encode_flip(&next);
			move_id++;
		}
		coordinate++;
	}
	coordinate = 0;
	while (coordinate < SLICE_COUNT)
	{
		mask = g_slice_masks[coordinate];
		move_id = 0;
		while (move_id < PHASE1_MOVE_COUNT)
		{
			next_mask = 0;
			position = 0;
			while (position < 12)
			{
				if ((mask & (1U << g_moves[move_id].edges[position])) != 0)
					next_mask |= 1U << position;
				position++;
			}
			g_slice_moves[coordinate][move_id]
				= g_slice_coordinate[next_mask];
			move_id++;
		}
		coordinate++;
	}
	return (0);
}

static void	init_permutation_state(t_cubie_cube *state,
		const unsigned char *permutation, size_t length, int corners)
{
	size_t	index;

	init_solved_cubies(state);
	index = 0;
	while (index < length)
	{
		if (corners)
			state->corners[index] = permutation[index];
		else
			state->edges[index] = permutation[index];
		index++;
	}
}

static int	build_phase2_move_tables(void)
{
	t_cubie_cube	state;
	t_cubie_cube	next;
	unsigned char	permutation[12];
	unsigned int	coordinate;
	size_t			move_index;
	size_t			position;
	int				move_id;

	coordinate = 0;
	while (coordinate < CORNER_PERM_COUNT)
	{
		unrank_permutation(coordinate, 8, permutation);
		init_permutation_state(&state, permutation, 8, 1);
		move_index = 0;
		while (move_index < PHASE2_MOVE_COUNT)
		{
			next = state;
			move_id = g_phase2_move_ids[move_index];
			if (apply_move_id(&next, move_id) != 0)
				return (-1);
			g_corner_perm_moves[coordinate][move_index]
				= (uint16_t)rank_permutation(next.corners, 8);
			move_index++;
		}
		coordinate++;
	}
	coordinate = 0;
	while (coordinate < EDGE_PERM_COUNT)
	{
		unrank_permutation(coordinate, 8, permutation);
		init_permutation_state(&state, permutation, 8, 0);
		move_index = 0;
		while (move_index < PHASE2_MOVE_COUNT)
		{
			next = state;
			move_id = g_phase2_move_ids[move_index];
			if (apply_move_id(&next, move_id) != 0)
				return (-1);
			g_edge_perm_moves[coordinate][move_index]
				= (uint16_t)rank_permutation(next.edges, 8);
			move_index++;
		}
		coordinate++;
	}
	coordinate = 0;
	while (coordinate < SLICE_PERM_COUNT)
	{
		unrank_permutation(coordinate, 4, permutation);
		init_solved_cubies(&state);
		position = 0;
		while (position < 4)
		{
			state.edges[position + 8] = (unsigned char)(permutation[position] + 8);
			position++;
		}
		move_index = 0;
		while (move_index < PHASE2_MOVE_COUNT)
		{
			next = state;
			move_id = g_phase2_move_ids[move_index];
			if (apply_move_id(&next, move_id) != 0)
				return (-1);
			position = 0;
			while (position < 4)
			{
				permutation[position] = (unsigned char)(next.edges[position + 8] - 8);
				position++;
			}
			g_slice_perm_moves[coordinate][move_index]
				= (uint16_t)rank_permutation(permutation, 4);
			move_index++;
		}
		coordinate++;
	}
	return (0);
}

static int	build_pair_pruning(uint8_t *pruning, unsigned int count_a,
		unsigned int count_b, unsigned int move_count,
		const uint16_t *moves_a, const uint16_t *moves_b,
		unsigned int goal_a, unsigned int goal_b)
{
	uint32_t	*queue;
	uint32_t	head;
	uint32_t	tail;
	uint32_t	current;
	uint32_t	next;
	unsigned int	coordinate_a;
	unsigned int	coordinate_b;
	unsigned int	move;
	size_t			state_count;

	state_count = (size_t)count_a * count_b;
	memset(pruning, 0xFF, state_count);
	queue = malloc(state_count * sizeof(*queue));
	if (queue == NULL)
	{
		errno = ENOMEM;
		return (-1);
	}
	head = 0;
	tail = 0;
	current = goal_a * count_b + goal_b;
	pruning[current] = 0;
	queue[tail++] = current;
	while (head < tail)
	{
		current = queue[head++];
		coordinate_a = current / count_b;
		coordinate_b = current % count_b;
		move = 0;
		while (move < move_count)
		{
			next = moves_a[(size_t)coordinate_a * move_count + move]
				* count_b
				+ moves_b[(size_t)coordinate_b * move_count + move];
			if (pruning[next] == UINT8_MAX)
			{
				pruning[next] = (uint8_t)(pruning[current] + 1);
				queue[tail++] = next;
			}
			move++;
		}
	}
	free(queue);
	if (tail != state_count)
	{
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

static int	build_all_tables(void)
{
	if (g_tables_state == 1)
		return (0);
	if (g_tables_state == -1)
		return (-1);
	errno = 0;
	g_tables_state = -1;
	if (build_cubie_moves() != 0)
		goto table_error;
	build_slice_coordinates();
	if (build_phase1_move_tables() != 0
		|| build_phase2_move_tables() != 0)
		goto table_error;
	if (build_pair_pruning(&g_twist_slice_pruning[0], TWIST_COUNT,
			SLICE_COUNT, PHASE1_MOVE_COUNT, &g_twist_moves[0][0],
			&g_slice_moves[0][0], 0, g_solved_slice_coordinate) != 0
		|| build_pair_pruning(&g_flip_slice_pruning[0], FLIP_COUNT,
			SLICE_COUNT, PHASE1_MOVE_COUNT, &g_flip_moves[0][0],
			&g_slice_moves[0][0], 0, g_solved_slice_coordinate) != 0
		|| build_pair_pruning(&g_corner_slice_pruning[0], CORNER_PERM_COUNT,
			SLICE_PERM_COUNT, PHASE2_MOVE_COUNT,
			&g_corner_perm_moves[0][0], &g_slice_perm_moves[0][0], 0, 0) != 0
		|| build_pair_pruning(&g_edge_slice_pruning[0], EDGE_PERM_COUNT,
			SLICE_PERM_COUNT, PHASE2_MOVE_COUNT,
			&g_edge_perm_moves[0][0], &g_slice_perm_moves[0][0], 0, 0) != 0)
		goto table_error;
	g_tables_state = 1;
	return (0);

table_error:
	if (errno == 0)
		errno = EINVAL;
	return (-1);
}

static unsigned int	slice_coordinate_from_cubies(const t_cubie_cube *cube)
{
	unsigned int	mask;
	size_t			position;

	mask = 0;
	position = 0;
	while (position < 12)
	{
		if (cube->edges[position] >= 8)
			mask |= 1U << position;
		position++;
	}
	return (g_slice_coordinate[mask]);
}

static unsigned int	slice_permutation_from_cubies(const t_cubie_cube *cube)
{
	unsigned char	permutation[4];
	size_t			position;

	position = 0;
	while (position < 4)
	{
		permutation[position] = (unsigned char)(cube->edges[position + 8] - 8);
		position++;
	}
	return (rank_permutation(permutation, 4));
}

static int	phase1_heuristic(unsigned int twist, unsigned int flip,
		unsigned int slice)
{
	unsigned int	first;
	unsigned int	second;

	first = g_twist_slice_pruning[twist * SLICE_COUNT + slice];
	second = g_flip_slice_pruning[flip * SLICE_COUNT + slice];
	if (first > second)
		return ((int)first);
	return ((int)second);
}

static int	phase2_heuristic(unsigned int corners, unsigned int edges,
		unsigned int slice)
{
	unsigned int	first;
	unsigned int	second;

	first = g_corner_slice_pruning[
		corners * SLICE_PERM_COUNT + slice];
	second = g_edge_slice_pruning[edges * SLICE_PERM_COUNT + slice];
	if (first > second)
		return ((int)first);
	return ((int)second);
}

static int	same_axis(int left_face, int right_face)
{
	return (left_face % 3 == right_face % 3);
}

static int	phase2_search(t_search *search, const t_cubie_cube *cube,
		unsigned int corners, unsigned int edges, unsigned int slice,
		int remaining, int phase2_limit, int previous_face,
		size_t phase1_length)
{
	t_cubie_cube	next_cube;
	unsigned int	next_corners;
	unsigned int	next_edges;
	unsigned int	next_slice;
	int				move_index;
	int				move_id;
	int				face;
	size_t			length;

	if (phase2_heuristic(corners, edges, slice) > remaining)
		return (0);
	if (corners == 0 && edges == 0 && slice == 0)
	{
		length = phase1_length + (size_t)(phase2_limit - remaining);
		memcpy(search->answer, search->path, length * sizeof(search->path[0]));
		search->answer_length = length;
		return (1);
	}
	if (remaining == 0)
		return (0);
	move_index = 0;
	while (move_index < PHASE2_MOVE_COUNT)
	{
		move_id = g_phase2_move_ids[move_index];
		face = move_id / 3;
		if (face == previous_face
			|| (previous_face >= 0 && same_axis(face, previous_face)
				&& face < previous_face))
		{
			move_index++;
			continue ;
		}
		next_corners = g_corner_perm_moves[corners][move_index];
		next_edges = g_edge_perm_moves[edges][move_index];
		next_slice = g_slice_perm_moves[slice][move_index];
		if (phase2_heuristic(next_corners, next_edges, next_slice)
			<= remaining - 1)
		{
			next_cube = *cube;
			if (apply_move_id(&next_cube, move_id) == 0)
			{
				search->path[phase1_length
					+ (phase2_limit - remaining)] = move_id;
				if (phase2_search(search, &next_cube, next_corners,
						next_edges, next_slice, remaining - 1, phase2_limit,
						face, phase1_length))
					return (1);
			}
		}
		move_index++;
	}
	return (0);
}

static int	try_phase2(t_search *search, const t_cubie_cube *cube,
		size_t phase1_length)
{
	unsigned int	corners;
	unsigned int	edges;
	unsigned int	slice;
	int				depth;
	int				minimum;

	corners = rank_permutation(cube->corners, 8);
	edges = rank_permutation(cube->edges, 8);
	slice = slice_permutation_from_cubies(cube);
	minimum = phase2_heuristic(corners, edges, slice);
	depth = minimum;
	while (depth <= PHASE2_MAX_DEPTH
		&& phase1_length + (size_t)depth <= MAX_SOLUTION_LENGTH)
	{
		if (phase2_search(search, cube, corners, edges, slice, depth, depth,
				-1, phase1_length))
			return (1);
		depth++;
	}
	return (0);
}

static int	set_deadline(t_timed_search *timed, unsigned int milliseconds)
{
	struct timespec	now;
	uint64_t		nanoseconds;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return (-1);
	nanoseconds = (uint64_t)now.tv_nsec + (uint64_t)milliseconds * 1000000ULL;
	timed->deadline.tv_sec = now.tv_sec + (time_t)(nanoseconds / 1000000000ULL);
	timed->deadline.tv_nsec = (long)(nanoseconds % 1000000000ULL);
	timed->nodes = 0;
	timed->timed_out = 0;
	return (0);
}

static void	check_deadline(t_timed_search *timed)
{
	struct timespec	now;

	if (timed->timed_out || ++timed->nodes % 1024 != 0)
		return ;
	if (clock_gettime(CLOCK_MONOTONIC, &now) == 0
		&& (now.tv_sec > timed->deadline.tv_sec
			|| (now.tv_sec == timed->deadline.tv_sec
				&& now.tv_nsec >= timed->deadline.tv_nsec)))
		timed->timed_out = 1;
}

static int	collect_phase1(t_timed_search *timed, t_phase1_candidate *candidates,
		size_t *candidate_count, size_t *candidate_seen,
		const t_cubie_cube *cube,
		unsigned int twist, unsigned int flip, unsigned int slice,
		int remaining, int previous_face, size_t depth, int *path)
{
	t_cubie_cube	next_cube;
	unsigned int	next_twist;
	unsigned int	next_flip;
	unsigned int	next_slice;
	int			move_id;
	int			face;
	size_t		candidate_slot;
	uint64_t	candidate_hash;
	int			store_candidate;

	check_deadline(timed);
	if (timed->timed_out)
		return (0);
	if (phase1_heuristic(twist, flip, slice) > remaining)
		return (0);
	if (remaining == 0)
	{
		if (twist == 0 && flip == 0 && slice == g_solved_slice_coordinate)
		{
			store_candidate = 1;
			candidate_slot = *candidate_seen;
			if (*candidate_seen >= PHASE1_CANDIDATE_LIMIT)
			{
				candidate_hash = (uint64_t)*candidate_seen * 11400714819323198485ULL
					+ 14029467366897019727ULL;
				candidate_hash ^= candidate_hash >> 29;
				candidate_hash *= 277803737ULL;
				candidate_slot = PHASE1_SHORT_CANDIDATES
					+ (size_t)(candidate_hash % ((uint64_t)*candidate_seen
						- PHASE1_SHORT_CANDIDATES + 1));
				store_candidate = candidate_slot < PHASE1_CANDIDATE_LIMIT;
			}
			if (store_candidate)
			{
				candidates[candidate_slot].cube = *cube;
				memcpy(candidates[candidate_slot].path, path,
					depth * sizeof(path[0]));
				candidates[candidate_slot].length = depth;
				if (*candidate_count < PHASE1_CANDIDATE_LIMIT)
					(*candidate_count)++;
			}
			(*candidate_seen)++;
		}
		return (0);
	}
	move_id = 0;
	while (move_id < PHASE1_MOVE_COUNT)
	{
		face = move_id / 3;
		if (face == previous_face
			|| (previous_face >= 0 && same_axis(face, previous_face)
				&& face < previous_face))
		{
			move_id++;
			continue ;
		}
		next_twist = g_twist_moves[twist][move_id];
		next_flip = g_flip_moves[flip][move_id];
		next_slice = g_slice_moves[slice][move_id];
		if (phase1_heuristic(next_twist, next_flip, next_slice)
			<= remaining - 1)
		{
			next_cube = *cube;
			if (apply_move_id(&next_cube, move_id) == 0)
			{
				path[depth] = move_id;
				collect_phase1(timed, candidates, candidate_count,
					candidate_seen, &next_cube, next_twist, next_flip,
					next_slice, remaining - 1, face, depth + 1, path);
				if (timed->timed_out)
					return (0);
			}
		}
		move_id++;
	}
	return (0);
}

static size_t	move_sequence_metric(const int *path, size_t length)
{
	int			faces[MAX_SOLUTION_LENGTH];
	unsigned int	turns[MAX_SOLUTION_LENGTH];
	size_t		index;
	size_t		metric;

	index = 0;
	metric = 0;
	while (index < length)
	{
		if (metric != 0 && faces[metric - 1] == path[index] / 3)
		{
			turns[metric - 1] = (turns[metric - 1]
				+ (unsigned int)(path[index] % 3 + 1)) % 4;
			if (turns[metric - 1] == 0)
				metric--;
		}
		else
		{
			faces[metric] = path[index] / 3;
			turns[metric] = (unsigned int)(path[index] % 3 + 1);
			metric++;
		}
		index++;
	}
	return (metric);
}

static int	phase2_timed_search(t_timed_search *timed, t_search *search,
		const t_cubie_cube *cube, unsigned int corners, unsigned int edges,
		unsigned int slice, int remaining, int phase2_limit,
		int previous_face, size_t phase1_length)
{
	t_cubie_cube	next_cube;
	unsigned int	next_corners;
	unsigned int	next_edges;
	unsigned int	next_slice;
	int			move_index;
	int			move_id;
	int			face;
	size_t			length;

	check_deadline(timed);
	if (timed->timed_out
		|| phase2_heuristic(corners, edges, slice) > remaining)
		return (0);
	if (remaining == 0 && corners == 0 && edges == 0 && slice == 0)
	{
		length = phase1_length + (size_t)(phase2_limit - remaining);
		if (move_sequence_metric(search->path, length) < search->answer_metric
			|| search->answer_length == 0)
		{
			memcpy(search->answer, search->path,
				length * sizeof(search->path[0]));
			search->answer_length = length;
			search->answer_metric = move_sequence_metric(search->path, length);
		}
		return (0);
	}
	if (remaining == 0)
		return (0);
	move_index = 0;
	while (move_index < PHASE2_MOVE_COUNT)
	{
		move_id = g_phase2_move_ids[move_index];
		face = move_id / 3;
		if (face != previous_face
			&& (previous_face < 0 || !same_axis(face, previous_face)
				|| face >= previous_face))
		{
			next_corners = g_corner_perm_moves[corners][move_index];
			next_edges = g_edge_perm_moves[edges][move_index];
			next_slice = g_slice_perm_moves[slice][move_index];
			if (phase2_heuristic(next_corners, next_edges, next_slice)
				<= remaining - 1)
			{
				next_cube = *cube;
				if (apply_move_id(&next_cube, move_id) == 0)
				{
					search->path[phase1_length
						+ phase2_limit - remaining] = move_id;
					phase2_timed_search(timed, search, &next_cube,
						next_corners, next_edges, next_slice,
						remaining - 1, phase2_limit, face,
						phase1_length);
					if (timed->timed_out)
						return (0);
				}
			}
		}
		move_index++;
	}
	return (0);
}

static int	try_phase2_timed(t_timed_search *timed, t_search *search,
		const t_phase1_candidate *candidate, int total_depth)
{
	unsigned int	corners;
	unsigned int	edges;
	unsigned int	slice;
	int				depth;

	memcpy(search->path, candidate->path,
		candidate->length * sizeof(search->path[0]));
	corners = rank_permutation(candidate->cube.corners, 8);
	edges = rank_permutation(candidate->cube.edges, 8);
	slice = slice_permutation_from_cubies(&candidate->cube);
	depth = total_depth - (int)candidate->length;
	if (depth >= 0 && depth <= PHASE2_MAX_DEPTH
		&& depth >= phase2_heuristic(corners, edges, slice)
		&& !timed->timed_out)
		phase2_timed_search(timed, search, &candidate->cube, corners,
			edges, slice, depth, depth, -1, candidate->length);
	return (0);
}

static int	phase1_search(t_search *search, const t_cubie_cube *cube,
		unsigned int twist, unsigned int flip, unsigned int slice,
		int remaining, int previous_face, size_t depth)
{
	t_cubie_cube	next_cube;
	unsigned int	next_twist;
	unsigned int	next_flip;
	unsigned int	next_slice;
	int				move_id;
	int				face;

	if (phase1_heuristic(twist, flip, slice) > remaining)
		return (0);
	if (remaining == 0)
	{
		if (twist == 0 && flip == 0 && slice == g_solved_slice_coordinate)
			return (try_phase2(search, cube, depth));
		return (0);
	}
	move_id = 0;
	while (move_id < PHASE1_MOVE_COUNT)
	{
		face = move_id / 3;
		if (face == previous_face
			|| (previous_face >= 0 && same_axis(face, previous_face)
				&& face < previous_face))
		{
			move_id++;
			continue ;
		}
		next_twist = g_twist_moves[twist][move_id];
		next_flip = g_flip_moves[flip][move_id];
		next_slice = g_slice_moves[slice][move_id];
		if (phase1_heuristic(next_twist, next_flip, next_slice)
			<= remaining - 1)
		{
			next_cube = *cube;
			if (apply_move_id(&next_cube, move_id) == 0)
			{
				search->path[depth] = move_id;
				if (phase1_search(search, &next_cube, next_twist, next_flip,
						next_slice, remaining - 1, face, depth + 1))
					return (1);
			}
		}
		move_id++;
	}
	return (0);
}

static int	write_solution(t_move_list *solution, const int *answer,
		size_t answer_length)
{
	size_t		index;
	size_t		write_index;
	unsigned int	combined_turns;

	solution->items = malloc(answer_length * sizeof(*solution->items));
	if (answer_length != 0 && solution->items == NULL)
	{
		errno = ENOMEM;
		return (-1);
	}
	solution->length = answer_length;
	index = 0;
	write_index = 0;
	while (index < solution->length)
	{
		if (write_index != 0
			&& solution->items[write_index - 1].face
				== (t_face)(answer[index] / 3))
		{
			combined_turns = solution->items[write_index - 1].turns
				+ (unsigned int)(answer[index] % 3 + 1);
			combined_turns %= 4;
			if (combined_turns == 0)
				write_index--;
			else
				solution->items[write_index - 1].turns
					= (unsigned char)combined_turns;
		}
		else
		{
			solution->items[write_index].face = (t_face)(answer[index] / 3);
			solution->items[write_index].turns
				= (unsigned char)(answer[index] % 3 + 1);
			write_index++;
		}
		index++;
	}
	solution->length = write_index;
	return (0);
}

static int	build_distances(const uint16_t *moves, uint8_t *distances,
		unsigned int state_count)
{
	uint32_t		*queue;
	uint32_t		head;
	uint32_t		tail;
	unsigned int	move;
	unsigned int	next;

	memset(distances, 0xFF, state_count);
	queue = malloc(state_count * sizeof(*queue));
	if (queue == NULL)
	{
		errno = ENOMEM;
		return (-1);
	}
	head = 0;
	tail = 1;
	queue[0] = 0;
	distances[0] = 0;
	while (head < tail)
	{
		move = 0;
		while (move < PHASE1_MOVE_COUNT)
		{
			next = moves[(size_t)queue[head] * PHASE1_MOVE_COUNT + move];
			if (distances[next] == UINT8_MAX)
			{
				distances[next] = (uint8_t)(distances[queue[head]] + 1);
				queue[tail++] = next;
			}
			move++;
		}
		head++;
	}
	free(queue);
	if (tail != state_count)
	{
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

static int	build_two_tables(void)
{
	t_cubie_cube		state;
	t_cubie_cube		next;
	unsigned char		permutation[8];
	unsigned int		coordinate;
	unsigned int		move;

	if (g_two_tables_state == 1)
		return (0);
	if (g_two_tables_state == -1 || build_cubie_moves() != 0)
		return (-1);
	g_two_tables_state = -1;
	coordinate = 0;
	while (coordinate < TWIST_COUNT)
	{
		init_solved_cubies(&state);
		decode_twist(coordinate, &state);
		move = 0;
		while (move < PHASE1_MOVE_COUNT)
		{
			next = state;
			if (apply_move_id(&next, (int)move) != 0)
				return (-1);
			g_two_twist_moves[coordinate][move]
				= (uint16_t)encode_twist(&next);
			move++;
		}
		coordinate++;
	}
	coordinate = 0;
	while (coordinate < CORNER_PERM_COUNT)
	{
		unrank_permutation(coordinate, 8, permutation);
		init_permutation_state(&state, permutation, 8, 1);
		move = 0;
		while (move < PHASE1_MOVE_COUNT)
		{
			next = state;
			if (apply_move_id(&next, (int)move) != 0)
				return (-1);
			g_two_corner_moves[coordinate][move]
				= (uint16_t)rank_permutation(next.corners, 8);
			move++;
		}
		coordinate++;
	}
	if (build_distances(&g_two_twist_moves[0][0], g_two_twist_distance,
			TWIST_COUNT) != 0
		|| build_distances(&g_two_corner_moves[0][0], g_two_corner_distance,
			CORNER_PERM_COUNT) != 0)
		return (-1);
	g_two_tables_state = 1;
	return (0);
}

static int	two_search(t_search *search, unsigned int twist,
		unsigned int corners, int remaining, int previous_face, size_t depth)
{
	unsigned int	next_twist;
	unsigned int	next_corners;
	int			move_id;
	int			face;
	int			heuristic;

	heuristic = (int)g_two_twist_distance[twist];
	if ((int)g_two_corner_distance[corners] > heuristic)
		heuristic = (int)g_two_corner_distance[corners];
	if (heuristic > remaining)
		return (0);
	if (twist == 0 && corners == 0)
	{
		memcpy(search->answer, search->path, depth * sizeof(search->path[0]));
		search->answer_length = depth;
		return (1);
	}
	if (remaining == 0)
		return (0);
	move_id = 0;
	while (move_id < PHASE1_MOVE_COUNT)
	{
		face = move_id / 3;
		if (face == previous_face
			|| (previous_face >= 0 && same_axis(face, previous_face)
				&& face < previous_face))
		{
			move_id++;
			continue ;
		}
		next_twist = g_two_twist_moves[twist][move_id];
		next_corners = g_two_corner_moves[corners][move_id];
		search->path[depth] = move_id;
		if (two_search(search, next_twist, next_corners, remaining - 1,
				face, depth + 1))
			return (1);
		move_id++;
	}
	return (0);
}

int	solve_cube(const t_cube *cube, t_move_list *solution)
{
	t_cubie_cube	cubies;
	t_search		search;
	unsigned int	twist;
	unsigned int	flip;
	unsigned int	slice;
	int				depth;

	if (cube == NULL || solution == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	solution->items = NULL;
	solution->length = 0;
	if (cube_to_cubie(cube, &cubies) != 0)
	{
		errno = EINVAL;
		return (-1);
	}
	if (cubie_is_solved(&cubies))
		return (0);
	if (build_all_tables() != 0)
		return (-1);
	memset(&search, 0, sizeof(search));
	twist = encode_twist(&cubies);
	flip = encode_flip(&cubies);
	slice = slice_coordinate_from_cubies(&cubies);
	depth = phase1_heuristic(twist, flip, slice);
	while (depth <= PHASE1_MAX_DEPTH)
	{
		if (phase1_search(&search, &cubies, twist, flip, slice, depth, -1, 0))
			break ;
		depth++;
	}
	if (depth > PHASE1_MAX_DEPTH)
	{
		errno = ENOENT;
		return (-1);
	}
	return (write_solution(solution, search.answer, search.answer_length));
}

int	solve_cube_timed(const t_cube *cube, t_move_list *solution,
		unsigned int milliseconds)
{
	t_cubie_cube		cubies;
	t_search			search;
	t_move_list			initial_solution;
	t_timed_search		timed;
	t_phase1_candidate	*candidates;
	unsigned int		twist;
	unsigned int		flip;
	unsigned int		slice;
	size_t				candidate_count;
	size_t				candidate_seen;
	size_t				candidate;
	int					path[TIMED_PHASE1_MAX_DEPTH];
	int					depth;

	if (milliseconds == 0)
		milliseconds = DEFAULT_PHASE_TIME_MS;
	if (cube == NULL || solution == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	solution->items = NULL;
	solution->length = 0;
	if (cube_to_cubie(cube, &cubies) != 0)
	{
		errno = EINVAL;
		return (-1);
	}
	if (cubie_is_solved(&cubies))
		return (0);
	if (build_all_tables() != 0)
		return (-1);
	if (solve_cube(cube, &initial_solution) != 0)
		return (-1);
	memset(&search, 0, sizeof(search));
	search.answer_length = initial_solution.length;
	search.answer_metric = initial_solution.length;
	candidate = 0;
	while (candidate < initial_solution.length)
	{
		search.answer[candidate] = initial_solution.items[candidate].face * 3
			+ initial_solution.items[candidate].turns - 1;
		candidate++;
	}
	free_move_list(&initial_solution);
	candidates = malloc(PHASE1_CANDIDATE_LIMIT * sizeof(*candidates));
	if (candidates == NULL)
	{
		errno = ENOMEM;
		return (-1);
	}
	candidate_count = 0;
	candidate_seen = 0;
	twist = encode_twist(&cubies);
	flip = encode_flip(&cubies);
	slice = slice_coordinate_from_cubies(&cubies);
	depth = phase1_heuristic(twist, flip, slice);
	if (set_deadline(&timed, milliseconds) != 0)
	{
		free(candidates);
		return (-1);
	}
	while (depth <= TIMED_PHASE1_MAX_DEPTH && !timed.timed_out)
	{
		collect_phase1(&timed, candidates, &candidate_count, &candidate_seen,
			&cubies, twist, flip, slice, depth, -1, 0, path);
		depth++;
	}
	if (candidate_count == 0)
	{
		free(candidates);
		return (write_solution(solution, search.answer, search.answer_length));
	}
	if (set_deadline(&timed, milliseconds) != 0)
	{
		free(candidates);
		return (-1);
	}
	depth = 0;
	while (depth <= MAX_SOLUTION_LENGTH && !timed.timed_out)
	{
		candidate = 0;
		while (candidate < candidate_count && !timed.timed_out)
		{
			check_deadline(&timed);
			try_phase2_timed(&timed, &search, &candidates[candidate], depth);
			candidate++;
		}
		depth++;
	}
	free(candidates);
	return (write_solution(solution, search.answer, search.answer_length));
}

int	solve_cube_2x2(const t_cube *cube, t_move_list *solution)
{
	t_cubie_cube	cubies;
	t_search		search;
	unsigned int	twist;
	unsigned int	corners;
	int				depth;

	if (cube == NULL || solution == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	solution->items = NULL;
	solution->length = 0;
	if (cube_to_cubie(cube, &cubies) != 0)
	{
		errno = EINVAL;
		return (-1);
	}
	twist = encode_twist(&cubies);
	corners = rank_permutation(cubies.corners, 8);
	if (twist == 0 && corners == 0)
		return (0);
	if (build_two_tables() != 0)
		return (-1);
	memset(&search, 0, sizeof(search));
	depth = (int)g_two_twist_distance[twist];
	if ((int)g_two_corner_distance[corners] > depth)
		depth = (int)g_two_corner_distance[corners];
	while (depth <= TWO_MAX_DEPTH)
	{
		if (two_search(&search, twist, corners, depth, -1, 0))
			break ;
		depth++;
	}
	if (depth > TWO_MAX_DEPTH)
	{
		errno = ENOENT;
		return (-1);
	}
	return (write_solution(solution, search.answer, search.answer_length));
}
