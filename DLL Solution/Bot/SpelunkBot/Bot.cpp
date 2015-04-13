//Rasmus, a Spelunkbot programmed by Samuel Kinnett

// SpelunkBot.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "Bot.h"
#include <math.h>

#pragma region Defines

// Use for functions that take either pixel or node coordinates
#define NODE_COORDS 0
#define PIXEL_COORDS 1

// Nodes in the x and y axes
#define Y_NODES 34
#define X_NODES 42

// Number of pixels in each node
#define PIXELS_IN_NODES 16

// Variable types - useful for when calling UpdatePlayerVariables()
#define BOOLEAN 0
#define DOUBLE 1
#define STRING 2

// Directions, used for naviagtion
#define LEFT 0
#define RIGHT 1

#pragma endregion
 
#pragma region Global Variables

bool canGoLeft;			// Can the bot go left?
bool canGoRight;		// Can the bot go right?
bool canJumpLeft;		// Can the bot jump left?
bool canJumpRight;		// Can the bot jump right?
bool canJumpGrabLeft;	// Can the bot jump left and grab a ledge?
bool canJumpGrabRight;	// Can the bot jump right and grab a ledge?

bool goalCurrentlyReachable;	// Used to detect if the goal can currently be reached. If the fog map has not been properly cleared, for example, it may not be possible to pathfind to the exit.

int jumpTimer;			// Used to ensure the bot holds the jump key to climb ledges.

bool walkSpikeLeft;		// Will the bot hit a spike if he moves left?
bool walkSpikeRight;	// Will the bot hit a spike if he moves right? 
bool jumpSpikeLeft;		// Will the bot hit a spike if he jumps left?
bool jumpSpikeRight;	// Will the bot hit a spike if he moves right?

bool isClimbing;		// Is the player wallclimbing?
bool isJumping;			// Is the player jumping?
int ticks;				// Used to manage repeated pathfinding
int targetX;			// The current X co-ordinate the bot is trying to reach
int targetY;			// The current Y co-ordinate the bot is trying to reach

int outputTicks;		// To stop the constant fucking stream of console updates

int visitedSquares[X_NODES][Y_NODES];	// Stores what squares the bot has visited
int goldSquares[X_NODES][Y_NODES];		// Stores what squares contain gold
int reachableSquares[X_NODES][Y_NODES]; // Stores what gold squares the bot can reach
int refreshTimer;						// Used to refresh the reachable gold squares on a regular basis

#pragma endregion

#pragma region Setup

SPELUNKBOT_API double Initialise(void)
{
	_targetX = 0;
	_targetY = 0;
	_pathCount = 0;
	_tempID = 0;
	_waitTimer = 0;
	_playerPositionX = 0;
	_playerPositionY = 0;
	_playerPositionXNode = 0;
	_playerPositionYNode = 0;
	_hasGoal = false;
	_spIsInAir = false;
	_spIsJetpacking = false;
	_itemGoal = false;
	_fogGoal = true;
	_endGoal = false;
	_headingRight = false;
	_headingLeft = false;
	_goRight = false;
	_goLeft = false;
	_jump = false;
	_attack = false;

	goalCurrentlyReachable = false;

	refreshTimer = 0;

	isJumping = false;
	return 1;
}

#pragma endregion

#pragma region Bot Logic

