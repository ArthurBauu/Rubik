#include "rubik.h"

#include <stddef.h>

typedef struct s_cubie_move
{
	unsigned char	corners[8];
	unsigned char	corner_orientation[8];
	unsigned char	edges[12];
	unsigned char	edge_orientation[12];
}	t_cubie_move;

static const char	g_colors[6] = {'U', 'R', 'F', 'D', 'L', 'B'};
static const unsigned char	g_corner_facelets[8][3] = {
	{8, 9, 20}, {6, 18, 38}, {0, 36, 47}, {2, 45, 11},
	{29, 26, 15}, {27, 44, 24}, {33, 53, 42}, {35, 17, 51}
};
static const char	g_corner_colors[8][3] = {
	{'U', 'R', 'F'}, {'U', 'F', 'L'}, {'U', 'L', 'B'}, {'U', 'B', 'R'},
	{'D', 'F', 'R'}, {'D', 'L', 'F'}, {'D', 'B', 'L'}, {'D', 'R', 'B'}
};
static const unsigned char	g_edge_facelets[12][2] = {
	{5, 10}, {7, 19}, {3, 37}, {1, 46},
	{32, 16}, {28, 25}, {30, 43}, {34, 52},
	{23, 12}, {21, 41}, {50, 39}, {48, 14}
};
static const char	g_edge_colors[12][2] = {
	{'U', 'R'}, {'U', 'F'}, {'U', 'L'}, {'U', 'B'},
	{'D', 'R'}, {'D', 'F'}, {'D', 'L'}, {'D', 'B'},
	{'F', 'R'}, {'F', 'L'}, {'B', 'L'}, {'B', 'R'}
};
static t_cubie_move	g_quarter_turns[6];
static int	g_move_tables_state;

static int	permutation_parity(const unsigned char *permutation, size_t length)
{
	int	parity;
	size_t	left;
	size_t	right;

	parity = 0;
	left = 0;
	while (left < length)
	{
		right = left + 1;
		while (right < length)
		{
			if (permutation[left] > permutation[right])
				parity ^= 1;
			right++;
		}
		left++;
	}
	return (parity);
}

static int	convert_corners(const t_cube *cube, t_cubie_cube *cubies)
{
	unsigned int	used;
	size_t			position;
	size_t			orientation;
	size_t			piece;
	char			color_1;
	char			color_2;

	used = 0;
	position = 0;
	while (position < 8)
	{
		orientation = 0;
		while (orientation < 3
			&& cube->stickers[g_corner_facelets[position][orientation]] != 'U'
			&& cube->stickers[g_corner_facelets[position][orientation]] != 'D')
			orientation++;
		if (orientation == 3)
			return (-1);
		color_1 = cube->stickers[
			g_corner_facelets[position][(orientation + 1) % 3]];
		color_2 = cube->stickers[
			g_corner_facelets[position][(orientation + 2) % 3]];
		piece = 0;
		while (piece < 8
			&& (g_corner_colors[piece][1] != color_1
				|| g_corner_colors[piece][2] != color_2))
			piece++;
		if (piece == 8 || (used & (1U << piece)) != 0)
			return (-1);
		used |= 1U << piece;
		cubies->corners[position] = (unsigned char)piece;
		cubies->corner_orientation[position] = (unsigned char)(orientation % 3);
		position++;
	}
	return (0);
}

static int	convert_edges(const t_cube *cube, t_cubie_cube *cubies)
{
	unsigned int	used;
	size_t			position;
	size_t			piece;
	char			color_0;
	char			color_1;

	used = 0;
	position = 0;
	while (position < 12)
	{
		color_0 = cube->stickers[g_edge_facelets[position][0]];
		color_1 = cube->stickers[g_edge_facelets[position][1]];
		piece = 0;
		while (piece < 12
			&& !((g_edge_colors[piece][0] == color_0
					&& g_edge_colors[piece][1] == color_1)
				|| (g_edge_colors[piece][0] == color_1
					&& g_edge_colors[piece][1] == color_0)))
			piece++;
		if (piece == 12 || (used & (1U << piece)) != 0)
			return (-1);
		used |= 1U << piece;
		cubies->edges[position] = (unsigned char)piece;
		cubies->edge_orientation[position]
			= (unsigned char)(g_edge_colors[piece][0] != color_0);
		position++;
	}
	return (0);
}

