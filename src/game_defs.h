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
#ifndef GAME_DEFS_H
#define GAME_DEFS_H

#define MIN_NUMBER_OF_PLAYERS		2
#define MAX_NUMBER_OF_PLAYERS		10

/* How many players a table of OURS seats (decision 2026-11-12, "9 игроков и общий
 * лимит для любых столов"): a cash table, a local game and a tournament table are
 * all nine-handed - eight opponents and the player. The engine keeps its ten seats
 * (arrays, protocol, ranking games of an upstream server): the tenth seat simply
 * stays empty, so a legacy ten-player game can still be rendered. */
#define MAX_TABLE_SEATS				9

#define MIN_GUI_SPEED			1
#define MAX_GUI_SPEED			11

#define DEBUG_MODE			0
#define SQLITE_LOG			1
#define HTML_LOG			0

#define POKERTH_VERSION_MAJOR	2
#define POKERTH_VERSION_MINOR	1
#define POKERTH_VERSION			((POKERTH_VERSION_MAJOR << 8) | POKERTH_VERSION_MINOR)

#define POKERTH_BETA_RELEASE_STRING	 "2.1.11"

/* Version numbers for the Windows resource (icon + FILEVERSION/PRODUCTVERSION in
 * the .exe): cmake/PokerthWindowsResource.cmake parses exactly these defines.
 * The values are the release version 2.1.11 (QML) and the frozen widget-client
 * version 2.1.9.3, which the installer still uses as its product version. */
#define WIDGET_VERSION_REVISION	9
#define WIDGET_VERSION_PATCH		3
#define QML_VERSION_MAJOR		2
#define QML_VERSION_MINOR		1
#define QML_VERSION_REVISION	11


#define SQLITE_LOG_VERSION		1

enum GameState {
	GAME_STATE_PREFLOP = 0,
	GAME_STATE_FLOP,
	GAME_STATE_TURN,
	GAME_STATE_RIVER,
	GAME_STATE_POST_RIVER,
	GAME_STATE_PREFLOP_SMALL_BLIND = 0xF0,
	GAME_STATE_PREFLOP_BIG_BLIND = 0xF1
};

enum PlayerAction {
	PLAYER_ACTION_NONE = 0,
	PLAYER_ACTION_FOLD,
	PLAYER_ACTION_CHECK,
	PLAYER_ACTION_CALL,
	PLAYER_ACTION_BET,
	PLAYER_ACTION_RAISE,
	PLAYER_ACTION_ALLIN
};

enum PlayerActionCode {
	ACTION_CODE_VALID = 0,
	ACTION_CODE_INVALID_STATE,
	ACTION_CODE_NOT_YOUR_TURN,
	ACTION_CODE_NOT_ALLOWED
};

enum PlayerActionLog {
	LOG_ACTION_NONE = 0,
	LOG_ACTION_DEALER,      // starts as dealer
	LOG_ACTION_SMALL_BLIND, // posts small blind
	LOG_ACTION_BIG_BLIND,   // posts big blind
	LOG_ACTION_FOLD,        // folds
	LOG_ACTION_CHECK,       // checks
	LOG_ACTION_CALL,        // calls
	LOG_ACTION_BET,         // bets
	LOG_ACTION_ALL_IN,      // is all in with
	LOG_ACTION_SHOW,        // shows
	LOG_ACTION_HAS,         // has
	LOG_ACTION_WIN,         // wins
	LOG_ACTION_WIN_SIDE_POT,// wins (side pot)
	LOG_ACTION_SIT_OUT,     // sits out
	LOG_ACTION_WIN_GAME,    // wins game
	LOG_ACTION_LEFT,		// has left the game
	LOG_ACTION_KICKED,		// was kicked from the game
	LOG_ACTION_ADMIN,		// is game admin now
	LOG_ACTION_JOIN			// has joined the game
};

enum Button {
	BUTTON_NONE = 0,
	BUTTON_DEALER,
	BUTTON_SMALL_BLIND,
	BUTTON_BIG_BLIND
};

#endif
