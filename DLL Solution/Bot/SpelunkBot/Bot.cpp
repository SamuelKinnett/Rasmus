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

bool canGoLeft;			//Can the bot go left?
bool canGoRight;		//Can the bot go right?
bool canJumpLeft;		//Can the bot jump left?
bool canJumpRight;		//Can the bot jump right?
bool canJumpGrabLeft;	//Can the bot jump left and grab a ledge?
bool canJumpGrabRight;	//Can the bot jump right and grab a ledge?

int jumpTimer;			//used to ensure the bot holds the jump key to climb ledges.

bool walkSpikeLeft;		//Will the bot hit a spike if he moves left?
bool walkSpikeRight;	//Will the bot hit a spike if he moves right? 
bool jumpSpikeLeft;		//Will the bot hit a spike if he jumps left?
bool jumpSpikeRight;	//Will the bot hit a spike if he moves right?

bool isClimbing;		//Is the player wallclimbing?
int ticks;				//Used to manage repeated pathfinding
int targetX;			//The current X co-ordinate the bot is trying to reach
int targetY;			//The current Y co-ordinate the bot is trying to reach

int outputTicks;		//To stop the constant fucking stream of console updates

int visitedSquares[X_NODES][Y_NODES];	//Stores what squares the bot has visited
int goldSquares[X_NODES][Y_NODES];		//Stores what squares contain gold

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
	return 1;
}

#pragma endregion

#pragma region Bot Logic

SPELUNKBOT_API double Update(double posX, double posY)
{

	#pragma region Update Variables

	//ResetBotVariables();

	//TODO: Stop the bot from always looking up

	// Store bot positions
	double pixelPosX = posX * 16;
	double pixelPosY = posY * 16;

	// Update visited squares
	visitedSquares[(int)posX][(int)posY] = 1;

	if (outputTicks == 0)
	{
		std::cout << "X = " << posX << "\t" << "Y = " << posY << std::endl;
		outputTicks = 20;
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

	UpdateMovementVariables(posX, posY, pixelPosX, pixelPosY);
	//UpdateStatusVariables();
	PopulateGoldMap();

		if (jumpTimer == 0)
	{
		_jump = false;
		if (_hasGoal)
		{
			Pathfind(posX, posY, targetX, targetY);
			ticks = 30;
		}
		jumpTimer = -1;
	}
	else
		jumpTimer--;

	#pragma endregion

	#pragma region Wait for Events

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

		if (_jump = false)
		{
			_goLeft = false;
			_goRight = false;
		}
		--_waitTimer;

		// Overriden by combat
		if (IsEnemyInNode(posX + 1, posY, NODE_COORDS) || IsEnemyInNode(posX - 1, posY, NODE_COORDS))
		{
			_attack = true;
			_waitTimer = 10;
		}

		return 1;
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

		if (posX < tempTargetX + 0.5)
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
			}
		}
		else
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

				//TODO: Check for enemies
			}
			//TODO: Check for enemies
		}

		//Let's not forget the Y-axis!

		/*
		if (posY < tempTargetY + 0.5)
		{
			_jump = true;
		}
		*/

		//Finally, update the pathfinding every 30 ticks to prevent the bot getting stuck in stupid places
		if (ticks == 0)
		{
			if (!_spIsInAir)
			{
				Pathfind(posX, posY, targetX, targetY);
				ticks = 30;
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
				targetX = x;
				targetY = y;
				Pathfind(posX, posY, targetX, targetY);
				return true;
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

	for (int y = 0; y < Y_NODES; y++)
	{
		for (int x = 0; x < X_NODES; x++)
		{
			if (goldSquares[x][y] == 1)
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
		std::cout << "Walking right" << std::endl;
		//move right
		_headingLeft = false;
		_headingRight = true;
		_goRight = true;
		_goLeft = false;
	}
	else
	{
		std::cout << "Walking left" << std::endl;
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
		std::cout << "Jumping right" << std::endl;
		//jump to the right
		_headingLeft = false;
		_headingRight = true;
		_goRight = true;
		_goLeft = false;
		_jump = true;

		jumpTimer = 10;
		_waitTimer = 20;
	}
	else
	{
		std::cout << "Jumping left" << std::endl;
		//jump to the left
		_headingRight = false;
		_headingLeft = true;
		_goLeft = true;
		_goRight = false;
		_jump = true;

		jumpTimer = 10;
		_waitTimer = 20;
	}
}

// This method allows the bot to jump in the required direction and climb up a ledge
void JumpGrab(double direction)
{
	if (direction == RIGHT)
	{
		std::cout << "Jump grabbing right" << std::endl;
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
		std::cout << "Jump grabbing left" << std::endl;
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
		std::cout << "Changing direction from right to left" << std::endl;
		//We can't head right, let's change our direction.
		_headingRight = false;
		_headingLeft = true;
		_goLeft = true;
		_goRight = false;
	}
	else
	{
		//We can't head left, let's change our direction.
		std::cout << "Changing direction from left to right" << std::endl;
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