int	cube_to_cubie(const t_cube *cube, t_cubie_cube *cubies)
{
	unsigned int	color_counts[6] = {0, 0, 0, 0, 0, 0};
	unsigned int	twist_sum;
	unsigned int	flip_sum;
	size_t			sticker;
	size_t			color;

	if (cube == NULL || cubies == NULL)
		return (-1);
	sticker = 0;
	while (sticker < 54)
	{
		color = 0;
		while (color < 6 && cube->stickers[sticker] != g_colors[color])
			color++;
		if (color == 6)
			return (-1);
		color_counts[color]++;
		sticker++;
	}
	color = 0;
	while (color < 6)
	{
		if (color_counts[color] != 9 || cube->stickers[color * 9 + 4]
			!= g_colors[color])
			return (-1);
		color++;
	}
	if (convert_corners(cube, cubies) != 0
		|| convert_edges(cube, cubies) != 0)
		return (-1);
	twist_sum = 0;
	flip_sum = 0;
	sticker = 0;
	while (sticker < 8)
		twist_sum += cubies->corner_orientation[sticker++];
	sticker = 0;
	while (sticker < 12)
		flip_sum += cubies->edge_orientation[sticker++];
	if (twist_sum % 3 != 0 || flip_sum % 2 != 0)
		return (-1);
	if (permutation_parity(cubies->corners, 8)
		!= permutation_parity(cubies->edges, 12))
		return (-1);
	return (0);
}

static int	build_move_tables(void)
{
	t_cube			facelets;
	t_cubie_cube	converted;
	t_move			move;
	size_t			face;

	if (g_move_tables_state != 0)
		return (g_move_tables_state > 0 ? 0 : -1);
	face = 0;
	while (face < 6)
	{
		cube_init(&facelets);
		move.face = (t_face)face;
		move.turns = 1;
		if (cube_apply_move(&facelets, move) != 0
			|| cube_to_cubie(&facelets, &converted) != 0)
		{
			g_move_tables_state = -1;
			return (-1);
		}
		{
			size_t	index;

			index = 0;
			while (index < 8)
			{
				g_quarter_turns[face].corners[index] = converted.corners[index];
				g_quarter_turns[face].corner_orientation[index]
					= converted.corner_orientation[index];
				index++;
			}
			index = 0;
			while (index < 12)
			{
				g_quarter_turns[face].edges[index] = converted.edges[index];
				g_quarter_turns[face].edge_orientation[index]
					= converted.edge_orientation[index];
				index++;
			}
		}
		face++;
	}
	g_move_tables_state = 1;
	return (0);
}

static void	apply_quarter_turn(t_cubie_cube *cube, t_face face)
{
	t_cubie_cube	next;
	size_t			position;
	size_t			source;

	position = 0;
	while (position < 8)
	{
		source = g_quarter_turns[face].corners[position];
		next.corners[position] = cube->corners[source];
		next.corner_orientation[position]
			= (cube->corner_orientation[source]
				+ g_quarter_turns[face].corner_orientation[position]) % 3;
		position++;
	}
	position = 0;
	while (position < 12)
	{
		source = g_quarter_turns[face].edges[position];
		next.edges[position] = cube->edges[source];
		next.edge_orientation[position]
			= cube->edge_orientation[source]
			^ g_quarter_turns[face].edge_orientation[position];
		position++;
	}
	*cube = next;
}

int	cubie_apply_move(t_cubie_cube *cube, t_move move)
{
	unsigned char	turn;

	if (cube == NULL || move.face < FACE_U || move.face > FACE_B
		|| move.turns < 1 || move.turns > 3 || build_move_tables() != 0)
		return (-1);
	turn = 0;
	while (turn < move.turns)
	{
		apply_quarter_turn(cube, move.face);
		turn++;
	}
	return (0);
}

int	cubie_is_solved(const t_cubie_cube *cube)
{
	size_t	position;

	if (cube == NULL)
		return (0);
	position = 0;
	while (position < 8)
	{
		if (cube->corners[position] != position
			|| cube->corner_orientation[position] != 0)
			return (0);
		position++;
	}
	position = 0;
	while (position < 12)
	{
		if (cube->edges[position] != position
			|| cube->edge_orientation[position] != 0)
			return (0);
		position++;
	}
	return (1);
}
