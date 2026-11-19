#include "visual.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <stdio.h>

typedef struct s_view
{
	SDL_Window		*window;
	SDL_GLContext	context;
	t_playback		playback;
	float			yaw;
	float			pitch;
	int				dragging;
}	t_view;

static void	color_for_sticker(char color)
{
	if (color == 'U')
		glColor3f(0.95f, 0.95f, 0.95f);
	else if (color == 'D')
		glColor3f(1.0f, 0.82f, 0.05f);
	else if (color == 'F')
		glColor3f(0.05f, 0.72f, 0.18f);
	else if (color == 'B')
		glColor3f(0.08f, 0.3f, 0.9f);
	else if (color == 'R')
		glColor3f(0.85f, 0.05f, 0.06f);
	else
		glColor3f(1.0f, 0.42f, 0.02f);
}

static void	draw_cubelet(void)
{
	static const float	vertices[8][3] = {
		{-0.46f, -0.46f, -0.46f}, {0.46f, -0.46f, -0.46f},
		{0.46f, 0.46f, -0.46f}, {-0.46f, 0.46f, -0.46f},
		{-0.46f, -0.46f, 0.46f}, {0.46f, -0.46f, 0.46f},
		{0.46f, 0.46f, 0.46f}, {-0.46f, 0.46f, 0.46f}
	};
	static const int	faces[6][4] = {
		{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
		{2, 3, 7, 6}, {0, 3, 7, 4}, {1, 2, 6, 5}
	};
	int	face;
	int	vertex;

	glColor3f(0.035f, 0.035f, 0.045f);
	glBegin(GL_QUADS);
	face = 0;
	while (face < 6)
	{
		vertex = 0;
		while (vertex < 4)
			glVertex3fv(vertices[faces[face][vertex++]]);
		face++;
	}
	glEnd();
}

static void	draw_sticker(size_t index, char color)
{
	t_vector	position;
	t_vector	normal;
	t_vector	right_point;
	t_vector	up_point;
	t_vector	unused;
	t_vector	right;
	t_vector	up;
	float		center[3];
	int			corner;
	int			sx;
	int			sy;

	cube_facelet_geometry(index, &position, &normal);
	cube_facelet_geometry(index / 9 * 9 + 5, &right_point, &unused);
	cube_facelet_geometry(index / 9 * 9 + 1, &up_point, &unused);
	right.x = right_point.x - normal.x;
	right.y = right_point.y - normal.y;
	right.z = right_point.z - normal.z;
	up.x = up_point.x - normal.x;
	up.y = up_point.y - normal.y;
	up.z = up_point.z - normal.z;
	center[0] = (float)normal.x * 0.465f;
	center[1] = (float)normal.y * 0.465f;
	center[2] = (float)normal.z * 0.465f;
	color_for_sticker(color);
	glBegin(GL_QUADS);
	corner = 0;
	while (corner < 4)
	{
		sx = (corner == 0 || corner == 3) ? -1 : 1;
		sy = (corner < 2) ? -1 : 1;
		glVertex3f(center[0] + 0.39f * (sx * right.x + sy * up.x),
			center[1] + 0.39f * (sx * right.y + sy * up.y),
			center[2] + 0.39f * (sx * right.z + sy * up.z));
		corner++;
	}
	glEnd();
}

static int	same_position(t_vector position, int x, int y, int z)
{
	return (position.x == x && position.y == y && position.z == z);
}

static void	draw_piece(t_view *view, int x, int y, int z)
{
	const t_move	*move;
	t_vector		axis;
	t_vector		position;
	t_vector		normal;
	float			angle;
	unsigned int	duration;
	size_t			index;

	move = playback_current_move(&view->playback);
	glPushMatrix();
	if (move != NULL)
	{
		cube_facelet_geometry((size_t)move->face * 9 + 4, &position, &axis);
		if (x * axis.x + y * axis.y + z * axis.z == 1)
		{
			duration = move->turns == 2 ? 600 : 300;
			angle = -90.0f * (float)view->playback.elapsed_ms / duration;
			if (move->turns == 2)
				angle *= 2.0f;
			else if (move->turns == 3)
				angle = -angle;
			glRotatef(angle, (float)axis.x, (float)axis.y, (float)axis.z);
		}
	}
	glTranslatef((float)x, (float)y, (float)z);
	draw_cubelet();
	index = 0;
	while (index < 54)
	{
		cube_facelet_geometry(index, &position, &normal);
		if (same_position(position, x, y, z))
			draw_sticker(index, view->playback.cube.stickers[index]);
		index++;
	}
	glPopMatrix();
}

static void	draw_scene(t_view *view)
{
	int	width;
	int	height;
	int	x;
	int	y;
	int	z;

	SDL_GL_GetDrawableSize(view->window, &width, &height);
	if (width <= 0 || height <= 0)
		return ;
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-0.65 * (double)width / height, 0.65 * (double)width / height,
		-0.65, 0.65, 1.0, 100.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -8.0f);
	glRotatef(view->pitch, 1.0f, 0.0f, 0.0f);
	glRotatef(view->yaw, 0.0f, 1.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	x = -1;
	while (x <= 1)
	{
		y = -1;
		while (y <= 1)
		{
			z = -1;
			while (z <= 1)
				draw_piece(view, x, y, z++);
			y++;
		}
		x++;
	}
	SDL_GL_SwapWindow(view->window);
}

static int	handle_event(t_view *view, const SDL_Event *event)
{
	if (event->type == SDL_QUIT)
		return (0);
	if (event->type == SDL_KEYDOWN && !event->key.repeat)
	{
		if (event->key.keysym.sym == SDLK_ESCAPE)
			return (0);
		if (event->key.keysym.sym == SDLK_SPACE)
			playback_toggle_pause(&view->playback);
		if (event->key.keysym.sym == SDLK_RIGHT)
			playback_step(&view->playback);
		if (event->key.keysym.sym == SDLK_r
			&& playback_reset(&view->playback) != 0)
			return (-1);
		if (event->key.keysym.sym == SDLK_LEFT
			&& playback_previous(&view->playback) != 0)
			return (-1);
	}
	if (event->type == SDL_MOUSEBUTTONDOWN
		&& event->button.button == SDL_BUTTON_LEFT)
		view->dragging = 1;
	if (event->type == SDL_MOUSEBUTTONUP
		&& event->button.button == SDL_BUTTON_LEFT)
		view->dragging = 0;
	if (event->type == SDL_MOUSEMOTION && view->dragging)
	{
		view->yaw += event->motion.xrel * 0.5f;
		view->pitch += event->motion.yrel * 0.5f;
	}
	return (1);
}

static void	update_title(t_view *view)
{
	const t_move	*move;
	const char		*phase;
	char			title[160];

	move = playback_current_move(&view->playback);
	if (move == NULL)
		phase = "Termine";
	else if (view->playback.phase == 0)
		phase = "Melange";
	else
		phase = "Solution";
	snprintf(title, sizeof(title), "Rubik 3D - %s %zu/%zu%s",
		phase, view->playback.index,
		view->playback.phase == 0 ? view->playback.mix->length
		: view->playback.solution->length,
		view->playback.paused ? " - Pause" : "");
	SDL_SetWindowTitle(view->window, title);
}

int	visual_run(const t_move_list *mix, const t_move_list *solution, int full)
{
	t_view		view;
	SDL_Event	event;
	Uint32		last_tick;
	Uint32		now;
	int			running;
	int			result;
	int			event_result;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		fprintf(stderr, "rubik: SDL initialization failed: %s\n", SDL_GetError());
		return (-1);
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	view.window = SDL_CreateWindow("Rubik 3D", SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED, 900, 700,
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (view.window == NULL)
	{
		fprintf(stderr, "rubik: cannot create window: %s\n", SDL_GetError());
		SDL_Quit();
		return (-1);
	}
	view.context = SDL_GL_CreateContext(view.window);
	if (view.context == NULL)
	{
		fprintf(stderr, "rubik: cannot create OpenGL context: %s\n", SDL_GetError());
		SDL_DestroyWindow(view.window);
		SDL_Quit();
		return (-1);
	}
	SDL_GL_SetSwapInterval(1);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.09f, 0.10f, 0.13f, 1.0f);
	view.yaw = -35.0f;
	view.pitch = 25.0f;
	view.dragging = 0;
	if (playback_init(&view.playback, mix, solution, full) != 0)
	{
		fprintf(stderr, "rubik: cannot initialize animation\n");
		SDL_GL_DeleteContext(view.context);
		SDL_DestroyWindow(view.window);
		SDL_Quit();
		return (-1);
	}
	last_tick = SDL_GetTicks();
	running = 1;
	result = 0;
	while (running)
	{
		while (SDL_PollEvent(&event))
		{
			event_result = handle_event(&view, &event);
			if (event_result < 0)
			{
				fprintf(stderr, "rubik: cannot navigate animation\n");
				result = -1;
				running = 0;
				break ;
			}
			if (event_result == 0)
				running = 0;
		}
		if (!running)
			break ;
		now = SDL_GetTicks();
		if (playback_update(&view.playback, now - last_tick) != 0)
		{
			fprintf(stderr, "rubik: animation move failed\n");
			result = -1;
			break ;
		}
		last_tick = now;
		update_title(&view);
		draw_scene(&view);
		SDL_Delay(8);
	}
	SDL_GL_DeleteContext(view.context);
	SDL_DestroyWindow(view.window);
	SDL_Quit();
	return (result);
}
