/*
  Copyright (C) 2001-2004 Stephane Magnenat & Luc-Olivier de Charrière
  for any question or comment contact us at nct@ysagoon.com or nuage@ysagoon.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef __BULDING_TYPE_H
#define __BULDING_TYPE_H

#include <vector>

#include "ConfigFiles.h"
#include "UnitConsts.h"
#include "Ressource.h"

class Sprite;

class BuildingType: public LoadableFromConfigFile
{
public:
	
	enum BuildingTypeShortNumber
	{
		SWARM_BUILDING=0,
		FOOD_BUILDING=1,
		HEAL_BUILDING=2,

		WALKSPEED_BUILDING=3,
		SWIMSPEED_BUILDING=4,
		ATTACK_BUILDING=5,
		SCIENCE_BUILDING=6,

		DEFENSE_BUILDING=7,
		
		EXPLORATION_FLAG=8,
		WAR_FLAG=9,
		CLEARING_FLAG=10,

		STONE_WALL=11,

		MARKET_BUILDING=12,

		NB_BUILDING
	};
	
	// basic infos
	std::string type;

	// visualisation
	std::string gameSprite;
	Sint32 gameSpriteImage;
	std::string miniSprite;
	Sint32 miniSpriteImage;
	
	Sint32 hueImage; // bool. The way we show the building's team (false=we draw a flag, true=we hue all the sprite)
	Sint32 flagImage;
	Sint32 crossConnectMultiImage; // If true, mean we have a wall-like building
	
	// could be Uint8, if non 0 tell the number of maximum units locked by bulding for:
	// by order of priority (top = max)
	Sint32 upgrade[NB_ABILITY]; // What kind on units can be upgraded here
	Sint32 upgradeTime[NB_ABILITY]; // Time to upgrade an unit, given the upgrade type needed.
	Sint32 foodable;
	Sint32 fillable;
	Sint32 zonable[NB_UNIT_TYPE]; // If an unit is required for a presence.
	Sint32 zonableForbidden;
	
	Sint32 canFeedUnit;
	Sint32 timeToFeedUnit;
	Sint32 canHealUnit;
	Sint32 timeToHealUnit;
	Sint32 insideSpeed;
	Sint32 canExchange;

	Sint32 width, height; // Uint8, size in square
	Sint32 decLeft, decTop;
	Sint32 isVirtual; // bool, doesn't occupy ground occupation map, used for war-flag and exploration-flag.
	Sint32 isCloacked; // bool, graphicaly invisible for enemy.
	//Sint32 *walkOverMap; // should be allocated and deleted in a cleany way
	//Sint32 walkableOver; // bool, can walk over
	Sint32 shootingRange; // Uint8, if 0 can't shoot
	Sint32 shootDamage; // Uint8
	Sint32 shootSpeed; // Uint8
	Sint32 shootRythme; // Uint8
	Sint32 maxBullets;
	Sint32 multiplierStoneToBullets;

	Sint32 unitProductionTime; // Uint8, nb tick to produce one unit
	Sint32 ressourceForOneUnit;

	Sint32 maxRessource[MAX_NB_RESSOURCES];
	Sint32 multiplierRessource[MAX_NB_RESSOURCES];
	Sint32 maxUnitInside;
	Sint32 maxUnitWorking;

	Sint32 hpInit; // (Uint16)
	Sint32 hpMax;
	Sint32 hpInc;
	Sint32 armor; // (Uint8)
	Sint32 level; // (Uint8)
	Sint32 shortTypeNum; // BuildingTypeShortNumber, Should not be used by the main engine, but only to choose the next level building.
	Sint32 isBuildingSite;

	// Flag usefull
	Sint32 defaultUnitStayRange;
	Sint32 maxUnitStayRange;

	Sint32 viewingRange;
	Sint32 regenerationSpeed;
	
	Sint32 prestige;

	// Number to access next upgrade in BuildingsTypes.
	// It is computed in the second phase of the BuildingsTypes constructor after all building are read from disk.
	Sint32 nextLevelTypeNum;
	Sint32 typeNum;
	Sint32 lastLevelTypeNum;
	
	// Regenerated parameters
	Sprite *gameSpritePtr;
	Sprite *miniSpritePtr;

public:
	virtual ~BuildingType() { }
	virtual void loadFromConfigFile(const ConfigBlock *configBlock);
};

#endif
 
