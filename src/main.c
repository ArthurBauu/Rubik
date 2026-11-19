#define _POSIX_C_SOURCE 200809L

#include "rubik.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef WITH_VISUAL
# include "visual.h"
#endif

static int	print_moves(const t_move_list *moves)
{
	static const char	face_letters[] = "URFDLB";
	size_t				index;

	index = 0;
	while (index < moves->length)
	{
		if (index != 0 && fputc(' ', stdout) == EOF)
			return (-1);
		if (fputc(face_letters[moves->items[index].face], stdout) == EOF)
			return (-1);
		if (moves->items[index].turns == 2 && fputc('2', stdout) == EOF)
			return (-1);
		if (moves->items[index].turns == 3 && fputc('\'', stdout) == EOF)
			return (-1);
		index++;
	}
	if (fputc('\n', stdout) == EOF)
		return (-1);
	return (0);
}

static int	parse_positive_size(const char *argument, size_t *value)
{
	const char	*cursor;
	char		*end;
	uintmax_t	parsed;

	if (argument[0] == '\0')
		return (-1);
	cursor = argument;
	while (*cursor != '\0')
	{
		if (!isdigit((unsigned char)*cursor))
			return (-1);
		cursor++;
	}
	errno = 0;
	parsed = strtoumax(argument, &end, 10);
	if (errno != 0 || *end != '\0' || parsed == 0 || parsed > SIZE_MAX)
		return (-1);
	*value = (size_t)parsed;
	return (0);
}

static int	parse_generator_options(int argc, char **argv,
		size_t *length, size_t *count, int *solve, int *timed, int *stats)
{
	int	length_seen;
	int	count_seen;
	int	solve_seen;
	int	index;

	*length = 25;
	*count = 1;
	length_seen = 0;
	count_seen = 0;
	solve_seen = 0;
	*solve = 0;
	*timed = 0;
	*stats = 0;
	index = 2;
	while (index < argc)
	{
		if (strcmp(argv[index], "--solve") == 0 && !solve_seen)
		{
			*solve = 1;
			solve_seen = 1;
			index++;
		}
		else if (strcmp(argv[index], "--timed") == 0 && !*timed)
		{
			*timed = 1;
			index++;
		}
		else if (strcmp(argv[index], "--stats") == 0 && !*stats)
		{
			*stats = 1;
			index++;
		}
		else if (strcmp(argv[index], "--length") == 0 && !length_seen
			&& index + 1 < argc
			&& parse_positive_size(argv[index + 1], length) == 0)
		{
			length_seen = 1;
			index += 2;
		}
		else if (strcmp(argv[index], "--count") == 0 && !count_seen
			&& index + 1 < argc
			&& parse_positive_size(argv[index + 1], count) == 0)
		{
			count_seen = 1;
			index += 2;
		}
		else
			return (-1);
	}
	return ((*timed || *stats) && !*solve ? -1 : 0);
}

static int	parse_solve_options(int argc, char **argv, int start,
		int *stats, int *timed, int *two_by_two, const char **input)
{
	int	index;

	*stats = 0;
	*timed = 0;
	*two_by_two = 0;
	index = start;
	while (index < argc - 1)
	{
		if (strcmp(argv[index], "--stats") == 0 && !*stats)
			*stats = 1;
		else if (strcmp(argv[index], "--timed") == 0 && !*timed)
			*timed = 1;
		else if (strcmp(argv[index], "--2x2") == 0 && !*two_by_two)
			*two_by_two = 1;
		else
			return (-1);
		index++;
	}
	if (index != argc - 1 || (*timed && *two_by_two))
		return (-1);
	*input = argv[index];
	return (0);
}

static int	print_labeled_moves(const char *label, size_t number,
		const t_move_list *moves)
{
	if (fprintf(stdout, "%s %zu: ", label, number) < 0)
		return (-1);
	return (print_moves(moves));
}

