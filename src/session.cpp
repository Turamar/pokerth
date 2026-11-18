/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *                                                                           *
 *                                                                           *
 * Additional permission under GNU AGPL version 3 section 7                  *
 *                                                                           *
 * If you modify this program, or any covered work, by linking or            *
 * combining it with the OpenSSL project's OpenSSL library (or a             *
 * modified version of that library), containing parts covered by the        *
 * terms of the OpenSSL or SSLeay licenses, the authors of PokerTH           *
 * (Felix Hammer, Florian Thauer, Lothar May) grant you additional           *
 * permission to convey the resulting work.                                  *
 * Corresponding Source for a non-source form of such a combination          *
 * shall include the source code for the parts of OpenSSL used as well       *
 * as that of the covered work.                                              *
 *****************************************************************************/
#include "session.h"
#include "game.h"
#include "log.h"
#include "guiinterface.h"
#include "configfile.h"
#include <qttoolsinterface.h>
#include <localenginefactory.h>
#include <core/avatarmanager.h>

#include <sstream>


using namespace std;

Session::Session(GuiInterface *g, ConfigFile *c, Log *l)
	: currentGameNum(0), myGui(g), myConfig(c), myLog(l)
{
	myQtToolsInterface = CreateQtToolsWrapper();
}


Session::~Session()
{
	delete myQtToolsInterface;
	myQtToolsInterface = 0;
	// The Log is not owned by the Session: main() keeps it alive until after
	// this destructor has run, so the engine can still write while the running
	// game is torn down here.
}

bool Session::init()
{
	myAvatarManager.reset(new AvatarManager());
	bool retVal = myAvatarManager->Init(
					  myQtToolsInterface->stringFromUtf8(myConfig->readConfigString("AppDataDir")),
					  myQtToolsInterface->stringFromUtf8(myConfig->readConfigString("CacheDir")));
	addOwnAvatar(myQtToolsInterface->stringFromUtf8(myConfig->readConfigString("MyAvatar")));
#ifndef POKERTH_OFFICIAL_SERVER
	// The official server keeps its cache as the permanent avatar store: it is
	// synced to the web server, which shows the avatars on the player profiles.
	// Deleting cache files there removes the avatars from the website.
	myAvatarManager->RemoveOldAvatarCacheEntries();
#endif
	return retVal;
}

void Session::init(boost::shared_ptr<AvatarManager> manager)
{
	myAvatarManager = manager;
}

void Session::addOwnAvatar(const std::string &avatarFile)
{
	myAvatarManager->AddSingleAvatar(avatarFile);
}

void Session::startLocalGame(const GameData &gameData, const StartData &startData,
							 bool startFirstHand)
{

	currentGame.reset();

	currentGameNum++;

	myGui->initGui(gameData.guiSpeed);

	PlayerDataList playerDataList;
	for(int i = 0; i < startData.numberOfPlayers; i++) {

		//query names and avatar path
		ostringstream myName;
		if (i==0) {
			myName << "MyName";
		} else {
			myName << "Opponent" << i << "Name";
		}
		ostringstream myAvatar;
		if (i==0) {
			myAvatar << "MyAvatar";
		} else {
			myAvatar << "Opponent" << i << "Avatar";
		}

		//PlayerData erzeugen
		// UniqueId = PlayerNumber for local games.
		boost::shared_ptr<PlayerData> playerData(new PlayerData(
					i,
					i,
					i == 0 ? PLAYER_TYPE_HUMAN : PLAYER_TYPE_COMPUTER,
					PLAYER_RIGHTS_NORMAL,
					i == 0));
		playerData->SetName(myConfig->readConfigString(myName.str()));
		playerData->SetAvatarFile(myConfig->readConfigString(myAvatar.str()));

		// A table built from a roster brings the name and the stack of every
		// seat (state/roster.h): the same person keeps both when the table is
		// rebuilt, which is what makes a moved player still be that player.
		if (i < static_cast<int>(gameData.seatNames.size())
				&& !gameData.seatNames[i].empty()) {
			playerData->SetName(gameData.seatNames[i]);
		}
		if (i < static_cast<int>(gameData.seatStacks.size())) {
			playerData->SetStartCash(gameData.seatStacks[i]);
		}

		playerDataList.push_back(playerData);
	}
	// EngineFactory erstellen
	boost::shared_ptr<EngineFactory> factory(new LocalEngineFactory(myConfig, gameData.botProfiles)); // LocalEngine erstellen

	currentGame.reset(new Game(myGui, factory, playerDataList, gameData, startData, currentGameNum, myLog));

	//// SPIEL-SCHLEIFE
	if(startFirstHand) {
		currentGame->initHand();
		currentGame->startHand();
	}
	// SPIEL-SCHLEIFE
}

boost::shared_ptr<Game> Session::getCurrentGame()
{
	return currentGame;
}

boost::shared_ptr<AvatarManager> Session::getAvatarManager()
{
	return myAvatarManager;
}
