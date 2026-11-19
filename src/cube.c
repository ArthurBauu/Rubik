#include "rubik.h"

#include <stddef.h>

static const char	g_colors[6] = {'U', 'R', 'F', 'D', 'L', 'B'};
static const t_vector	g_normals[6] = {
	{0, 1, 0}, {1, 0, 0}, {0, 0, 1},
	{0, -1, 0}, {-1, 0, 0}, {0, 0, -1}
};
static const t_vector	g_right[6] = {
	{1, 0, 0}, {0, 0, -1}, {1, 0, 0},
	{1, 0, 0}, {0, 0, 1}, {-1, 0, 0}
};
static const t_vector	g_up[6] = {
	{0, 0, -1}, {0, 1, 0}, {0, 1, 0},
	{0, 0, 1}, {0, 1, 0}, {0, 1, 0}
};

static t_vector	add_scaled(t_vector base, t_vector direction, int scale)
{
	base.x += direction.x * scale;
	base.y += direction.y * scale;
	base.z += direction.z * scale;
	return (base);
}

void	cube_facelet_geometry(size_t index, t_vector *position,
		t_vector *normal)
{
	size_t	face;
	int		row;
	int		column;

	face = index / 9;
	row = (int)(index % 9) / 3;
	column = (int)(index % 3);
	*position = g_normals[face];
	*position = add_scaled(*position, g_right[face], column - 1);
	*position = add_scaled(*position, g_up[face], 1 - row);
	*normal = g_normals[face];
}

static t_vector	rotate_clockwise(t_vector value, t_vector axis)
{
	t_vector	result;
	int			projection;

	projection = axis.x * value.x + axis.y * value.y + axis.z * value.z;
	result.x = axis.x * projection
		- (axis.y * value.z - axis.z * value.y);
	result.y = axis.y * projection
		- (axis.z * value.x - axis.x * value.z);
	result.z = axis.z * projection
		- (axis.x * value.y - axis.y * value.x);
	return (result);
}

static int	same_vector(t_vector left, t_vector right)
{
	return (left.x == right.x && left.y == right.y && left.z == right.z);
}

static size_t	find_facelet(t_vector position, t_vector normal)
{
	size_t	index;
	t_vector	candidate_position;
	t_vector	candidate_normal;

	index = 0;
	while (index < 54)
	{
		cube_facelet_geometry(index, &candidate_position, &candidate_normal);
		if (same_vector(position, candidate_position)
			&& same_vector(normal, candidate_normal))
			return (index);
		index++;
	}
	return (54);
}

static void	rotate_layer(t_cube *cube, t_face face)
{
	char		next[54];
	size_t		index;
	size_t		destination;
	t_vector	position;
	t_vector	normal;
	t_vector	axis;

	axis = g_normals[face];
	index = 0;
	while (index < 54)
	{
		cube_facelet_geometry(index, &position, &normal);
		if (axis.x * position.x + axis.y * position.y
			+ axis.z * position.z == 1)
		{
			position = rotate_clockwise(position, axis);
			normal = rotate_clockwise(normal, axis);
		}
		destination = find_facelet(position, normal);
		if (destination < 54)
			next[destination] = cube->stickers[index];
		index++;
	}
	for (index = 0; index < 54; index++)
		cube->stickers[index] = next[index];
}

void	cube_init(t_cube *cube)
{
	size_t	index;

	if (cube == NULL)
		return ;
	index = 0;
	while (index < 54)
	{
		cube->stickers[index] = g_colors[index / 9];
		index++;
	}
}

int	cube_apply_move(t_cube *cube, t_move move)
{
	unsigned char	turn;

	if (cube == NULL || move.face < FACE_U || move.face > FACE_B
		|| move.turns < 1 || move.turns > 3)
		return (-1);
	turn = 0;
	while (turn < move.turns)
	{
		rotate_layer(cube, move.face);
		turn++;
	}
	return (0);
}

int	cube_apply_moves(t_cube *cube, const t_move_list *moves)
{
	size_t	index;

	if (cube == NULL || moves == NULL
		|| (moves->length != 0 && moves->items == NULL))
		return (-1);
	index = 0;
	while (index < moves->length)
	{
		if (cube_apply_move(cube, moves->items[index]) != 0)
			return (-1);
		index++;
	}
	return (0);
}

int	cube_is_solved(const t_cube *cube)
{
	size_t	face;
	size_t	sticker;

	if (cube == NULL)
		return (0);
	face = 0;
	while (face < 6)
	{
		sticker = face * 9;
		while (sticker < face * 9 + 9)
		{
			if (cube->stickers[sticker] != g_colors[face])
				return (0);
			sticker++;
		}
		face++;
	}
	return (1);
}