static int	run_generator(size_t length, size_t count, int solve, int timed,
		int stats)
{
	t_move_list	scramble;
	t_move_list	solution;
	t_cube		cube;
	size_t		index;
	struct timespec	start;
	struct timespec	end;
	double			elapsed;

	index = 0;
	while (index < count)
	{
		if (generate_scramble(length, &scramble) != 0)
		{
			perror("rubik: cannot generate scramble");
			return (EXIT_FAILURE);
		}
		if (solve)
		{
			if (stats && clock_gettime(CLOCK_MONOTONIC, &start) != 0)
			{
				free_move_list(&scramble);
				perror("rubik: cannot start timer");
				return (EXIT_FAILURE);
			}
			cube_init(&cube);
			if (cube_apply_moves(&cube, &scramble) != 0)
			{
				free_move_list(&scramble);
				fprintf(stderr, "rubik: failed to apply generated mix\n");
				return (EXIT_FAILURE);
			}
			if ((timed && solve_cube_timed(&cube, &solution,
					DEFAULT_PHASE_TIME_MS) != 0)
				|| (!timed && solve_cube(&cube, &solution) != 0))
			{
				free_move_list(&scramble);
				perror("rubik: cannot solve generated mix");
				return (EXIT_FAILURE);
			}
			if (stats && clock_gettime(CLOCK_MONOTONIC, &end) != 0)
			{
				free_move_list(&solution);
				free_move_list(&scramble);
				perror("rubik: cannot read timer");
				return (EXIT_FAILURE);
			}
			if ((index != 0 && fputc('\n', stdout) == EOF)
				|| print_labeled_moves("Mix", index + 1, &scramble) != 0
				|| print_labeled_moves("Solution", index + 1, &solution) != 0)
			{
				free_move_list(&solution);
				free_move_list(&scramble);
				perror("rubik: cannot write generated mix and solution");
				return (EXIT_FAILURE);
			}
			if (stats)
			{
				elapsed = (double)(end.tv_sec - start.tv_sec)
					+ (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
				if (fprintf(stdout, "Mouvements: %zu\nTemps: %.6f s\n",
						solution.length, elapsed) < 0)
				{
					free_move_list(&solution);
					free_move_list(&scramble);
					perror("rubik: cannot write statistics");
					return (EXIT_FAILURE);
				}
			}
			free_move_list(&solution);
		}
		else if (print_moves(&scramble) != 0)
		{
			free_move_list(&scramble);
			perror("rubik: cannot write scramble");
			return (EXIT_FAILURE);
		}
		free_move_list(&scramble);
		index++;
	}
	return (EXIT_SUCCESS);
}

#ifdef WITH_VISUAL
static int	run_visual(const char *input, int full)
{
	t_move_list	mix;
	t_move_list	solution;
	t_cube		cube;
	size_t		error_offset;
	int			result;

	if (parse_moves(input, &mix, &error_offset) != 0)
	{
		if (errno == EINVAL)
			fprintf(stderr, "rubik: invalid mix at byte %zu\n", error_offset);
		else
			perror("rubik: cannot parse mix");
		return (EXIT_FAILURE);
	}
	cube_init(&cube);
	if (cube_apply_moves(&cube, &mix) != 0)
	{
		fprintf(stderr, "rubik: failed to apply mix\n");
		free_move_list(&mix);
		return (EXIT_FAILURE);
	}
	if (solve_cube(&cube, &solution) != 0)
	{
		perror("rubik: cannot solve cube");
		free_move_list(&mix);
		return (EXIT_FAILURE);
	}
	result = visual_run(&mix, &solution, full);
	free_move_list(&solution);
	free_move_list(&mix);
	return (result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
#endif

int	main(int argc, char **argv)
{
	t_move_list	mix;
	t_move_list	solution;
	t_cube		cube;
	size_t		error_offset;
	size_t		length;
	size_t		count;
	int			solve;
	int			stats;
	int			timed;
	int			two_by_two;
	const char	*input;
	struct timespec	start;
	struct timespec	end;
	double			elapsed;

#ifdef WITH_VISUAL
	if (argc >= 2 && strcmp(argv[1], "--visual") == 0)
	{
		if (argc == 3 && strcmp(argv[2], "--full") != 0)
			return (run_visual(argv[2], 0));
		if (argc == 4 && strcmp(argv[2], "--full") == 0)
			return (run_visual(argv[3], 1));
		if (argc == 4 && strcmp(argv[3], "--full") == 0)
			return (run_visual(argv[2], 1));
		fprintf(stderr, "usage: %s --visual [--full] \"MIX\"\n", argv[0]);
		return (EXIT_FAILURE);
	}
#endif
	if (argc >= 2 && strcmp(argv[1], "--generate") == 0)
	{
		if (parse_generator_options(argc, argv, &length, &count, &solve,
				&timed, &stats) != 0)
		{
			fprintf(stderr, "usage: %s --generate [--solve] "
				"[--timed] [--stats] [--length N] [--count N]\n", argv[0]);
			return (EXIT_FAILURE);
		}
		return (run_generator(length, count, solve, timed, stats));
	}
	if (argc < 2 || parse_solve_options(argc, argv, 1, &stats, &timed,
			&two_by_two, &input) != 0)
	{
		fprintf(stderr, "usage: %s [--stats] [--timed] \"MIX\"\n"
			"       %s [--stats] --2x2 \"MIX\"\n"
			"       %s --generate [--solve] [--timed] [--stats] "
			"[--length N] [--count N]\n",
			argv[0], argv[0], argv[0]);
		return (EXIT_FAILURE);
	}
	if (stats && clock_gettime(CLOCK_MONOTONIC, &start) != 0)
	{
		perror("rubik: cannot start timer");
		return (EXIT_FAILURE);
	}
	if (parse_moves(input, &mix, &error_offset) != 0)
	{
		if (errno == EINVAL)
			fprintf(stderr, "rubik: invalid mix at byte %zu\n", error_offset);
		else
			perror("rubik: cannot parse mix");
		return (EXIT_FAILURE);
	}
	cube_init(&cube);
	if (cube_apply_moves(&cube, &mix) != 0)
	{
		free_move_list(&mix);
		fprintf(stderr, "rubik: failed to apply mix\n");
		return (EXIT_FAILURE);
	}
	free_move_list(&mix);
	if ((two_by_two && solve_cube_2x2(&cube, &solution) != 0)
		|| (!two_by_two && timed
			&& solve_cube_timed(&cube, &solution, DEFAULT_PHASE_TIME_MS) != 0)
		|| (!two_by_two && !timed && solve_cube(&cube, &solution) != 0))
	{
		if (errno == EINVAL)
			fprintf(stderr, "rubik: invalid cube state\n");
		else
			perror("rubik: cannot solve cube");
		return (EXIT_FAILURE);
	}
	if (stats && clock_gettime(CLOCK_MONOTONIC, &end) != 0)
	{
		free_move_list(&solution);
		perror("rubik: cannot read timer");
		return (EXIT_FAILURE);
	}
	if ((stats && fputs("Solution: ", stdout) == EOF)
		|| print_moves(&solution) != 0)
	{
		free_move_list(&solution);
		perror("rubik: cannot write solution");
		return (EXIT_FAILURE);
	}
	if (stats)
	{
		elapsed = (double)(end.tv_sec - start.tv_sec)
			+ (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
		if (fprintf(stdout, "Mouvements: %zu\nTemps: %.6f s\n",
				solution.length, elapsed) < 0 || fflush(stdout) == EOF)
		{
			free_move_list(&solution);
			perror("rubik: cannot write statistics");
			return (EXIT_FAILURE);
		}
	}
	free_move_list(&solution);
	return (EXIT_SUCCESS);
}