SPELUNKBOT_API double Update(double posX, double posY)
{

	#pragma region Update Variables

	//ResetBotVariables();

	// Store bot positions
	double pixelPosX = posX * 16;
	double pixelPosY = posY * 16;

	// Update visited squares
	visitedSquares[(int)posX][(int)posY] = 1;

	// Output position
	if (outputTicks == 0)
	{
		std::cout << "X = " << posX << "\t" << "Y = " << posY << std::endl;
		outputTicks = 5;
	}
	else
		--outputTicks;

	// Reset goal if target reached
	if (_hasGoal == true)
	{
		if (floor(posX) == targetX && floor(posY) == targetY)
		{
			_hasGoal = false;
			std::cout << "Goal Reached" << std::endl;
			targetX = 0;
			targetY = 0;
			goldSquares[(int)posX][(int)posY] = 0;
			return 1;
		}
	}

	// Refresh reachable squares if necessary

	if (refreshTimer == 0)
	{
		for (int y = 0; y < Y_NODES; y++)
		{
			for (int x = 0; x < X_NODES; x++)
			{
				reachableSquares[x][y] = 0;
			}
		}

		refreshTimer = 30;
	}

	UpdateMovementVariables(posX, posY, pixelPosX, pixelPosY);
	//UpdateStatusVariables();
	PopulateGoldMap();

	if (jumpTimer == 0)
	{
		_jump = false;
		/*
		if (_hasGoal)
		{
			Pathfind(posX, posY, targetX, targetY);
			ticks = 30;
		}
		*/
		jumpTimer = -1;
	}
	else
		jumpTimer--;

	#pragma endregion

	#pragma region Wait for Events

	if (IsEnemyInNode(posX + 1, posY, NODE_COORDS) || IsEnemyInNode(posX - 1, posY, NODE_COORDS))
	{
		_attack = true;
		_waitTimer = 5;
	}
	else
	{
		_attack = false;
	}

	if (_waitTimer > 0)
	{
		if (isClimbing)
		{
			if (_headingRight)
				_goRight = true;
			else
				_goLeft = true;
				_jump = true;
			if (_waitTimer == 1)
				isClimbing = false;

			_waitTimer--;

			return 1;
		}

		if (_jump == false && !_spIsInAir)
		{
			_goLeft = false;
			_goRight = false;
		}

		if (isJumping && !_spIsInAir)
		{
			_waitTimer = 0;
			isJumping = false;
			return 1;
		}
		--_waitTimer;

		return 1;
	}
	else
	{
		if (_hasGoal)
		{
			Pathfind(posX, posY, targetX, targetY);
			ticks = 30;
		}
		_waitTimer = -1;
	}

	#pragma endregion

	#pragma region Navigate

	//Does Apsalus have a goal?
	if (!_hasGoal)
	{
		//If not, let's look for the exit
		if (FindExit(posX, posY))
			return 1;
		//Otherwise, let's look for GOLD!
		if (FindGold(posX, posY))
			return 1;

		//If we haven't found the exit, navigate normally
		Navigate(posX, posY);

		//AttackEnemies
	}
	else
	{
		//if we've reached the target, reset
		if (floor (posX) == targetX && floor (posY) == targetY)
		{
			_hasGoal = false;
			std::cout << "Goal Reached" << std::endl;
			targetX = 0;
			targetY = 0;
			goldSquares[(int)posX][(int)posY] = 0;
			return 1;
		}
		//otherwise, let's move towards the next node in the path
		double tempTargetX = GetNextPathXPos(pixelPosX, pixelPosY, PIXEL_COORDS);
		double tempTargetY = GetNextPathYPos(pixelPosX, pixelPosY, PIXEL_COORDS);

		if (outputTicks % 5 == 0)
			std::cout << "Target X = " << tempTargetX << "\t" << "Target Y = " << tempTargetY << std::endl;

		// First of all, let's check to see if the pathfinding has given us an impossible jump
		if (posX == tempTargetX + 0.5 && posY - tempTargetY + 0.5 >= 2)
		{
			// If it has, we will ignore it for now and try to move into a (hopefully) more advantageous position
			_hasGoal = false;
			Navigate(posX, posY);
		}

		#pragma region Movement Tree

		if (posX < tempTargetX + 0.5)
		{
			//If we're also below the target square, let's prioritise jumping
			if (posY > tempTargetY + 0.5)
			{
				if (canJumpGrabRight && IsJumpSpikeRight && !SquareVisited(posX + 1, posY - 2))
					JumpGrab(RIGHT);
				else if (canGoRight && IsWalkSpikeRight && !SquareVisited(posX + 1, posY))
					Walk(RIGHT);
				else if (canJumpRight && IsJumpSpikeRight && !SquareVisited(posX + 1, posY - 1))
					Jump(RIGHT);
				else
				{
					// If there are no 'new' routes to take
					if (canJumpGrabRight && IsJumpSpikeRight)
						JumpGrab(RIGHT);
					else if (canGoRight && IsWalkSpikeRight)
						Walk(RIGHT);
					else if (canJumpRight && IsJumpSpikeRight)
						Jump(RIGHT);
				}
			}
			else
			{
				//Otherwise, we'll prioritise walking normally.
				if (canGoRight && IsWalkSpikeRight && !SquareVisited(posX + 1, posY))
					Walk(RIGHT);
				else if (canJumpRight && IsJumpSpikeRight && !SquareVisited(posX + 1, posY - 1))
					Jump(RIGHT);
				else if (canJumpGrabRight && IsJumpSpikeRight && !SquareVisited(posX + 1, posY - 2))
					JumpGrab(RIGHT);
				else
				{

					// If there are no 'new' routes to take
					if (canGoRight && IsWalkSpikeRight)
						Walk(RIGHT);
					else if (canJumpRight && IsJumpSpikeRight)
						Jump(RIGHT);
					else if (canJumpGrabRight && IsJumpSpikeRight)
						JumpGrab(RIGHT);
				}
			}
		}
		else
		{
			//If we're also below the target square, let's prioritise jumping
			if (posY > tempTargetY + 0.5)
			{
				if (canJumpGrabRight && IsJumpSpikeRight && !SquareVisited(posX + 1, posY - 2))
					JumpGrab(LEFT);
				else if (canGoRight && IsWalkSpikeRight && !SquareVisited(posX + 1, posY))
					Walk(LEFT);
				else if (canJumpRight && IsJumpSpikeRight && !SquareVisited(posX + 1, posY - 1))
					Jump(LEFT);
				else
				{
					// If there are no 'new' routes to take
					if (canJumpGrabRight && IsJumpSpikeRight)
						JumpGrab(LEFT);
					else if (canGoRight && IsWalkSpikeRight)
						Walk(LEFT);
					else if (canJumpRight && IsJumpSpikeRight)
						Jump(LEFT);
				}
			}
			else
			{
				//Otherwise, we'll prioritise walking normally.
				if (canGoRight && IsWalkSpikeRight && !SquareVisited(posX + 1, posY))
					Walk(LEFT);
				else if (canJumpRight && IsJumpSpikeRight && !SquareVisited(posX + 1, posY - 1))
					Jump(LEFT);
				else if (canJumpGrabRight && IsJumpSpikeRight && !SquareVisited(posX + 1, posY - 2))
					JumpGrab(LEFT);
				else
				{

					// If there are no 'new' routes to take
					if (canGoRight && IsWalkSpikeRight)
						Walk(LEFT);
					else if (canJumpRight && IsJumpSpikeRight)
						Jump(LEFT);
					else if (canJumpGrabRight && IsJumpSpikeRight)
						JumpGrab(LEFT);
				}
			}
		}

		#pragma endregion

		/*
		//jump if necessary
		if (posY < tempTargetY + 0.5)
		{
			_jump = true;
			jumpTimer = 2;
		}
		*/

		//Let's not forget those bastard ladders!
		//Seriously, fuck ladders
		
		if (GetNodeState(posX, posY, NODE_COORDS) == spLadder && posY > tempTargetY + 0.5 && posX == tempTargetX + 0.5)
		{
			_jump = true;
			jumpTimer = 1;
		}
		

		//Finally, update the pathfinding every 30 ticks to prevent the bot getting stuck in stupid places
		if (ticks == 0)
		{
			if (!_spIsInAir)
			{
				Pathfind(posX, posY, targetX, targetY);
				ticks = 30;
			}
			//if the square isn't reachable, let's cancel the pathfinding
			if (!IsSquareReachable(posX, posY, targetX, targetY))
			{
				_hasGoal = false;
			}
		}
		else
		{
			--ticks;
		}
	}

	#pragma endregion

	return 1;
}

