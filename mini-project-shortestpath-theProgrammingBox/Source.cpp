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
	int mazeWidth;			// Width of the maze, number of "nodes"
	int mazeHeight;			// Height of the maze, number of "nodes"

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
	float timeStep;				// time between each DFS step

	int trailLength;			// length of player trail
	vector<vi2d> playerTrail;	// completely cosmetics
	
	Maze(int mazeWidth, int mazeHeight, float timeStep, int trailLength)
	{
		sAppName = "Maze Genertor and Solver";
		
		this->mazeWidth = mazeWidth;
		this->mazeHeight = mazeHeight;
		this->timeStep = timeStep;
		this->trailLength = trailLength;

		mazeRealWidth = mazeWidth * 2;		// each node contains the main path and side paths connecting to its neighbours
		mazeRealHeight = mazeHeight * 2;	// each node contains the main path and side paths connecting to its neighbours
		
		maze = new uint8_t[mazeRealWidth * mazeRealHeight];		// 2x2 nodes, each cell describes a path/wall and if it has been visited during solving
		mazeDirections = new uint8_t[mazeWidth * mazeHeight];	// each cell describes if it is up, left, down, right, and if it has been visited during generating
	}

	~Maze()
	{
		delete[] maze;
		delete[] mazeDirections;
	}

	void GenerateMaze()
	{
		memset(maze, 0, sizeof(uint8_t) * mazeRealWidth * mazeRealHeight);		// set all cells to no path
		memset(mazeDirections, 0, sizeof(uint8_t) * mazeWidth * mazeHeight);	// set all cells to no connections and not visited
		
		vector<vi2d> stack;
		stack.push_back({ mazeWidth / 2, mazeHeight / 2 });						// start at the middle of the maze
		mazeDirections[stack.back().x + stack.back().y * mazeWidth] |= VISITED;	// mark the middle node as visited
		
		vi2d nextPos;
		vector<uint8_t> neighbours;
		while (!stack.empty())
		{
			neighbours.clear();
			vi2d current = stack.back();

			for (int i = 0; i < 4; i++)
			{
				nextPos = current + directions[i];
				if (nextPos.x >= 0 && nextPos.x < mazeWidth && nextPos.y >= 0 && nextPos.y < mazeHeight && !(mazeDirections[nextPos.y * mazeWidth + nextPos.x] & VISITED))
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
				
				mazeDirections[current.y * mazeWidth + current.x] |= (1 << direction);				// set the direction bit to 1, reference MazeBits
				mazeDirections[nextPos.y * mazeWidth + nextPos.x] |= (1 << (direction + 2) % 4);	// opposite direction, loop around the direction bit
				mazeDirections[nextPos.y * mazeWidth + nextPos.x] |= VISITED;	// set the new cell as visited
				stack.push_back(nextPos);										// add the new cell to the stack
			}
		}
		
		int mazex;
		int mazey;
		for (int y = 0; y < mazeHeight; y++)
		{
			for (int x = 0; x < mazeWidth; x++)
			{
				mazex = x << 1;	// convert to cell space
				mazey = y << 1;	// convert to cell space
				
				maze[mazey * mazeRealWidth + mazex] |= PATH;			// set the center cell to path
				
				if (mazeDirections[y * mazeWidth + x] & UP)
				{
					maze[(mazey + 1) * mazeRealWidth + mazex] |= PATH;	// set the top cell to path
				}
				if (mazeDirections[y * mazeWidth + x] & RIGHT)
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
		}
		
		for (int y = 0; y < mazeRealHeight; y++)	// draw the maze
			for (int x = 0; x < mazeRealWidth; x++)
				if (maze[y * mazeRealWidth + x])
					Draw(x, y);
		
		for (int i = 0; i < shortestPath.size(); i++)	// draw the shortest path
			Draw(shortestPath[i], olc::BLUE);
		
		Draw(playerPosition, olc::GREEN);	// draw player
		Draw(goalPosition, olc::RED);		// draw goal

		for (int i = 0; i < trailLength; i++)	// draw the player trail
			Draw(playerTrail[i], Pixel(255 * trailLength / i));

		if (playerPosition != goalPosition)
		{
			timer += fElapsedTime;
			if (timer >= timeStep)	// move towards the goal every timeStep ms
			{
				timer -= timeStep;
				playerTrail.push_back(playerPosition);
				if (playerTrail.size() > trailLength)
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
	const int mazeWidth = 32;
	const int mazeHeight = 16;
	const float timeStep = 0.06f;
	const int trailLength = 10;
	const int pixelSize = 16;

	Maze demo(mazeWidth, mazeHeight, timeStep, trailLength);
	if (demo.Construct(demo.mazeRealWidth, demo.mazeRealHeight, pixelSize, pixelSize))
		demo.Start();

	return 0;
}