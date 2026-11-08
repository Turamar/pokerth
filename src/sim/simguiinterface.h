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

#ifndef SIMGUIINTERFACE_H
#define SIMGUIINTERFACE_H

#include <generic/serverguiwrapper.h>

#include <deque>
#include <functional>

class ConfigFile;
class Game;
class HandInterface;

/* GuiInterface for the headless simulator.
 *
 * The local engine does not run a loop of its own: every step of a hand is
 * announced to the GUI and the GUI is expected to call back into the engine -
 * "preflopAnimation1" starts the betting, "beRoAnimation2" lets the next seat
 * act, "nextPlayerAnimation" closes the action, "postRiverAnimation1" pays the
 * pot out. The Qt clients spread that chain over timers and animations, which
 * is how the player gets to see what happens.
 *
 * Here there is nothing to show, so every callback simply enqueues the engine
 * call it stands for, and the driver executes the queue step by step. That
 * keeps the recursion depth constant (the QML client has the same problem and
 * solves it the same way with singleShot timers) and makes a hand a plain
 * sequence of steps that the driver can count, limit and time.
 *
 * ServerGuiWrapper is the GUI-less implementation the dedicated server uses:
 * it already answers every other method of the interface with a no-op, so only
 * the steps of the game loop have to be replaced here.
 */
class SimGui : public ServerGuiWrapper
{
public:
	explicit SimGui(ConfigFile *config);
	virtual ~SimGui();

	// The game whose hand the queued steps apply to. Steps look the current
	// hand up when they run, so a step queued by one hand can never act on the
	// hand after it.
	void setGame(Game *game)
	{
		m_game = game;
	}

	// Execute the next queued step. Returns false when nothing is left.
	bool pump();

	size_t pendingSteps() const
	{
		return m_steps.size();
	}

	// Called once the pot of the current hand has been distributed, i.e. when
	// the hand is over. The engine data of that hand is still valid here.
	std::function<void()> onHandFinished;

	// ── the steps of the game loop ──
	void nextPlayerAnimation() override;
	void beRoAnimation2(int beRoID) override;
	void preflopAnimation1() override;
	void flopAnimation1() override;
	void turnAnimation1() override;
	void riverAnimation1() override;
	void dealBeRoCards(int beRoID) override;
	void postRiverAnimation1() override;
	void postRiverRunAnimation1() override;
	void meInAction() override;

private:
	void queue(std::function<void()> step);
	void queueRunBeRo();
	boost::shared_ptr<HandInterface> currentHand() const;

	Game *m_game;
	std::deque<std::function<void()> > m_steps;
};

#endif