// Reset variables as required each frame
void ResetBotVariables(void)
{
	_goRight = false;
	_goLeft = false;
	_jump = false;
	_attack = false;
}

#pragma endregion

#pragma region Update Variables

// This method updates the terrain checking and available movement variables
void UpdateMovementVariables(double posX, double posY, double pixelPosX, double pixelPosY)
{
	walkSpikeLeft = IsWalkSpikeLeft(posX, posY);
	walkSpikeRight = IsWalkSpikeRight(posX, posY);
	jumpSpikeLeft = IsJumpSpikeLeft(posX, posY);
	jumpSpikeRight = IsJumpSpikeRight(posX, posY);

	canGoLeft = CanGoLeft(pixelPosX, pixelPosY);
	canGoRight = CanGoRight(pixelPosX, pixelPosY);
	canJumpLeft = CanJumpLeft(posX, posY);
	canJumpRight = CanJumpRight(posX, posY);
	canJumpGrabLeft = CanJumpGrabLeft(posX, posY);
	canJumpGrabRight = CanJumpGrabRight(posX, posY);

	_spIsInAir = (IsNodePassable(pixelPosX, pixelPosY + 10, PIXEL_COORDS));
}

// This method updates the status variables about Apsalus' environment
void UpdateStatusVariables()
{
	_hasGoal = GetHasGoal();
	_spIsInAir = GetIsInAir();
	_spIsJetpacking = GetIsJetpacking();
	_itemGoal = GetItemGoal();
	_pathCount = GetPathCount();
	_tempID = GetTempID();
	_fogGoal = GetFogGoal();
	_endGoal = GetEndGoal();
	_waitTimer = GetWaitTimer();
	_headingRight = GetHeadingRight();
	_headingLeft = GetHeadingLeft();
}

