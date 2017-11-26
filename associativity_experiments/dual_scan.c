#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#define MAX_ASSOC 60
#define N (4096)
#define B 64
#define W (64*4096)
#define S (4096*1024)
#define A 16
#define TIMES 1000
// #define MICRO 1000000

struct timeval t1;
struct timeval t2;

// Initialize the given linked list structure.
void init(void * start, int assoc)
{
	// Tells which indices have been visited.
	int visited[assoc];
	int count = 0;
	visited[0] = 0;
	void ** curr = start;
	while(count < assoc-1)
	{
		// Chooses a random unvisited index.
		while(1)
		{
			int random_index = rand() % assoc;
			int already_visited = 0;
			int j;
			for(j = 0; j <= count; j++)
				if(visited[j] == random_index)
				{
					already_visited = 1;
					break;
				}
			if(!already_visited)
			{
				*curr = start + W*random_index;
				curr = *((void **)curr);
				count++;
				visited[count] = random_index;
				break;
			}
		}
	}
	*curr = NULL;
}

// Accesses the given list repeatedly.
void repeatAccess(void * start)
{
	int t;
	for(t = 0; t < TIMES; t++)
	{
		void ** curr = start;
		while(*curr != NULL)
			curr = *((void **)curr);
	}
}

int main()
{
	srand(time(NULL));
	void * base = malloc(64*S);
	int assoc = 42;
	int set = 0;
	while(set < N)
	{
		init(base + B*set, assoc);
		gettimeofday(&t1, NULL);
		repeatAccess(base + B*set);
		gettimeofday(&t2, NULL);
		printf("0\t%d\t%d\t%0.5lf\n", assoc, set, ((double)((t2.tv_sec-t1.tv_sec)*1000000 + (t2.tv_usec-t1.tv_usec)))/(assoc));
		set = set + 1;
	}

	set = N - 1;
	while(set >= 0)
	{
		gettimeofday(&t1, NULL);
		repeatAccess(base + B*set);
		gettimeofday(&t2, NULL);
		printf("1\t%d\t%d\t%.4lf\n", assoc, set, (double)((t2.tv_sec-t1.tv_sec)*1000000 + (t2.tv_usec-t1.tv_usec))/assoc);
		set = set - 1;
	}

	return 0;
}

