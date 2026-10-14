#ifndef GAME_OBJECTS_H
#define GAME_OBJECTS_H
struct player_stats;
typedef struct game_object
{
	unsigned short	type;			/* 0xffff = free slot, 0xfffe = list head, bit 15 = keep on remove_all */
	short		id;			/* index in the table */
	short		next;			/* -2 = end */
	short		prev;
	short		*data;			/* set by init(); receives the id */
	int		(*init)(struct game_object *);
	void		(*draw)(struct game_object *);	/* run by the "FOO" GOOSE object's draw pass */
	void		(*think)(struct game_object *);	/* run by object_draw_func() each frame */
	int		(*hit)(struct game_object *, struct player_stats *);	/* the player's shot against it (shoot_objects) */
	int		(*die)(struct game_object *, struct player_stats *);	/* unused by the objects code: the enemies keep their die callback here too */
	void		(*remove)(struct game_object *);
	int		unused24;
} game_object_t;
#define MAX_OBJECTS	128
#define OBJ_FREE	0xffff
#define OBJ_HEAD	0xfffe
#define OBJ_NONE	(-2)
extern game_object_t	objects[MAX_OBJECTS];
extern short		num_objects_on_heap, cur_object_id, next_free_object;
void *get_object_data(int id);
void init_objects(void);
short add_object(game_object_t *tmpl);
short add_object1_wrapper(short type, void (*draw)(game_object_t *), int (*init)(game_object_t *), void (*think)(game_object_t *));
short remove_game_object(short id);
void remove_all_objects(void);
void remove_objects_of_type(short type);
#endif
