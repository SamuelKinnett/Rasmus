//Rasmus, a Spelunkbot programmed by Samuel Kinnett

// SpelunkBot.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "Bot.h"

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

#pragma endregion

#pragma region Global Variables

bool canGoLeft;			//Can the bot go left?
bool canGoRight;		//Can the bot go right?
bool canJumpLeft;		//Can the bot jump left?
bool canJumpRight;		//Can the bot jump right?
bool canJumpGrabLeft;	//Can the bot jump left and grab a ledge?
bool canJumpGrabRight;	//Can the bot jump right and grab a ledge?

bool walkSpikeLeft;		//Will the bot hit a spike if he moves left?
bool walkSpikeRight;	//Will the bot hit a spike if he moves right? 
bool jumpSpikeLeft;		//Will the bot hit a spike if he jumps left?
bool jumpSpikeRight;	//Will the bot hit a spike if he moves right?

bool isClimbing;		//Is the player wallclimbing?
int ticks;				//Used to manage repeated pathfinding
int targetX;			//The current X co-ordinate the bot is trying to reach
int targetY;			//The current Y co-ordinate the bot is trying to reach

int outputTicks;		//To stop the constant fucking stream of console updates

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
	// Sample bot

	//ResetBotVariables();

	// Store bot positions
	double pixelPosX = posX * 16;
	double pixelPosY = posY * 16;

	if (outputTicks == 0)
	{
		std::cout << "X = " << posX << "\t" << "Y = " << posY << std::endl;
		outputTicks = 20;
	}
	else
		--outputTicks;

	UpdateMovementVariables(posX, posY, pixelPosX, pixelPosY);
	//UpdateStatusVariables();

	if (!IsNodePassable(posX, posY + 1, NODE_COORDS))
		_jump = false;
	if (_waitTimer > 0)
		--_waitTimer;

	#pragma region Wait for Events

	if (_waitTimer > 0)
	{
		//wallclimb check
		if (isClimbing)
		{
			if (_headingRight)
				_goRight = true;
			else
				_goLeft = true;
			_jump = true;
			if (_waitTimer == 1)
				isClimbing = false;
		}

		//waiting overridden by combat

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
		for (int y = 0; y < Y_NODES; y++)
		{
			for (int x = 0; x < X_NODES; x++)
			{
				//Aparrently the entrance is actually the exit.
				if (GetNodeState(x, y, NODE_COORDS) == spExit)
				{
					//If we have found the exit, pathfind to it
					std::cout << "Exit found!";
					targetX = x;
					targetY = y;
					Pathfind(posX, posY, targetX, targetY);
					return 1;
				}
			}
		}

		//If we haven't found the exit, navigate normally
		Navigate(posX, posY);

		//AttackEnemies
	}
	else
	{
		//if it does, let's move towards the next node in the path
		double tempTargetX = GetNextPathXPos(pixelPosX, pixelPosY, PIXEL_COORDS);
		double tempTargetY = GetNextPathYPos(pixelPosX, pixelPosY, PIXEL_COORDS);

		std::cout << "Target X = " << tempTargetX << "\t" << "Target Y = " << tempTargetY << std::endl;

		if (posX < tempTargetX)
		{
			_headingRight = true;
			_headingLeft = false;
			_goRight = true;

			//TODO: Check for enemies
		}
		else
		{
			_headingLeft = true;
			_headingRight = false;
			_goLeft = true;

			//TODO: Check for enemies
		}

		//Let's not forget the Y-axis!

		if (posY < tempTargetY)
		{
			_jump = true;
		}

		//Finally, update the pathfinding every 16 ticks to prevent the bot getting stuck in stupid places
		if (ticks == 0)
		{
			if (!_spIsInAir)
			{
				Pathfind(posX, posY, targetX, targetY);
				ticks = 16;
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
	_headingRight = false;
	_headingLeft = false;
	_goRight = false;
	_goLeft = false;
	_jump = false;
	_attack = false;
}

#pragma endregion

#pragma region Update Variables

//This method updates the terrain checking and available movement variables
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

//This method updates the status variables about Apsalus' environment
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

#pragma endregion

#pragma region Terrain Checks

//NODE_COORDS: Returns true if walking left would result in hitting a spike
bool IsWalkSpikeLeft(double posX, double posY)
{
	return ((GetNodeState(posX - 1, posY, NODE_COORDS) == spSpike)
		|| (GetNodeState(posX - 1, posY + 1, NODE_COORDS) == spSpike));
}

//NODE_COORDS: Returns true if walking right would result in hitting a spike
bool IsWalkSpikeRight(double posX, double posY)
{
	return ((GetNodeState(posX + 1, posY, NODE_COORDS) == spSpike)
		|| (GetNodeState(posX + 1, posY + 1, NODE_COORDS) == spSpike));
}

//NODE_COORDS: Returns true if jumping left would result in hitting a spike
bool IsJumpSpikeLeft(double posX, double posY)
{
	return ((GetNodeState(posX - 3, posY, NODE_COORDS) == spSpike)
		|| (GetNodeState(posX - 3, posY + 1, NODE_COORDS) == spSpike));
}

//NODE_COORDS: Returns true if jumping right would result in hitting a spike
bool IsJumpSpikeRight(double posX, double posY)
{
	return ((GetNodeState(posX + 3, posY, NODE_COORDS) == spSpike)
		|| (GetNodeState(posX + 3, posY + 1, NODE_COORDS) == spSpike));
}

//PIXEL_COORDS: Returns true if the bot can walk left
bool CanGoLeft(double posX, double posY)
{
	return IsNodePassable(posX - 16, posY, PIXEL_COORDS);
	//Uses pixel coords for improved accuracy
}

//PIXEL_COORDS: Returns true if the bot can walk right
bool CanGoRight(double posX, double posY)
{
	return IsNodePassable(posX + 16, posY, PIXEL_COORDS);
	//Uses pixel coords for improved accuracy
}

//Returns true if the bot can jump left
bool CanJumpLeft(double posX, double posY)
{
	return IsNodePassable(posX - 1, posY - 1, NODE_COORDS);
}

//Returns true if the bot can jump right
bool CanJumpRight(double posX, double posY)
{
	return IsNodePassable(posX + 1, posY - 1, NODE_COORDS);
}

//Returns true if the bot can jump and grab a ledge to the left
bool CanJumpGrabLeft(double posX, double posY)
{
	return (IsNodePassable(posX - 1, posY - 2, NODE_COORDS) && !IsNodePassable(posX - 1, posY - 1, NODE_COORDS) && IsNodePassable(posX, posY - 1, NODE_COORDS));
}

//Returns true if the bot can jump and grab a ledge to the right
bool CanJumpGrabRight(double posX, double posY)
{
	return (IsNodePassable(posX + 1, posY - 2, NODE_COORDS) && !IsNodePassable(posX + 1, posY - 1, NODE_COORDS) && IsNodePassable(posX, posY - 1, NODE_COORDS));
}

#pragma endregion

#pragma region Navigation

//This method calls the pathfinding algorithm and updates the ticks variable
void Pathfind(double posX, double posY, double x, double y)
{
	ticks = 16;
	_hasGoal = true;
	CalculatePathFromXYtoXY(posX, posY, x, y, NODE_COORDS);
}

//This method decides how the bot should move to navigate the environment
void Navigate(double posX, double posY)
{
	if (_headingLeft)
	{
		if (canJumpGrabLeft && IsJumpSpikeLeft)
		{
			std::cout << "Jump grabbing left" << std::endl;
			//jump grab to the left
			_headingLeft = true;
			_goLeft = true;
			_goRight = false;
			_jump = true;
			isClimbing = true;
			_waitTimer = 2;
		}
		else if (canGoLeft && IsWalkSpikeLeft)
		{
			std::cout << "Walking left" << std::endl;
			//move left
			_headingLeft = true;
			_goLeft = true;
			_goRight = false;
		}
		else if (canJumpLeft && IsJumpSpikeLeft)
		{
			std::cout << "Jumping left" << std::endl;
			//jump to the left
			_headingLeft = true;
			_goLeft = true;
			_goRight = false;
			_jump = true;
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
	else if (_headingRight)
	{
		if (canJumpGrabRight && IsJumpSpikeRight)
		{
			std::cout << "Jump grabbing right" << std::endl;
			//jump grab to the right
			_headingRight = true;
			_goRight = true;
			_goLeft = false;
			_jump = true;
			isClimbing = true;
			_waitTimer = 2;
		}
		else if (canGoRight && IsWalkSpikeRight)
		{
			std::cout << "Walking right" << std::endl;
			//move right
			_headingRight = true;
			_goRight = true;
			_goLeft = false;
		}
		else if (canJumpRight && IsJumpSpikeRight)
		{
			std::cout << "Jumping right" << std::endl;
			//jump to the right
			_headingRight = true;
			_goRight = true;
			_goLeft = false;
			_jump = true;
		}
		else
		{
			std::cout << "Changing direction from right to left" << std::endl;
			//We can't head right, let's change our direction.
			_headingRight = false;
			_headingLeft = true;
			_goLeft = true;
			_goRight = false;
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