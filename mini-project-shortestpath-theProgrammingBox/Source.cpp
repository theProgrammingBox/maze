#include <iostream>
#include <stack>

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

using olc::vi2d;
using std::vector;

class Maze : public olc::PixelGameEngine
{
public:
	int mazeWidth;
	int mazeHeight;

	int mazeRealWidth;
	int mazeRealHeight;

	int mazeRealWidthLimit;
	int mazeRealHeightLimit;
	
	uint8_t* maze;				// physical maze, 2x2 cells
	uint8_t* mazeDirections;	// open directions from each cell to its neighbours
	
	vi2d playerPosition;	// same as a pair, stores x and y coordinates
	vi2d goalPosition;		// same as a pair, stores x and y coordinates
	
	const vi2d directions[4] = { {0, 1}, {-1, 0}, {0, -1}, {1, 0} };

	enum MazeBits
	{
		UP = 0x01,		// 0000 0001
		LEFT = 0x02,	// 0000 0010
		DOWN = 0x04,	// 0000 0100
		RIGHT = 0x08,	// 0000 1000
		VISITED = 0x10,	// 0001 0000
		PATH = 0x20		// 0010 0000
	};

	vector<vi2d> shortestPath;	// DFS result;
	float timer = 0.0f;
	float timeStep;
	
	Maze(int mazeWidth, int mazeHeight, float timeStep)
	{
		sAppName = "Maze";
		
		this->mazeWidth = mazeWidth;
		this->mazeHeight = mazeHeight;
		this->timeStep = timeStep;

		mazeRealWidth = mazeWidth * 2;
		mazeRealHeight = mazeHeight * 2;
		
		maze = new uint8_t[mazeRealWidth * mazeRealHeight];		// 2x2 cells, 1 = path, 0 = no path
		mazeDirections = new uint8_t[mazeWidth * mazeHeight];	// each cell describes if it is up, left, down, right, and visited
	}

	~Maze()
	{
		delete[] maze;
	}

	void GenerateMaze()
	{
		// clear paths and connections
		memset(maze, 0, sizeof(uint8_t) * mazeRealWidth * mazeRealHeight);		// set all cells to no path
		memset(mazeDirections, 0, sizeof(uint8_t) * mazeWidth * mazeHeight);	// set all cells to no connections and not visited
		
		vector<vi2d> stack;
		stack.push_back({ 0, 0 });
		mazeDirections[0] |= VISITED;
		
		while (!stack.empty())
		{
			vi2d current = stack.back();

			vi2d nextPos;
			vector<uint8_t> neighbours;

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
				
				mazeDirections[current.y * mazeWidth + current.x] |= (1 << direction);				// set the direction bit to 1
				mazeDirections[nextPos.y * mazeWidth + nextPos.x] |= (1 << (direction + 2) % 4);	// opposite direction
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
				mazex = x * 2;
				mazey = y * 2;
				
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

		//randomize player and goal
		do
		{
			playerPosition = { rand() % mazeRealWidth, rand() % mazeRealHeight };
		} while (!(maze[playerPosition.y * mazeRealWidth + playerPosition.x] & PATH));
		
		do
		{
			goalPosition = { rand() % mazeRealWidth, rand() % mazeRealHeight };
		} while (!(maze[goalPosition.y * mazeRealWidth + goalPosition.x] & PATH));
	}

	// use DFS to find the shortest path, return the path
	void FindShortestPath()
	{
		shortestPath.clear();
		for (int i = 0; i < mazeRealWidth * mazeRealHeight; i++)
			maze[i] &= ~VISITED;	// make all spots unvisited
		
		DFS(playerPosition, goalPosition);
	}

	// use DFS to find the shortest path, return the path
	bool DFS(vi2d current, vi2d goal)
	{
		if (current == goal)
		{
			shortestPath.push_back(current);
			return true;
		}

		maze[current.y * mazeRealWidth + current.x] |= VISITED;
		
		vi2d nextPos;
		for (int i = 0; i < 4; i++)
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
		GenerateMaze();		// generate the maze directions and fill the maze array
		FindShortestPath();	// find the shortest path from player to goal
		
		return true;
	}

	bool OnUserUpdate(float fElapsedTime)
	{
		if (GetKey(olc::SPACE).bPressed)
		{
			GenerateMaze();		// generate the maze directions and fill the maze array
		}
		
		Clear(olc::BLACK);	// clear the screen
		
		// draw the maze
		for (int y = 0; y < mazeRealHeight; y++)
			for (int x = 0; x < mazeRealWidth; x++)
				if (maze[y * mazeRealWidth + x])
					Draw(x, y);
		
		// draw the shortest path
		for (int i = 0; i < shortestPath.size(); i++)
		{
			Draw(shortestPath[i], olc::BLUE);
		}
		
		// draw the player and goal
		Draw(playerPosition, olc::GREEN);
		Draw(goalPosition, olc::RED);

		//move the player based on the next cell in the shortest path
		if (playerPosition != goalPosition)
		{
			timer += fElapsedTime;
			if (timer >= timeStep)
			{
				timer -= timeStep;
				shortestPath.pop_back();
				playerPosition = shortestPath.back();
			}
		}
		else
		{
			do
			{
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

	Maze demo(mazeWidth, mazeHeight, timeStep);
	if (demo.Construct(demo.mazeRealWidth, demo.mazeRealHeight, 16, 16))
		demo.Start();

	return 0;
}