// This method updates the goldSquares array such that it represents the current gold layout, with 1 being gold and 0 being anything else
void PopulateGoldMap()
{
	for (int y = 0; y < Y_NODES; y++)
	{
		for (int x = 0; x < X_NODES; x++)
		{
			if ((NumberOfCollectableTypeInNode(spGoldBar, x, y, NODE_COORDS) != 0
				|| NumberOfCollectableTypeInNode(spGoldBars, x, y, NODE_COORDS) != 0
				|| NumberOfCollectableTypeInNode(spGoldNugget, x, y, NODE_COORDS) != 0)
				&& visitedSquares[x][y] != 1
				&& IsNodePassable(x, y, NODE_COORDS))

				goldSquares[x][y] = 1;

			else

				goldSquares[x][y] = 0;

		}
	}
}

#pragma endregion

#pragma region Terrain Checks

// NODE_COORDS: Returns true if walking left would result in hitting a spike
bool IsWalkSpikeLeft(double posX, double posY)
{
	return ((GetNodeState(posX - 1, posY, NODE_COORDS) == spSpike)
		|| (GetNodeState(posX - 1, posY + 1, NODE_COORDS) == spSpike)
		|| (GetNodeState(posX - 1, posY + 2, NODE_COORDS) == spSpike));
}

// NODE_COORDS: Returns true if walking right would result in hitting a spike
bool IsWalkSpikeRight(double posX, double posY)
{
	return ((GetNodeState(posX + 1, posY, NODE_COORDS) == spSpike)
		|| (GetNodeState(posX + 1, posY + 1, NODE_COORDS) == spSpike)
		|| (GetNodeState(posX + 1, posY + 2, NODE_COORDS) == spSpike));
}

// NODE_COORDS: Returns true if jumping left would result in hitting a spike
bool IsJumpSpikeLeft(double posX, double posY)
{
	return ((GetNodeState(posX - 3, posY, NODE_COORDS) == spSpike)
		|| (GetNodeState(posX - 3, posY + 1, NODE_COORDS) == spSpike));
}

// NODE_COORDS: Returns true if jumping right would result in hitting a spike
bool IsJumpSpikeRight(double posX, double posY)
{
	return ((GetNodeState(posX + 3, posY, NODE_COORDS) == spSpike)
		|| (GetNodeState(posX + 3, posY + 1, NODE_COORDS) == spSpike));
}

// PIXEL_COORDS: Returns true if the bot can walk left
bool CanGoLeft(double posX, double posY)
{
	return (IsNodePassable(posX - 16, posY, PIXEL_COORDS)
		&& !IsNodePassable(posX - 16, posY + 16, PIXEL_COORDS));
	//Uses pixel coords for improved accuracy
}

// PIXEL_COORDS: Returns true if the bot can walk right
bool CanGoRight(double posX, double posY)
{
	return (IsNodePassable(posX + 16, posY, PIXEL_COORDS)
		&& !IsNodePassable(posX + 16, posY + 16, PIXEL_COORDS));
	//Uses pixel coords for improved accuracy
}

// Returns true if the bot can jump left
bool CanJumpLeft(double posX, double posY)
{
	return (IsNodePassable(posX - 1, posY - 1, NODE_COORDS)
		|| (IsNodePassable(posX - 1, posY, NODE_COORDS) && IsNodePassable(posX - 2, posY, NODE_COORDS)));
}

