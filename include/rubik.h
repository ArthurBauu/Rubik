#ifndef RUBIK_H
#define RUBIK_H

#include <stddef.h>

#define DEFAULT_PHASE_TIME_MS 1500

typedef enum e_face
{
	FACE_U,
	FACE_R,
	FACE_F,
	FACE_D,
	FACE_L,
	FACE_B
}	t_face;

typedef struct s_move
{
	t_face		face;
	unsigned char	turns;
}	t_move;

typedef struct s_move_list
{
	t_move	*items;
	size_t	length;
}	t_move_list;

typedef struct s_cube
{
	char	stickers[54];
}	t_cube;

typedef struct s_vector
{
	int	x;
	int	y;
	int	z;
}	t_vector;

typedef struct s_cubie_cube
{
	unsigned char	corners[8];
	unsigned char	corner_orientation[8];
	unsigned char	edges[12];
	unsigned char	edge_orientation[12];
}	t_cubie_cube;

int		parse_moves(const char *input, t_move_list *moves,
			size_t *error_offset);
void	free_move_list(t_move_list *moves);
int		generate_scramble(size_t length, t_move_list *scramble);

void	cube_init(t_cube *cube);
void	cube_facelet_geometry(size_t index, t_vector *position,
			t_vector *normal);
int		cube_apply_move(t_cube *cube, t_move move);
int		cube_apply_moves(t_cube *cube, const t_move_list *moves);
int		cube_is_solved(const t_cube *cube);
int		cube_to_cubie(const t_cube *cube, t_cubie_cube *cubies);
int		cubie_apply_move(t_cubie_cube *cube, t_move move);
int		cubie_is_solved(const t_cubie_cube *cube);
int		solve_cube(const t_cube *cube, t_move_list *solution);int	solve_cube_timed(const t_cube *cube, t_move_list *solution,
		unsigned int milliseconds);
int	solve_cube_2x2(const t_cube *cube, t_move_list *solution);
#endif
