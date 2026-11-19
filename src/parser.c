#include "rubik.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

static int	face_from_char(char value, t_face *face)
{
	if (value == 'U')
		*face = FACE_U;
	else if (value == 'R')
		*face = FACE_R;
	else if (value == 'F')
		*face = FACE_F;
	else if (value == 'D')
		*face = FACE_D;
	else if (value == 'L')
		*face = FACE_L;
	else if (value == 'B')
		*face = FACE_B;
	else
		return (-1);
	return (0);
}

static int	append_move(t_move_list *moves, size_t *capacity, t_move move)
{
	t_move	*new_items;
	size_t	new_capacity;

	if (moves->length == *capacity)
	{
		new_capacity = 8;
		if (*capacity != 0)
		{
			if (*capacity > SIZE_MAX / 2 / sizeof(*moves->items))
			{
				errno = EOVERFLOW;
				return (-1);
			}
			new_capacity = *capacity * 2;
		}
		new_items = realloc(moves->items, new_capacity * sizeof(*moves->items));
		if (new_items == NULL)
			return (-1);
		moves->items = new_items;
		*capacity = new_capacity;
	}
	moves->items[moves->length++] = move;
	return (0);
}

static size_t	modifier_length(const char *input, size_t offset)
{
	if (input[offset] == '\'')
		return (1);
	if ((unsigned char)input[offset] == 0xE2
		&& (unsigned char)input[offset + 1] == 0x80
		&& (unsigned char)input[offset + 2] == 0x99)
		return (3);
	return (0);
}

int	parse_moves(const char *input, t_move_list *moves, size_t *error_offset)
{
	size_t	offset;
	size_t	capacity;
	size_t	suffix_length;
	t_move	move;

	if (input == NULL || moves == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	moves->items = NULL;
	moves->length = 0;
	if (error_offset != NULL)
		*error_offset = 0;
	offset = 0;
	capacity = 0;
	while (input[offset] != '\0')
	{
		if (isspace((unsigned char)input[offset]))
		{
			offset++;
			continue ;
		}
		if (face_from_char(input[offset], &move.face) != 0)
			goto invalid_input;
		move.turns = 1;
		offset++;
		if (input[offset] == '2')
		{
			move.turns = 2;
			offset++;
		}
		else
		{
			suffix_length = modifier_length(input, offset);
			if (suffix_length != 0)
			{
				move.turns = 3;
				offset += suffix_length;
			}
		}
		if (input[offset] != '\0' && !isspace((unsigned char)input[offset]))
			goto invalid_input;
		if (append_move(moves, &capacity, move) != 0)
		{
			free_move_list(moves);
			return (-1);
		}
	}
	return (0);

invalid_input:
	if (error_offset != NULL)
		*error_offset = offset;
	free_move_list(moves);
	errno = EINVAL;
	return (-1);
}

void	free_move_list(t_move_list *moves)
{
	if (moves == NULL)
		return ;
	free(moves->items);
	moves->items = NULL;
	moves->length = 0;
}
