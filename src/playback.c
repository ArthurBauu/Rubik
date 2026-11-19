#include "visual.h"

int	playback_init(t_playback *playback, const t_move_list *mix,
		const t_move_list *solution, int full)
{
	playback->mix = mix;
	playback->solution = solution;
	playback->full = full;
	playback->paused = 0;
	return (playback_reset(playback));
}

const t_move	*playback_current_move(t_playback *playback)
{
	const t_move_list	*moves;

	moves = playback->solution;
	if (playback->phase == 0)
		moves = playback->mix;
	if (playback->index >= moves->length)
		return (NULL);
	return (&moves->items[playback->index]);
}

int	playback_update(t_playback *playback, unsigned int delta_ms)
{
	const t_move	*move;
	unsigned int	duration;

	move = playback_current_move(playback);
	if (move == NULL || (playback->paused && !playback->stepping))
		return (0);
	duration = 300;
	if (move->turns == 2)
		duration = 600;
	if (delta_ms >= duration - playback->elapsed_ms)
	{
		if (cube_apply_move(&playback->cube, *move) != 0)
			return (-1);
		playback->index++;
		playback->elapsed_ms = 0;
		playback->stepping = 0;
		if (playback->phase == 0 && playback->index == playback->mix->length)
		{
			playback->phase = 1;
			playback->index = 0;
			playback->paused = 1;
		}
	}
	else
		playback->elapsed_ms += delta_ms;
	return (0);
}

void	playback_toggle_pause(t_playback *playback)
{
	playback->paused = !playback->paused;
	playback->stepping = 0;
}

void	playback_step(t_playback *playback)
{
	if (playback->paused && playback_current_move(playback) != NULL)
		playback->stepping = 1;
}

int	playback_reset(t_playback *playback)
{
	t_cube	initial;

	cube_init(&initial);
	if (!playback->full && cube_apply_moves(&initial, playback->mix) != 0)
		return (-1);
	playback->cube = initial;
	playback->index = 0;
	playback->elapsed_ms = 0;
	playback->phase = !playback->full;
	playback->stepping = 0;
	if (playback->full && playback->mix->length == 0)
	{
		playback->phase = 1;
		playback->paused = 1;
	}
	return (0);
}

int	playback_previous(t_playback *playback)
{
	const t_move_list	*moves;
	t_move				inverse;
	size_t				index;
	int					phase;

	playback->paused = 1;
	playback->stepping = 0;
	if (playback->elapsed_ms != 0)
	{
		playback->elapsed_ms = 0;
		return (0);
	}
	phase = playback->phase;
	index = playback->index;
	if (phase == 1 && index == 0)
	{
		if (!playback->full)
			return (0);
		phase = 0;
		index = playback->mix->length;
	}
	if (index == 0)
		return (0);
	moves = playback->mix;
	if (phase == 1)
		moves = playback->solution;
	inverse = moves->items[index - 1];
	inverse.turns = 4 - inverse.turns;
	if (cube_apply_move(&playback->cube, inverse) != 0)
		return (-1);
	playback->phase = phase;
	playback->index = index - 1;
	return (0);
}
