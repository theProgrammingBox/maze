#include <iostream>
#include <queue>
#include <chrono>
#include <utility>

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

using olc::vi2d;
using olc::Pixel;
using std::vector;
using std::queue;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;
using std::pair;

class Maze : public olc::PixelGameEngine
{
public:
	int MAZE_WIDTH;			// Width of the maze, number of "nodes"
	int MAZE_HEIGHT;		// Height of the maze, number of "nodes"

	int mazeRealWidth;		// Width of the maze, number of cells, can be path or wall
	int mazeRealHeight;		// Height of the maze, number of cells, can be path or wall
	
	uint8_t* maze;				// physical maze, 2x2 cells
	uint8_t* mazeDirections;	// path directions from each "node" to its neighbours
	
	vi2d playerPosition;	// same as a pair, stores x and y coordinates of the player
	vi2d goalPosition;		// same as a pair, stores x and y coordinates of the goal
	
	const vi2d directions[4] = { {0, 1}, {-1, 0}, {0, -1}, {1, 0} };	// directions to move in the maze

	enum MazeBits
	{
		UP = 0x01,		// 0000 0001, is there a path above this node?
		LEFT = 0x02,	// 0000 0010, is there a path to the left of this node?
		DOWN = 0x04,	// 0000 0100, is there a path below this node?
		RIGHT = 0x08,	// 0000 1000, is there a path to the right of this node?
		VISITED = 0x10,	// 0001 0000, has this node been visited? reused when generating and solving the maze
		PATH = 0x20		// 0010 0000, is this node a path or a wall?
	};

	vector<vi2d> shortestPath;	// DFS result;
	float timer = 0.0f;			// timer for the DFS animation
	float TIME_STEP;			// time between each DFS step

	int TRAIN_LENGTH;			// length of player trail
	vi2d* playerTrail;			// completely cosmetics
	int trailIndex = 0;			// keeps track of the circular array, instead of using a queue cuz fast

	unsigned int seed;			// seed for the xor random number generator
	
	Maze(int MAZE_WIDTH, int MAZE_HEIGHT, float TIME_STEP, int TRAIN_LENGTH)
	{
		sAppName = "Maze Genertor and Solver";
		
		this->MAZE_WIDTH = MAZE_WIDTH;
		this->MAZE_HEIGHT = MAZE_HEIGHT;
		this->TIME_STEP = TIME_STEP;
		this->TRAIN_LENGTH = TRAIN_LENGTH;

		mazeRealWidth = MAZE_WIDTH * 2;		// each node contains the main path and side paths connecting to its neighbours
		mazeRealHeight = MAZE_HEIGHT * 2;	// each node contains the main path and side paths connecting to its neighbours
		
		maze = new uint8_t[mazeRealWidth * mazeRealHeight];		// 2x2 nodes, each cell describes a path/wall and if it has been visited during solving
		mazeDirections = new uint8_t[MAZE_WIDTH * MAZE_HEIGHT];	// each cell describes if it is up, left, down, right, and if it has been visited during generating

		playerTrail = new vi2d[TRAIN_LENGTH];	// a trail behind the player, completely cosmetics
		
		seed = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
	}

	~Maze()
	{
		delete[] maze;
		delete[] mazeDirections;
		delete[] playerTrail;
	}

	unsigned int Rand2()	//xorshift32
	{
		seed ^= seed << 13;
		seed ^= seed >> 17;
		seed ^= seed << 5;
		return seed;
	}

	void RandomizeMaze()
	{
		memset(maze, 0, sizeof(uint8_t) * mazeRealWidth * mazeRealHeight);		// set all cells to no path
		memset(mazeDirections, 0, sizeof(uint8_t) * MAZE_WIDTH * MAZE_HEIGHT);	// set all cells to no connections and not visited

		vector<vi2d> stack;
		stack.push_back({ MAZE_WIDTH / 2, MAZE_HEIGHT / 2 });						// start at the middle of the maze

		vi2d nextPos;
		vector<uint8_t> neighbours;
		while (!stack.empty())
		{
			neighbours.clear();
			vi2d current = stack.back();
			mazeDirections[current.x + current.y * MAZE_WIDTH] |= VISITED;		// mark current node as visited

			for (int i = 4; i--;)
			{
				nextPos = current + directions[i];
				if (nextPos.x >= 0 && nextPos.x < MAZE_WIDTH && nextPos.y >= 0 && nextPos.y < MAZE_HEIGHT && !(mazeDirections[nextPos.y * MAZE_WIDTH + nextPos.x] & VISITED))
					neighbours.push_back(i);
			}

			if (neighbours.empty())
			{
				stack.pop_back();
			}
			else
			{
				int direction = neighbours[Rand2() % neighbours.size()];
				nextPos = current + directions[direction];

				mazeDirections[current.y * MAZE_WIDTH + current.x] |= (1 << direction);				// set the direction bit to 1, reference MazeBits
				mazeDirections[nextPos.y * MAZE_WIDTH + nextPos.x] |= (1 << (direction + 2) % 4);	// opposite direction, loop around the direction bit
				stack.push_back(nextPos);	// add the new cell to the stack
			}
		}

		int mazex;
		int mazey;
		for (int x = MAZE_WIDTH; x--;)
		{
			for (int y = MAZE_HEIGHT; y--;)
			{
				mazex = x << 1;	// convert to cell space
				mazey = y << 1;	// convert to cell space

				maze[mazey * mazeRealWidth + mazex] |= PATH;			// set the center cell to path

				if (mazeDirections[y * MAZE_WIDTH + x] & UP)
				{
					maze[(mazey + 1) * mazeRealWidth + mazex] |= PATH;	// set the top cell to path
				}
				if (mazeDirections[y * MAZE_WIDTH + x] & RIGHT)
				{
					maze[mazey * mazeRealWidth + mazex + 1] |= PATH;	// set the right cell to path
				}
			}
		}
	}

