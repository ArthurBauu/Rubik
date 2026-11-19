#include "rubik.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

static int	read_random_u32(int descriptor, uint32_t *value)
{
	unsigned char	*bytes;
	size_t			total;
	ssize_t			amount;

	bytes = (unsigned char *)value;
	total = 0;
	while (total < sizeof(*value))
	{
		amount = read(descriptor, bytes + total, sizeof(*value) - total);
		if (amount > 0)
			total += (size_t)amount;
		else if (amount < 0 && errno == EINTR)
			continue ;
		else
		{
			if (amount == 0)
				errno = EIO;
			return (-1);
		}
	}
	return (0);
}

static int	random_below(int descriptor, uint32_t bound, uint32_t *value)
{
	uint32_t	random_value;
	uint32_t	threshold;

	threshold = (uint32_t)(0U - bound) % bound;
	while (1)
	{
		if (read_random_u32(descriptor, &random_value) != 0)
			return (-1);
		if (random_value >= threshold)
		{
			*value = random_value % bound;
			return (0);
		}
	}
}

int	generate_scramble(size_t length, t_move_list *scramble)
{
	uint32_t	random_face;
	uint32_t	random_turns;
	size_t		index;
	int			previous_face;
	int			descriptor;
	int			saved_errno;

	if (scramble == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	scramble->items = NULL;
	scramble->length = 0;
	if (length == 0)
	{
		errno = EINVAL;
		return (-1);
	}
	if (length > SIZE_MAX / sizeof(*scramble->items))
	{
		errno = EOVERFLOW;
		return (-1);
	}
	descriptor = open("/dev/urandom", O_RDONLY);
	if (descriptor < 0)
		return (-1);
	scramble->items = malloc(length * sizeof(*scramble->items));
	if (scramble->items == NULL)
	{
		close(descriptor);
		errno = ENOMEM;
		return (-1);
	}
	scramble->length = length;
	previous_face = -1;
	index = 0;
	while (index < length)
	{
		if (random_below(descriptor, previous_face < 0 ? 6 : 5,
				&random_face) != 0
			|| random_below(descriptor, 3, &random_turns) != 0)
		{
			saved_errno = errno;
			free_move_list(scramble);
			close(descriptor);
			errno = saved_errno;
			return (-1);
		}
		if (previous_face >= 0 && random_face >= (uint32_t)previous_face)
			random_face++;
		scramble->items[index].face = (t_face)random_face;
		scramble->items[index].turns = (unsigned char)(random_turns + 1);
		previous_face = (int)random_face;
		index++;
	}
	if (close(descriptor) != 0)
	{
		free_move_list(scramble);
		return (-1);
	}
	return (0);
}
