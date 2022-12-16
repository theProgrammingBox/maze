#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include <chrono>

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

using std::vector;
using std::queue;
using std::min;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;
using olc::vi2d;
using olc::Pixel;

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
	float* drawingColor;		// purely cosmetic, used to fade between past and current distance colors

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
	float numUpdateFrames;		// numUpdateFrames for the movement animation
	float FPS;					// how many frames to update per second

	int TRAIL_LENGTH;			// length of player trail
	vi2d* playerTrail;			// purely cosmetic, list of previous player positions up to TRAIL_LENGTH positions long
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

		maze = new uint8_t[mazeFilledWidth * mazeFilledHeight];			// 2x2 nodes, each cell describes a path/wall and if it has been visited during solving
		mazeAttributes = new uint8_t[MAZE_WIDTH * MAZE_HEIGHT];			// each cell describes if it is up, left, down, right, and if it has been visited during generating
		distances = new size_t[mazeFilledWidth * mazeFilledHeight];		// each cell describes the distance from the player to that cell
		drawingColor = new float[mazeFilledWidth * mazeFilledHeight];	// purely cosmetic, used to fade between past and current distance colors
		playerTrail = new vi2d[TRAIL_LENGTH];							// a trail behind the player, purely cosmetic

		seed = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
	}

	~Maze()
	{
		delete[] maze;
		delete[] mazeAttributes;
		delete[] distances;
		delete[] drawingColor;
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
		memset(maze, 0, sizeof(uint8_t) * mazeFilledWidth * mazeFilledHeight);	// set all cells to no path
		memset(mazeAttributes, 0, sizeof(uint8_t) * MAZE_WIDTH * MAZE_HEIGHT);	// set all cells to no connections and not visited
		for (int i = mazeFilledWidth * mazeFilledHeight; i--;)					// set all colors to white
			drawingColor[i] = 255;

		vector<vi2d> stack;
		stack.push_back({ MAZE_WIDTH / 2, MAZE_HEIGHT / 2 });					// start at the middle of the maze

		vi2d nextPos;
		vector<uint8_t> neighbours;
		while (!stack.empty())
		{
			neighbours.clear();
			vi2d current = stack.back();
			mazeAttributes[current.x + current.y * MAZE_WIDTH] |= VISITED;		// mark current node as visited

			for (int i = 4; i--;)
			{
				nextPos = current + directions[i];	// get the next position in the direction
				if (nextPos.x >= 0 && nextPos.x < MAZE_WIDTH && nextPos.y >= 0 && nextPos.y < MAZE_HEIGHT && !(mazeAttributes[nextPos.y * MAZE_WIDTH + nextPos.x] & VISITED))
					neighbours.push_back(i);	// if the next position is within the maze and has not been visited, add it to the list of neighbours
			}

			if (neighbours.empty())
				stack.pop_back();	// if there are no neighbours, backtrack
			else
			{
				int direction = neighbours[Rand2() % neighbours.size()];	// pick a random neighbour
				nextPos = current + directions[direction];

				mazeAttributes[current.y * MAZE_WIDTH + current.x] |= (1 << direction);	// set the direction bit to 1, reference MazeBits
				direction += 2;						// get the opposite direction
				direction -= (direction > 3) << 2;	// loop around the byte if the direction is greater RIGHT
				mazeAttributes[nextPos.y * MAZE_WIDTH + nextPos.x] |= (1 << direction);	// set the opposite direction bit to 1, reference MazeBits
				stack.push_back(nextPos);			// add the new cell to the stack
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

				maze[mazey * mazeFilledWidth + mazex] |= PATH;										// set the center cell to path
				if (mazeAttributes[y * MAZE_WIDTH + x] & UP || (Rand2() % MUTATION_RATE == 0))		// if the cell has a path up or if it is a mutation
					maze[(mazey + 1) * mazeFilledWidth + mazex] |= PATH;							// set the top cell to path
				if (mazeAttributes[y * MAZE_WIDTH + x] & RIGHT || (Rand2() % MUTATION_RATE == 0))	// if the cell has a path right or if it is a mutation
					maze[mazey * mazeFilledWidth + mazex + 1] |= PATH;								// set the right cell to path
			}
		}
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

	void FindShortestPath()	// Breadth First Search
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
		queue.push(goalPosition);	// add the goal to the queue

		vi2d current;
		vi2d nextPos;
		while (!queue.empty())
		{
			current = queue.front();
			queue.pop();

			for (int i = 4; i--;)
			{
				nextPos = current + directions[i];	// get the next position in the direction
				if (nextPos.x >= 0 && nextPos.x < mazeFilledWidth && nextPos.y >= 0 && nextPos.y < mazeFilledHeight && distances[nextPos.y * mazeFilledWidth + nextPos.x] == -1 && maze[nextPos.y * mazeFilledWidth + nextPos.x] & PATH)
				{
					distances[nextPos.y * mazeFilledWidth + nextPos.x] = distances[current.y * mazeFilledWidth + current.x] + 1;
					queue.push(nextPos);	// if the next position is within the maze and has not been visited, add it to the list of neighbours
				}
			}
		}

		largestDistance = distances[playerPosition.y * mazeFilledWidth + playerPosition.x];	// set the largest distance to the distance to the player
		shortestPath.resize(largestDistance);	// resize the shortest path vector to the largest distance
		current = playerPosition;				// start at the player position
		for (int i = largestDistance; i--;)
		{
			for (int j = 4; j--;)
			{
				nextPos = current + directions[j];
				if (nextPos.x >= 0 && nextPos.x < mazeFilledWidth && nextPos.y >= 0 && nextPos.y < mazeFilledHeight && distances[nextPos.y * mazeFilledWidth + nextPos.x] == distances[current.y * mazeFilledWidth + current.x] - 1)
					break;				// move to the next position with the lowest distance
			}
			shortestPath[i] = nextPos;	// add the next position to the shortest path
			current = nextPos;			// set the current position to the next position
		}
	}

	void DrawMaze()
	{
		float color;
		for (int x = mazeFilledWidth; x--;)
			for (int y = mazeFilledHeight; y--;)
				if (maze[y * mazeFilledWidth + x])	// if the cell is a path
				{
					color = distances[y * mazeFilledWidth + x] * 255 / (largestDistance + 1) - drawingColor[y * mazeFilledWidth + x];
					color *= 0.006;
					drawingColor[y * mazeFilledWidth + x] = min(255.0f, drawingColor[y * mazeFilledWidth + x] + color);
					Draw(x, y, olc::Pixel(255, drawingColor[y * mazeFilledWidth + x], 255));	// magenta
				}
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

	void NewScene()
	{
		RandomizeMaze();	// randomize the maze
		RandomizePlayer();	// randomize the player position
		RandomizeGoal();	// randomize the goal position
		FindShortestPath();	// find the shortest path to the goal
	}

	void Render()
	{
		Clear(Pixel(0, 0, 0));	// clear the screen with black
		DrawMaze();
		DrawGoalTrail();
		DrawPlayerTrail();
	}

	bool OnUserCreate()
	{
		NewScene();

		return true;
	}

	bool OnUserUpdate(float fElapsedTime)
	{
		if (GetKey(olc::SPACE).bPressed)
			NewScene();							// create a new scene when space is pressed

		numUpdateFrames += fElapsedTime * FPS;	// F / S * S = F
		while (numUpdateFrames > 0)				// while there are frames to update
		{
			Render();							// render the scene
			MovePlayer(fElapsedTime);			// move the player
			numUpdateFrames--;					// subtract a frame
		}

		return true;
	}
};

int main()
{
	const int MAZE_WIDTH = 200;		// width of the maze
	const int MAZE_HEIGHT = 100;	// height of the maze
	const int MUTATION_RATE = 80;	// 1 in 80 chance to flip a cell into a path
	const int WINDOW_WIDTH = 900;	// width of the window
	const int WINDOW_HEIGHT = 500;	// height of the window
	const int PIXEL_SIZE = min(WINDOW_WIDTH / MAZE_WIDTH, WINDOW_HEIGHT / MAZE_HEIGHT);	// size of the pixels

	Maze program(MAZE_WIDTH, MAZE_HEIGHT, MUTATION_RATE);
	if (program.Construct(program.mazeFilledWidth, program.mazeFilledHeight, PIXEL_SIZE, PIXEL_SIZE))
		program.Start();

	return 0;
}