	void RandomizePlayer()
	{
		do
		{	// randomize player position
			playerPosition = { int(Rand2() % mazeRealWidth), int(Rand2() % mazeRealHeight) };
		} while (!(maze[playerPosition.y * mazeRealWidth + playerPosition.x] & PATH || goalPosition == playerPosition));
		
		for (int i = TRAIN_LENGTH; i--;) playerTrail[i] = playerPosition;	// player trail setup
	}

	void RandomizeGoal()
	{
		do
		{	// randomize goal position
			goalPosition = { int(Rand2() % mazeRealWidth), int(Rand2() % mazeRealHeight) };
		} while (!(maze[goalPosition.y * mazeRealWidth + goalPosition.x] & PATH || goalPosition == playerPosition));
	}

	void FindShortestPath()	//BFS
	{
		int* distances = new int[mazeRealWidth * mazeRealHeight];
		memset(distances, -1, sizeof(int) * mazeRealWidth * mazeRealHeight);
		distances[goalPosition.y * mazeRealWidth + goalPosition.x] = 0;
		
		queue<vi2d> queue;
		queue.push(goalPosition);
		
		vi2d current;
		vi2d nextPos;
		while (!queue.empty())
		{
			current = queue.front();
			queue.pop();

			for (int i = 4; i--;)
			{
				nextPos = current + directions[i];
				if (nextPos.x >= 0 && nextPos.x < mazeRealWidth && nextPos.y >= 0 && nextPos.y < mazeRealHeight && distances[nextPos.y * mazeRealWidth + nextPos.x] == -1 && maze[nextPos.y * mazeRealWidth + nextPos.x] & PATH)
				{
					distances[nextPos.y * mazeRealWidth + nextPos.x] = distances[current.y * mazeRealWidth + current.x] + 1;
					queue.push(nextPos);
				}
			}
		}
		
		shortestPath.resize(distances[playerPosition.y * mazeRealWidth + playerPosition.x]);
		current = playerPosition;
		for (int i = shortestPath.size(); i--;)
		{
			for (int j = 4; j--;)
			{
				nextPos = current + directions[j];
				if (nextPos.x >= 0 && nextPos.x < mazeRealWidth && nextPos.y >= 0 && nextPos.y < mazeRealHeight && distances[nextPos.y * mazeRealWidth + nextPos.x] == distances[current.y * mazeRealWidth + current.x] - 1)
					break;
			}
			shortestPath[i] = nextPos;
			current = nextPos;
		}
		
		delete[] distances;
	}

	void NewScene()
	{
		RandomizeMaze();	// randomize the maze
		RandomizePlayer();	// randomize the player position
		RandomizeGoal();	// randomize the goal position
		FindShortestPath();	// find the shortest path to the goal
	}

	void DrawMaze()
	{
		for (int x = mazeRealWidth; x--;)
			for (int y = mazeRealHeight; y--;)
				if (maze[y * mazeRealWidth + x])
					Draw(x, y, Pixel(240, 240, 240));	// whitish grey
	}

	void DrawGoalTrail()
	{
		int color;
		for (int i = shortestPath.size(); i--;)
		{
			color = 150 * i / shortestPath.size() + 50;
			Draw(shortestPath[i], Pixel(color, color, 255));	// blue
		}
	}

	void DrawPlayerTrail()
	{
		int color;
		for (int i = TRAIN_LENGTH; i--;)
		{
			color = 150 * i / TRAIN_LENGTH + 50;
			Draw(playerTrail[trailIndex++], Pixel(color, 255, color));	// green
			trailIndex -= (trailIndex == TRAIN_LENGTH) * TRAIN_LENGTH;
		}
	}

	void MovePlayer(float fElapsedTime)
	{
		if (playerPosition != goalPosition)
		{
			timer += fElapsedTime;
			if (timer >= TIME_STEP)	// move towards the goal every TIME_STEP ms
			{
				timer -= TIME_STEP;											// reset timer
				playerTrail[trailIndex++] = playerPosition;					// add current position to trail
				trailIndex -= (trailIndex >= TRAIN_LENGTH) * TRAIN_LENGTH;	// reset trail index if it goes over the trail length
				shortestPath.pop_back();									// remove the last position from the shortest path
				playerPosition = shortestPath.back();						// set the player position to the last position in the shortest path
			}
		}
		else
		{
			RandomizeGoal();	// randomize the goal
			FindShortestPath();	// find the new shortest path
		}
	}
	
	bool OnUserCreate()
	{
		NewScene();
		
		return true;
	}

	bool OnUserUpdate(float fElapsedTime)
	{
		if (GetKey(olc::SPACE).bPressed) NewScene();
		
		Clear(Pixel(30, 30, 30));	// clear the screen with blackish grey
		DrawMaze();
		DrawGoalTrail();
		DrawPlayerTrail();
		MovePlayer(fElapsedTime);
		
		return true;
	}
};

int main()
{
	const int MAZE_WIDTH = 256;		// width of the maze
	const int MAZE_HEIGHT = 128;	// height of the maze
	const float TIME_STEP = 0.001f;	// time (ms) between each action
	const int TRAIN_LENGTH = 256;	// length of the player trail
	const int PIXEL_SIZE = 2;		// size of each pixel

	Maze demo(MAZE_WIDTH, MAZE_HEIGHT, TIME_STEP, TRAIN_LENGTH);
	if (demo.Construct(demo.mazeRealWidth, demo.mazeRealHeight, PIXEL_SIZE, PIXEL_SIZE))
		demo.Start();

	return 0;
}