/*
 * objects.c - CarnEvil's game object list: a fixed array of 128 nodes threaded into a list
 * ordered by type, with the head at index 0. Functions at 0x800cc398-0x800cc9c4.
 */
#include <stdio.h>

#include "objects.h"

game_object_t	objects[MAX_OBJECTS];
short		num_objects_on_heap;
short		cur_object_id;
short		next_free_object;
int		object_pad_801e5bfc;

void *get_object_data(int id)
{
	if((unsigned int)id > 0x80)
	{
		printf("Attempting to get_object_data with invalid id - %d\n", id);
		return(0);
	}
	if(objects[id].type == OBJ_FREE)
	{
		printf("Attempting to get_object_data with nonexistant id - %d\n", id);
		return(0);
	}
	return(objects[id].data);
}

void init_objects(void)
{
	int	i;

	object_pad_801e5bfc = 0;
	for(i = 0; i < MAX_OBJECTS; i++)
	{
		objects[i].type = OBJ_FREE;
		objects[i].next = OBJ_NONE;
		objects[i].prev = OBJ_NONE;
	}
	num_objects_on_heap = 1;
	next_free_object = 1;
	objects[0].type = OBJ_HEAD;
	objects[0].id = 0;
	objects[0].next = OBJ_NONE;
	objects[0].prev = OBJ_NONE;
}

static short alloc_object_id(void)
{
	short	i;

	i = next_free_object;
	while(objects[i].type != OBJ_FREE)
	{
		i = (i + 1) & 0x7f;
	}
	next_free_object = (i + 1) & 0x7f;
	return(i);
}

short remove_game_object(short id);

short add_object(game_object_t *tmpl)
{
	game_object_t	*obj;
	game_object_t	*p;
	short		id;
	int		err;

	if(num_objects_on_heap >= MAX_OBJECTS)
	{
		return(0);
	}
	id = alloc_object_id();
	obj = &objects[id];
	*obj = *tmpl;
	obj->id = id;
	obj->unused18 = 0;
	obj->unused1c = 0;
	obj->remove = 0;
	obj->data = 0;

	/* insert in type order */
	p = &objects[0];
	if(objects[0].id != OBJ_NONE)
	{
		for(;;)
		{
			if(p->type != OBJ_HEAD && p->type < obj->type)
			{
				break;
			}
			if(p->next == OBJ_NONE)
			{
				break;
			}
			p = &objects[p->next];
		}
	}
	if(p->next == OBJ_NONE)
	{
		obj->next = OBJ_NONE;
		obj->prev = p->id;
		p->next = id;
	}
	else
	{
		obj->prev = p->prev;
		obj->next = p->id;
		objects[p->prev].next = id;
		p->prev = id;
	}

	err = 0;
	if(obj->init)
	{
		err = obj->init(obj);
	}
	if(obj->data && obj->type != 0x1000)
	{
		*obj->data = obj->id;
	}
	num_objects_on_heap++;
	if(err)
	{
		printf("Big time fuckup in add object TYPE %04X ERR %d\n", obj->type, err);
		remove_game_object(obj->id);
		return(0);
	}
	return(obj->id);
}

short add_object1_wrapper(short type, void (*draw)(game_object_t *), int (*init)(game_object_t *), int think)
{
	game_object_t	t;

	t.type = type;
	t.draw = draw;
	t.init = init;
	t.think = (void (*)(game_object_t *))think;
	return(add_object(&t));
}

short remove_game_object(short id)
{
	game_object_t	*obj;

	obj = &objects[id];
	if(num_objects_on_heap < 2)
	{
		return(0);
	}
	if(id == 0)
	{
		printf("Trying to remove HEAD from object list\n");
		return(0);
	}
	if(obj->type == OBJ_FREE)
	{
		return(0);
	}
	if(obj->remove)
	{
		obj->remove(obj);
	}
	if(obj->id == cur_object_id)
	{
		cur_object_id = 0;
	}
	if(obj->next == OBJ_NONE)
	{
		objects[obj->prev].next = OBJ_NONE;
	}
	else
	{
		objects[obj->prev].next = obj->next;
		objects[obj->next].prev = obj->prev;
	}
	objects[id].type = OBJ_FREE;
	num_objects_on_heap--;
	return(1);
}

void remove_all_objects(void)
{
	game_object_t	*p;
	short		i;

	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		p = &objects[i];
		i = p->next;
		if(!(p->type & 0x8000))
		{
			remove_game_object(p->id);
		}
	}
}

void remove_objects_of_type(short type)
{
	game_object_t	*p;
	short		i;

	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		p = &objects[i];
		i = p->next;
		if(p->type == type)
		{
			remove_game_object(p->id);
		}
	}
}