// Returns true if the bot can jump right
bool CanJumpRight(double posX, double posY)
{
	return (IsNodePassable(posX + 1, posY - 1, NODE_COORDS)
		|| (IsNodePassable(posX + 1, posY, NODE_COORDS) && IsNodePassable(posX + 2, posY, NODE_COORDS)));
}

// Returns true if the bot can jump and grab a ledge to the left
bool CanJumpGrabLeft(double posX, double posY)
{
	return (IsNodePassable(posX - 1, posY - 2, NODE_COORDS) && !IsNodePassable(posX - 1, posY - 1, NODE_COORDS) && IsNodePassable(posX, posY - 1, NODE_COORDS));
}

// Returns true if the bot can jump and grab a ledge to the right
bool CanJumpGrabRight(double posX, double posY)
{
	return (IsNodePassable(posX + 1, posY - 2, NODE_COORDS) && !IsNodePassable(posX + 1, posY - 1, NODE_COORDS) && IsNodePassable(posX, posY - 1, NODE_COORDS));
}

// Returns true if the square at the passed in co-ordinates has previously been visited
bool SquareVisited(double posX, double posY)
{
	return (visitedSquares[(int)posX][(int)posY] == 1);
}

// Simulates pathfinding to determine whether a square can be reached by the bot
bool IsSquareReachable(double posX, double posY, double targetX, double targetY)
{
	double tempNodeX = posX;
	double tempNodeY = posY;
	double nextNodeX;
	double nextNodeY;

	// distanceY stores the current difference between two points in the Y axis to try and identify impossible climbs.
	int distanceY = 0;
	// vertical is true if the path goes vertically upwards
	bool vertical = false;

	// first, we pathfind to the target square
	Pathfind(tempNodeX, tempNodeY, targetX, targetY);

	std::cout << "calculating if target (" << targetX << "," << targetY << ") is reachable:" << std::endl;

	// now, we simulate moving along the path
	while (tempNodeX != targetX && tempNodeY != targetY)
	{
		nextNodeX = GetNextPathXPos(tempNodeX, tempNodeY, NODE_COORDS);
		nextNodeY = GetNextPathYPos(tempNodeX, tempNodeY, NODE_COORDS);
		
		if (tempNodeX == nextNodeX && tempNodeY == nextNodeY)
		{
			std::cout << "fog hasn't been fully revealed, square not currently reachable" << std::endl;
			reachableSquares[(int)targetX][(int)targetY] = 2;
			return false;
		}
		
		if (nextNodeX == tempNodeX)
		{
			vertical = true;
			distanceY += (tempNodeY - nextNodeY);
			if (distanceY >= 2)
			{
				std::cout << "target unreachable." << std::endl;
				reachableSquares[(int)targetX][(int)targetY] = 2;
				return false;
			}
		}
		else
		{
			vertical = false;
			distanceY = 0;
		}

		tempNodeX = nextNodeX;
		tempNodeY = nextNodeY;

		std::cout << "current test node: (" << tempNodeX << "," << tempNodeY << ")" << std::endl;
	}

	std::cout << "target reached." << std::endl;
	return true;
}

#pragma endregion

#pragma region Navigation

// This method calls the pathfinding algorithm and updates the ticks variable
void Pathfind(double posX, double posY, double x, double y)
{
	ticks = 16;
	_hasGoal = true;
	CalculatePathFromXYtoXY(posX, posY, x, y, NODE_COORDS);
}

