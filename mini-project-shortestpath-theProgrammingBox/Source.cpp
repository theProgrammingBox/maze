#include <iostream>
#include <stack>

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

using olc::vi2d;
using olc::Pixel;
using std::vector;

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
	vector<vi2d> playerTrail;	// completely cosmetics
	
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
	}

	~Maze()
	{
		delete[] maze;
		delete[] mazeDirections;
	}

	void GenerateMaze()
	{
		memset(maze, 0, sizeof(uint8_t) * mazeRealWidth * mazeRealHeight);		// set all cells to no path
		memset(mazeDirections, 0, sizeof(uint8_t) * MAZE_WIDTH * MAZE_HEIGHT);	// set all cells to no connections and not visited
		
		vector<vi2d> stack;
		stack.push_back({ MAZE_WIDTH / 2, MAZE_HEIGHT / 2 });						// start at the middle of the maze
		mazeDirections[stack.back().x + stack.back().y * MAZE_WIDTH] |= VISITED;	// mark the middle node as visited
		
		vi2d nextPos;
		vector<uint8_t> neighbours;
		while (!stack.empty())
		{
			neighbours.clear();
			vi2d current = stack.back();

			for (int i = 0; i < 4; i++)
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
				int direction = neighbours[rand() % neighbours.size()];
				nextPos = current + directions[direction];
				
				mazeDirections[current.y * MAZE_WIDTH + current.x] |= (1 << direction);				// set the direction bit to 1, reference MazeBits
				mazeDirections[nextPos.y * MAZE_WIDTH + nextPos.x] |= (1 << (direction + 2) % 4);	// opposite direction, loop around the direction bit
				mazeDirections[nextPos.y * MAZE_WIDTH + nextPos.x] |= VISITED;	// set the new cell as visited
				stack.push_back(nextPos);										// add the new cell to the stack
			}
		}
		
		int mazex;
		int mazey;
		for (int y = 0; y < MAZE_HEIGHT; y++)
		{
			for (int x = 0; x < MAZE_WIDTH; x++)
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

		do
		{	// randomize player position
			playerPosition = { rand() % mazeRealWidth, rand() % mazeRealHeight };
		} while (!(maze[playerPosition.y * mazeRealWidth + playerPosition.x] & PATH));
		
		do
		{	// randomize goal position
			goalPosition = { rand() % mazeRealWidth, rand() % mazeRealHeight };
		} while (!(maze[goalPosition.y * mazeRealWidth + goalPosition.x] & PATH));

		playerTrail.clear();
	}

	void FindShortestPath()	// DFS method
	{
		shortestPath.clear();
		for (int i = 0; i < mazeRealWidth * mazeRealHeight; i++)
			maze[i] &= ~VISITED;	// make all spots unvisited
		
		DFS(playerPosition, goalPosition);
	}

	bool DFS(vi2d current, vi2d goal)
	{
		if (current == goal)	// end once the goal is found
		{
			shortestPath.push_back(current);
			return true;
		}

		maze[current.y * mazeRealWidth + current.x] |= VISITED;	// mark cell is visited
		
		vi2d nextPos;
		for (int i = 0; i < 4; i++)	// dps all neighboring cells
		{
			nextPos = current + directions[i];
			if (nextPos.x >= 0 && nextPos.x < mazeRealWidth && nextPos.y >= 0 && nextPos.y < mazeRealHeight && (maze[nextPos.y * mazeRealWidth + nextPos.x] & PATH) && !(maze[nextPos.y * mazeRealWidth + nextPos.x] & VISITED))
			{
				if (DFS(nextPos, goal))
				{
					shortestPath.push_back(current);
					return true;
				}
			}
		}

		return false;
	}

	void FindShortestPath2()	// stack method
	{
		shortestPath.clear();
		for (int i = 0; i < mazeRealWidth * mazeRealHeight; i++)
			maze[i] &= ~VISITED;	// make all spots unvisited
		
		shortestPath.push_back(playerPosition);
		maze[playerPosition.y * mazeRealWidth + playerPosition.x] |= VISITED;	// mark cell is visited
		
		vi2d nextPos;
		while (!shortestPath.empty())
		{
			vi2d current = shortestPath.back();
			shortestPath.pop_back();

			if (current == goalPosition)	// end once the goal is found
			{
				shortestPath.push_back(current);
				break;
			}

			for (int i = 0; i < 4; i++)	// dps all neighboring cells
			{
				nextPos = current + directions[i];
				if (nextPos.x >= 0 && nextPos.x < mazeRealWidth && nextPos.y >= 0 && nextPos.y < mazeRealHeight && (maze[nextPos.y * mazeRealWidth + nextPos.x] & PATH) && !(maze[nextPos.y * mazeRealWidth + nextPos.x] & VISITED))
				{
					shortestPath.push_back(nextPos);
					maze[nextPos.y * mazeRealWidth + nextPos.x] |= VISITED;	// mark cell is visited
				}
			}
		}
	}
	
	bool OnUserCreate()
	{
		GenerateMaze();		// generate the maze directions, fill the maze array, and randomize player and goal
		FindShortestPath();	// find the shortest path from player to goal
		
		return true;
	}

	bool OnUserUpdate(float fElapsedTime)
	{
		if (GetKey(olc::SPACE).bPressed)
		{
			GenerateMaze();		// generate the maze directions, fill the maze array, and randomize player and goal
			FindShortestPath();	// find the shortest path from player to goal
			Clear(olc::BLACK);	// clear the screen
		}
		
		for (int y = 0; y < mazeRealHeight; y++)	// draw the maze
			for (int x = 0; x < mazeRealWidth; x++)
				if (maze[y * mazeRealWidth + x])
					Draw(x, y);
		
		for (int i = 0; i < shortestPath.size(); i++)	// draw the shortest path
			Draw(shortestPath[i], Pixel(200,200,255));	// rgb
		
		Draw(goalPosition, olc::RED);		// draw goal

		for (int i = 0; i < TRAIN_LENGTH; i++)	// draw the player trail
		{
			if (i >= playerTrail.size())
				break;
			int color = 255 * (TRAIN_LENGTH - i) / TRAIN_LENGTH;
			Draw(playerTrail[i], Pixel(color, 255, color));	// rgb
		}

		if (playerPosition != goalPosition)
		{
			timer += fElapsedTime;
			if (timer >= TIME_STEP)	// move towards the goal every TIME_STEP ms
			{
				timer -= TIME_STEP;
				playerTrail.push_back(playerPosition);
				if (playerTrail.size() > TRAIN_LENGTH)
					playerTrail.erase(playerTrail.begin());
				shortestPath.pop_back();
				playerPosition = shortestPath.back();
			}
		}
		else
		{
			do
			{	// randomize the goal
				goalPosition = { rand() % mazeRealWidth, rand() % mazeRealHeight };
			} while (!(maze[goalPosition.y * mazeRealWidth + goalPosition.x] & PATH));
			
			FindShortestPath();	// find the shortest path from player to goal
		}
		
		return true;
	}
};

int main()
{
	const int MAZE_WIDTH = 128;		// width of the maze
	const int MAZE_HEIGHT = 64;		// height of the maze
	const float TIME_STEP = 0.002f;	// time (ms) between each action
	const int TRAIN_LENGTH = 128;	// length of the player trail
	const int PIXEL_SIZE = 6;		// size of each pixel

	Maze demo(MAZE_WIDTH, MAZE_HEIGHT, TIME_STEP, TRAIN_LENGTH);
	if (demo.Construct(demo.mazeRealWidth, demo.mazeRealHeight, PIXEL_SIZE, PIXEL_SIZE))
		demo.Start();

	return 0;
}