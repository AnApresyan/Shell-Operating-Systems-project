#include "shell.h"
#include "stdio.h"

mem_list *create_mem_list()
{
	mem_list *new_list = (mem_list *)malloc(sizeof(mem_list));
	if (new_list == NULL)
	{
		perror("Malloc error while creating list.");
		return NULL;
	}
	new_list->top = NULL;

	return new_list;
}

mem_block *create_block(void *addr, size_t size, char type)
{
	mem_block *new_block = (mem_block *)malloc(sizeof(mem_block));
	if (new_block == NULL)
	{
		perror("Memory allocation failed");
		return NULL;
	}

	new_block->addr = addr;
	new_block->size = size;
	time(&new_block->time);
	new_block->type = type;
	new_block->next = NULL;

	return new_block;
}

int insert_block(mem_list *list, void *addr, size_t size, char type_alloc)
{
	mem_block *new_block = create_block(addr, size, type_alloc);
	if (new_block != NULL)
	{
		new_block->next = list->top;
		list->top = new_block;
		return 1;
	}
	return 0;
}

void destroy_mem_list(mem_list *list)
{
	mem_block *current = list->top;
	mem_block *next;

	while (current != NULL)
	{
		next = current->next;
		if (current->type == 'p') {
			free(current->file_name);
			close(current->fd);
		}
		if (current->type == 'm')
			free(current->addr);
		free(current);
		current = next;
	}
	list->top = NULL;
	free(list);
}

void remove_block(mem_list *list, size_t size, char type)
{
	mem_block *tmp = list->top;
	mem_block *prev = NULL;

	while (tmp != NULL)
	{
		if (tmp->size == size && tmp->type == type)
			break;
		prev = tmp;
		tmp = tmp->next;
	}
	if (tmp == NULL)
		printf("No memory block of size %lu assigned by %c\n", (unsigned long)size, type);
	else
	{
		if (!prev)
			list->top = tmp->next;
		else
			prev->next = tmp->next;
		if (type == 'm')
			free(tmp->addr);
		free(tmp);
		printf("Deallocated %ld bytes\n", size);
	}
}

void remove_shared_block(mem_list *list, key_t key)
{
	mem_block *tmp = list->top;
	mem_block *prev = NULL;

	while (tmp != NULL && tmp->type == 's') {
		if (tmp->key == key)
			break;
		prev = tmp;
		tmp = tmp->next;
	}
	if (tmp == NULL)
		printf("No shared memory block with key %lu\n", (unsigned long)key);
	else {
		if (!prev)
			list->top = tmp->next;
		else
			prev->next = tmp->next;
		free(tmp);
	}

}

void insert_shared_block(mem_list *mem_blocks, void *p, size_t size, key_t key)
{
	if (insert_block(mem_blocks, p, size, 's'))
		mem_blocks->top->key = key;
}

void insert_mmap_block(mem_list *mem_blocks, void *p, size_t size, char *file, int fd)
{
	if (insert_block(mem_blocks, p, size, 'p')) {
		mem_blocks->top->file_name = strdup(file);	  //free in the end
		mem_blocks->top->fd = fd;
	}
}

void print_mem_list(mem_list *list)
{
	printf("Memory blocks assigned by process %lu\n", (long)getpid());
	if (list == NULL)
		return;
	mem_block *tmp = list->top; 
	while (tmp != NULL)
	{
		printf("%16p %10lu on ", tmp->addr, (unsigned long)tmp->size);
		print_time(tmp->time);
		if (tmp->type == 'm')
			printf(" malloc");
		if (tmp->type == 's')
			printf(" shared (key %lu)", (unsigned long)tmp->key);
		if (tmp->type == 'p')
			printf(" %s (descriptor %d)", tmp->file_name, tmp->fd);
		printf("\n");
		tmp = tmp->next;
	}
}


//Go over all the blocks, print their size, address, time 

//print only those where type == 'p', besides the above ones print the file and fd
void print_mmap_blocks(mem_list *list)			
{
	// printf("Printing mmap list:\n");
	printf("Memory blocks assigned by process %lu : mmap\n", (long)getpid());
	if (list == NULL)
		return;
	mem_block *tmp = list->top; 
	while (tmp != NULL)
	{
		//tmp->type == 'p'
		if (tmp->type == 'p') {
			printf("%16p %10lu on ", tmp->addr, (unsigned long)tmp->size);
			print_time(tmp->time);
			printf(" %s (descriptor %d)\n", tmp->file_name, tmp->fd);
		}
		tmp = tmp->next;
	}
}


//print only those where type == 's', besides the above ones print the key printf(" (key %lu)", (long)tmp->key);
void print_shared_blocks(mem_list *list)
{
	// printf("Printing shared list:\n");
	printf("Memory blocks assigned by process %lu : shared\n", (long)getpid());
	if (list == NULL)
		return;
	mem_block *tmp = list->top; 
	while (tmp != NULL)
	{
		//tmp->type == 's'
		if (tmp->type == 's')
		{
			printf("%16p %10lu on ", tmp->addr, (unsigned long)tmp->size);
			print_time(tmp->time);
			printf(" shared (key %lu)\n", (unsigned long)tmp->key);
		}
		tmp = tmp->next;
	}
}

//print only those where type == 'm'
void print_malloc_blocks(mem_list *list)
{
	// printf("Printing malloc list\n"); //tmp->type=='m'
	printf("Memory blocks assigned by process %lu : malloc\n", (long)getpid());
	if (list == NULL)
		return;
	mem_block *tmp = list->top; 
	while (tmp != NULL)
	{
		if (tmp->type == 'm')
		{
			printf("%16p %10lu on ", tmp->addr, (unsigned long)tmp->size);
			print_time(tmp->time);
			printf("\n");
		}
		tmp = tmp->next;
	}
}

void *get_sharedmem_addr(key_t key, mem_list *list)
{
	mem_block *block;
	if (list != NULL) {
		block = list->top;
		while (block) {
			if (block->type == 's' && block->key == key)
				return block->addr;
			block = block->next;
		}
	}
	return NULL;
}

int ft_munmap(void *addr, size_t size)
{
	if (munmap(addr, size) == -1)
	{
		perror("Error unmapping file");
		return 0;
	}
	// printf("Munmap successful\n");
	return 1;
}

void remove_mmap_block(char *file, mem_list *list)
{
	mem_block *block;
	mem_block *prev = NULL;

	if (file == NULL)
		return;
	block = list->top;
	while (block != NULL) {
		if (block->type == 'p' && !strcmp(file, block->file_name)){
			if (ft_munmap(block->addr, block->size)) {
				free(block->file_name);
				close(block->fd);
				if (!prev)
					list->top = block->next;
				else
					prev->next = block->next;
				free(block);
				return;
			}
		}
		prev = block;
		block = block->next;
	}
	// printf("No mmap block of that name in this process\n");
}