// This method decides how the bot should move to navigate the environment
void Navigate(double posX, double posY)
{
	if (_headingLeft)
	{
		if (canJumpGrabLeft && IsJumpSpikeLeft && !SquareVisited(posX - 1, posY - 2))
			JumpGrab(LEFT);
		else if (canGoLeft && IsWalkSpikeLeft && !SquareVisited(posX - 1, posY))
			Walk(LEFT);
		else if (canJumpLeft && IsJumpSpikeLeft && !SquareVisited(posX - 1, posY - 1))
			Jump(LEFT);
		else
		{
			// If there are no 'new' routes to take
			if (canJumpGrabLeft && IsJumpSpikeLeft)
				JumpGrab(LEFT);
			else if (canGoLeft && IsWalkSpikeLeft)
				Walk(LEFT);
			else if (canJumpLeft && IsJumpSpikeLeft)
				Jump(LEFT);
			else
				ChangeDirection(LEFT);
		}
	}
	else if (_headingRight)
	{
		if (canJumpGrabRight && IsJumpSpikeRight && !SquareVisited(posX + 1, posY - 2))
			JumpGrab(RIGHT);
		else if (canGoRight && IsWalkSpikeRight && !SquareVisited(posX + 1, posY))
			Walk(RIGHT);
		else if (canJumpRight && IsJumpSpikeRight && !SquareVisited(posX + 1, posY - 1))
			Jump(RIGHT);
		//TODO: Check for enemies
		else
		{
			// If there are no 'new' routes to take
			if (canJumpGrabRight && IsJumpSpikeRight)
				JumpGrab(RIGHT);
			else if (canGoRight && IsWalkSpikeRight)
				Walk(RIGHT);
			else if (canJumpRight && IsJumpSpikeRight)
				Jump(RIGHT);
			//TODO: Check for enemies
			else
				ChangeDirection(RIGHT);
		}
	}
	else
	{
		if (canGoLeft || canJumpLeft || canJumpGrabLeft)
			_headingLeft = true;
		else
			_headingRight = true;
	}
}

// This method searches for the exit. If it finds the exit it will return true, otherwise it will return false
bool FindExit(double posX, double posY)
{
	for (int y = 0; y < Y_NODES; y++)
	{
		for (int x = 0; x < X_NODES; x++)
		{
			if (GetNodeState(x, y, NODE_COORDS) == spExit)
			{
				//If we have found the exit, pathfind to it
				std::cout << "Exit found!" << std::endl;
				if (IsSquareReachable(posX, posY, x, y))
				{
					targetX = x;
					targetY = y;
					Pathfind(posX, posY, targetX, targetY);
					return true;
				}
				else
				{
					std::cout << "Exit not currently reachable." << std::endl;
					return false;
				}
			}
		}
	}

	return false;
}

// This method searches for the nearest gold. If it finds any it will return true, otherwise it will return false
bool FindGold(double posX, double posY)
{
	int currentClosestX;
	int currentClosestY;
	int currentDistance = -1;
	double distance;
	bool goldFound = false;

	// Calculate which gold pieces are reachable

	for (int y = 0; y < Y_NODES; y++)
	{
		for (int x = 0; x < X_NODES; x++)
		{
			// If the square contains gold and has not yet been proven to be unreachable
			if (reachableSquares[x][y] != 1 && goldSquares[x][y] == 1)
			{
				if (IsSquareReachable(posX, posY, x, y))
					reachableSquares[x][y] = 2; // A value of 2 indicates that the square contains gold and is reachable
				else
					reachableSquares[x][y] = 1; // A value of 1 indicates that the square contains gold but is not reachable
			}
		}
	}

	for (int y = 0; y < Y_NODES; y++)
	{
		for (int x = 0; x < X_NODES; x++)
		{
			if (goldSquares[x][y] == 1 && reachableSquares[x][y] == 2)
			{
				goldFound = true;

				distance = sqrt(pow(posX - x, 2) + pow(posY - y, 2));

				if (currentDistance == -1)
				{
					currentClosestX = x;
					currentClosestY = y;
					currentDistance = distance;
				}
				else if (distance < currentDistance)
				{
					currentClosestX = x;
					currentClosestY = y;
					currentDistance = distance;
				}
			}
		}
	}

	if (goldFound)
	{
		std::cout << "Gold Found at X = " << targetX << " Y = " << targetY << std::endl;

		_hasGoal = true;
		targetX = currentClosestX;
		targetY = currentClosestY;
		Pathfind(posX, posY, targetX, targetY);
		return true;
	}

	return false;
}

#pragma endregion

#pragma region Movement

// This method allows the bot to walk in the required direction
void Walk(double direction)
{
	if (direction == RIGHT)
	{
		//std::cout << "Walking right" << std::endl;
		//move right
		_headingLeft = false;
		_headingRight = true;
		_goRight = true;
		_goLeft = false;
	}
	else
	{
		//std::cout << "Walking left" << std::endl;
		//move left
		_headingRight = false;
		_headingLeft = true;
		_goLeft = true;
		_goRight = false;
	}
}

