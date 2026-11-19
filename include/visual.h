#ifndef VISUAL_H
#define VISUAL_H

#include "rubik.h"

typedef struct s_playback
{
	t_cube				cube;
	const t_move_list	*mix;
	const t_move_list	*solution;
	size_t				index;
	unsigned int		elapsed_ms;
	int					phase;
	int					full;
	int					paused;
	int					stepping;
}	t_playback;

int		playback_init(t_playback *playback, const t_move_list *mix,
			const t_move_list *solution, int full);
const t_move	*playback_current_move(t_playback *playback);
int		playback_update(t_playback *playback, unsigned int delta_ms);
void	playback_toggle_pause(t_playback *playback);
void	playback_step(t_playback *playback);
int		playback_reset(t_playback *playback);
int		playback_previous(t_playback *playback);
int		visual_run(const t_move_list *mix, const t_move_list *solution,
			int full);

#endif
