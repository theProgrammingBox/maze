#include <iostream>
#include <queue>
#include <chrono>
#include <algorithm>

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

using olc::vi2d;
using olc::Pixel;
using std::vector;
using std::queue;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;
using std::max;
using std::min;

class Maze : public olc::PixelGameEngine
{
public:
	int MAZE_WIDTH;				// Width of the maze
	int MAZE_HEIGHT;			// Height of the maze
	int MUTATION_RATE;			// chance that a wall gets flipped into a path

	int mazeFilledWidth;		// Width of the maze including the walls
	int mazeFilledHeight;		// Height of the maze including the walls

	size_t largestDistance;		// orthoganal distance from the goal to player
	
	uint8_t* maze;				// maze with walls
	uint8_t* mazeAttributes;	// path directions from each maze component to its neighbours and other attributes
	size_t* distances;			// orthoganal distance from each cell away from the goal
	
	vi2d playerPosition;		// same as a pair, stores x and y coordinates of the player
	vi2d goalPosition;			// same as a pair, stores x and y coordinates of the goal
	
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

	vector<vi2d> shortestPath;	// Breadth First Search result, list of nodes to visit to reach the goal
	float animationDelay;		// animationDelay for the movement animation
	float FPS;					// how many frames to update per second

	int TRAIL_LENGTH;			// length of player trail
	vi2d* playerTrail;			// completely cosmetics, list of previous player positions up to TRAIL_LENGTH positions long
	int trailIndex = 0;			// keeps track of the circular array, instead of using a queue cuz fast

	unsigned int seed;			// seed for the xor random number generator
	
	Maze(int MAZE_WIDTH, int MAZE_HEIGHT, int MUTATION_RATE)
	{
		sAppName = "Maze Generator and Solver";
		
		this->MAZE_WIDTH = MAZE_WIDTH;
		this->MAZE_HEIGHT = MAZE_HEIGHT;
		this->MUTATION_RATE = MUTATION_RATE;

		mazeFilledWidth = MAZE_WIDTH * 2;	// each node contains the main path and side paths connecting to its neighbours, EX: P = PATH	PW	PP	PW
		mazeFilledHeight = MAZE_HEIGHT * 2;	// each node contains the main path and side paths connecting to its neighbours, W = WALL		WW	WW	PW
		
		FPS = mazeFilledWidth + mazeFilledHeight;
		TRAIL_LENGTH = FPS * 0.2;
		
		maze = new uint8_t[mazeFilledWidth * mazeFilledHeight];		// 2x2 nodes, each cell describes a path/wall and if it has been visited during solving
		mazeAttributes = new uint8_t[MAZE_WIDTH * MAZE_HEIGHT];		// each cell describes if it is up, left, down, right, and if it has been visited during generating
		distances = new size_t[mazeFilledWidth * mazeFilledHeight];	// each cell describes the distance from the player to that cell
		playerTrail = new vi2d[TRAIL_LENGTH];						// a trail behind the player, completely cosmetics
		
		seed = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
	}

	~Maze()
	{
		delete[] maze;
		delete[] mazeAttributes;
		delete[] distances;
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
		memset(maze, 0, sizeof(uint8_t) * mazeFilledWidth * mazeFilledHeight);		// set all cells to no path
		memset(mazeAttributes, 0, sizeof(uint8_t) * MAZE_WIDTH * MAZE_HEIGHT);	// set all cells to no connections and not visited

		vector<vi2d> stack;
		stack.push_back({ MAZE_WIDTH / 2, MAZE_HEIGHT / 2 });						// start at the middle of the maze

		vi2d nextPos;
		vector<uint8_t> neighbours;
		while (!stack.empty())
		{
			neighbours.clear();
			vi2d current = stack.back();
			mazeAttributes[current.x + current.y * MAZE_WIDTH] |= VISITED;		// mark current node as visited

			for (int i = 4; i--;)
			{
				nextPos = current + directions[i];
				if (nextPos.x >= 0 && nextPos.x < MAZE_WIDTH && nextPos.y >= 0 && nextPos.y < MAZE_HEIGHT && !(mazeAttributes[nextPos.y * MAZE_WIDTH + nextPos.x] & VISITED))
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

				mazeAttributes[current.y * MAZE_WIDTH + current.x] |= (1 << direction);				// set the direction bit to 1, reference MazeBits
				mazeAttributes[nextPos.y * MAZE_WIDTH + nextPos.x] |= (1 << (direction + 2) % 4);	// opposite direction, loop around the direction bit
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
				maze[mazey * mazeFilledWidth + mazex] |= PATH;	// set the center cell to path
				if (mazeAttributes[y * MAZE_WIDTH + x] & UP)
					maze[(mazey + 1) * mazeFilledWidth + mazex] |= PATH;		// set the top cell to path
				if (mazeAttributes[y * MAZE_WIDTH + x] & RIGHT)
					maze[mazey * mazeFilledWidth + mazex + 1] |= PATH;	// set the right cell to path
			}
		}
		
		for (int i = mazeFilledWidth * mazeFilledHeight; i--;)	// remove some walls so there isn't just a single path
			if (Rand2() % MUTATION_RATE == 0)
				maze[i] |= PATH;
	}