// This method allows the bot to jump in the required direction
void Jump(double direction)
{
	if (direction == RIGHT)
	{
		//std::cout << "Jumping right" << std::endl;
		//jump to the right
		_headingLeft = false;
		_headingRight = true;
		_goRight = true;
		_goLeft = false;
		_jump = true;

		isJumping = true;
		jumpTimer = 1;
		_waitTimer = 20;
	}
	else
	{
		//std::cout << "Jumping left" << std::endl;
		//jump to the left
		_headingRight = false;
		_headingLeft = true;
		_goLeft = true;
		_goRight = false;
		_jump = true;

		isJumping = true;
		jumpTimer = 1;
		_waitTimer = 20;
	}
}

// This method allows the bot to jump in the required direction and climb up a ledge
void JumpGrab(double direction)
{
	if (direction == RIGHT)
	{
		//std::cout << "Jump grabbing right" << std::endl;
		//jump grab to the right
		_headingLeft = false;
		_headingRight = true;
		_goRight = true;
		_goLeft = false;
		_jump = true;
		isClimbing = true;
		_waitTimer = 3;
	}
	else
	{
		//std::cout << "Jump grabbing left" << std::endl;
		//jump grab to the left
		_headingRight = false;
		_headingLeft = true;
		_goLeft = true;
		_goRight = false;
		_jump = true;
		isClimbing = true;
		_waitTimer = 3;
	}
}

// This method changes the bots direction to the opposite of the direction passed into the method
void ChangeDirection(double direction)
{
	if (direction == RIGHT)
	{
		//We can't head right, let's change our direction.
		//std::cout << "Changing direction from right to left" << std::endl;
		_headingRight = false;
		_headingLeft = true;
		_goLeft = true;
		_goRight = false;
	}
	else
	{
		//We can't head left, let's change our direction.
		//std::cout << "Changing direction from left to right" << std::endl;
		_headingLeft = false;
		_headingRight = true;
		_goRight = true;
		_goLeft = false;
	}
}

#pragma endregion

#pragma region Get functions for GM

double ConvertBoolToDouble(bool valToConvert)
{
	if (valToConvert)
	{
		return 1;
	}
	return 0;
}
char* ConvertBoolToChar(bool valToConvert)
{
	if (valToConvert)
	{
		return "1";
	}
	return "0";
}
SPELUNKBOT_API double GetHasGoal(void)
{
	return ConvertBoolToDouble(_hasGoal);
}
SPELUNKBOT_API double GetIsInAir(void)
{
	return ConvertBoolToDouble(_spIsInAir);
}
SPELUNKBOT_API double GetIsJetpacking(void)
{
	return ConvertBoolToDouble(_spIsJetpacking);
}
SPELUNKBOT_API double GetItemGoal(void)
{
	return ConvertBoolToDouble(_itemGoal);
}
SPELUNKBOT_API double GetPathCount(void)
{
	return _pathCount;
}
SPELUNKBOT_API double GetTempID(void)
{
	return _tempID;
}
SPELUNKBOT_API double GetFogGoal(void)
{
	return ConvertBoolToDouble(_fogGoal);
}
SPELUNKBOT_API double GetEndGoal(void)
{
	return ConvertBoolToDouble(_endGoal);
}
SPELUNKBOT_API double GetWaitTimer(void)
{
	return _waitTimer;
}
SPELUNKBOT_API double GetHeadingRight(void)
{
	return ConvertBoolToDouble(_headingRight);
}
SPELUNKBOT_API double GetHeadingLeft(void)
{
	return ConvertBoolToDouble(_headingLeft);
}
SPELUNKBOT_API double GetGoRight(void)
{
	return ConvertBoolToDouble(_goRight);
}
SPELUNKBOT_API double GetGoLeft(void)
{
	return ConvertBoolToDouble(_goLeft);
}
SPELUNKBOT_API double GetJump(void)
{
	return ConvertBoolToDouble(_jump);
}
SPELUNKBOT_API double GetTargetX(void)
{
	return _targetX;
}
SPELUNKBOT_API double GetTargetY(void)
{
	return _targetY;
}
SPELUNKBOT_API double GetAttack(void)
{
	return ConvertBoolToDouble(_attack);
}

#pragma endregion