	void RandomizePlayer()
	{
		do
		{	// randomize player position
			playerPosition = { int(Rand2() % mazeFilledWidth), int(Rand2() % mazeFilledHeight) };
		} while (!(maze[playerPosition.y * mazeFilledWidth + playerPosition.x] & PATH || goalPosition == playerPosition));
		
		for (int i = TRAIL_LENGTH; i--;) playerTrail[i] = playerPosition;	// player trail reset
	}

	void RandomizeGoal()
	{
		do
		{	// randomize goal position
			goalPosition = { int(Rand2() % mazeFilledWidth), int(Rand2() % mazeFilledHeight) };
		} while (!(maze[goalPosition.y * mazeFilledWidth + goalPosition.x] & PATH || goalPosition == playerPosition));
	}

	void FindShortestPath()	//BFS
	{
		while (!(maze[playerPosition.y * mazeFilledWidth + playerPosition.x] & PATH))
		{
			RandomizePlayer();	// ensure player is on a path
		}
		while (!(maze[goalPosition.y * mazeFilledWidth + goalPosition.x] & PATH))
		{
			RandomizeGoal();	// ensure goal is on a path
		}
		
		memset(distances, -1, sizeof(size_t) * mazeFilledWidth * mazeFilledHeight);	// set all distances to -1
		distances[goalPosition.y * mazeFilledWidth + goalPosition.x] = 0;			// set the goal distance to 0
		
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
				if (nextPos.x >= 0 && nextPos.x < mazeFilledWidth && nextPos.y >= 0 && nextPos.y < mazeFilledHeight && distances[nextPos.y * mazeFilledWidth + nextPos.x] == -1 && maze[nextPos.y * mazeFilledWidth + nextPos.x] & PATH)
				{
					distances[nextPos.y * mazeFilledWidth + nextPos.x] = distances[current.y * mazeFilledWidth + current.x] + 1;
					queue.push(nextPos);
				}
			}
		}
		
		largestDistance = distances[playerPosition.y * mazeFilledWidth + playerPosition.x];
		shortestPath.resize(largestDistance);
		current = playerPosition;
		for (int i = largestDistance; i--;)
		{
			for (int j = 4; j--;)
			{
				nextPos = current + directions[j];
				if (nextPos.x >= 0 && nextPos.x < mazeFilledWidth && nextPos.y >= 0 && nextPos.y < mazeFilledHeight && distances[nextPos.y * mazeFilledWidth + nextPos.x] == distances[current.y * mazeFilledWidth + current.x] - 1)
					break;
			}
			shortestPath[i] = nextPos;
			current = nextPos;
		}
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
		for (int x = mazeFilledWidth; x--;)
			for (int y = mazeFilledHeight; y--;)
				if (maze[y * mazeFilledWidth + x])	// if the cell is a path
					Draw(x, y, olc::Pixel(255, min(size_t(255), distances[y * mazeFilledWidth + x] * 255 / (largestDistance + 1)), 255));	// megenta
	}

	void DrawGoalTrail()
	{
		for (int i = shortestPath.size(); i--;)
			Draw(shortestPath[i], Pixel(255, 0, 0));	// red
	}

	void DrawPlayerTrail()
	{
		for (int i = TRAIL_LENGTH; i--;)
		{
			Draw(playerTrail[trailIndex++], Pixel(255, i * 255 / TRAIL_LENGTH, 0));	// orange to yellow
			trailIndex -= (trailIndex == TRAIL_LENGTH) * TRAIL_LENGTH;
		}
	}

	void Render()
	{
		Clear(Pixel(0, 0, 0));	// clear the screen with black
		DrawMaze();
		DrawGoalTrail();
		DrawPlayerTrail();
	}

	void MovePlayer(float fElapsedTime)
	{
		if (shortestPath.size() >= 2)
		{
			playerTrail[trailIndex++] = playerPosition;					// add current position to trail
			trailIndex -= (trailIndex >= TRAIL_LENGTH) * TRAIL_LENGTH;	// reset trail index if it goes over the trail length
			shortestPath.pop_back();									// remove the last position from the shortest path
			playerPosition = shortestPath.back();						// set the player position to the last position in the shortest path
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
		
		animationDelay += fElapsedTime * FPS;
		while (animationDelay > 0)
		{
			animationDelay--;
			Render();
			MovePlayer(fElapsedTime);
		}
		
		return true;
	}
};

int main()
{
	const int MAZE_WIDTH = 200;		// width of the maze
	const int MAZE_HEIGHT = 100;	// height of the maze
	const int MUTATION_RATE = 100;	// 1 in 100
	const int PIXEL_SIZE = min(900 / MAZE_WIDTH, 450 / MAZE_HEIGHT);	// size of the pixels

	Maze demo(MAZE_WIDTH, MAZE_HEIGHT, MUTATION_RATE);
	if (demo.Construct(demo.mazeFilledWidth, demo.mazeFilledHeight, PIXEL_SIZE, PIXEL_SIZE))
		demo.Start();

	return 0;
}