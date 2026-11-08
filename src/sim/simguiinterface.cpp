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

#include "simguiinterface.h"

#include <berointerface.h>
#include <game.h>
#include <handinterface.h>

using namespace std;

SimGui::SimGui(ConfigFile *config)
	: ServerGuiWrapper(config, NULL, NULL)
{
}

SimGui::~SimGui()
{
}

void SimGui::queue(function<void ()> step)
{
	m_steps.push_back(step);
}

bool SimGui::pump()
{
	if(m_steps.empty()) return false;

	function<void ()> step = m_steps.front();
	m_steps.pop_front();
	if(step) step();
	return true;
}

boost::shared_ptr<HandInterface> SimGui::currentHand() const
{
	if(!m_game) return boost::shared_ptr<HandInterface>();
	return m_game->getCurrentHand();
}

void SimGui::queueRunBeRo()
{
	// Start of the betting of a street: BeRo::run() deals the cards of that
	// street on its first call and runs the betting afterwards.
	queue([this]() {
		boost::shared_ptr<HandInterface> hand = currentHand();
		if(hand && hand->getCurrentBeRo()) hand->getCurrentBeRo()->run();
	});
}

void SimGui::preflopAnimation1()
{
	queueRunBeRo();
}

void SimGui::flopAnimation1()
{
	queueRunBeRo();
}

void SimGui::turnAnimation1()
{
	queueRunBeRo();
}

void SimGui::riverAnimation1()
{
	queueRunBeRo();
}

void SimGui::dealBeRoCards(int /*beRoID*/)
{
	// Called from BeRo::run() before the cards of a street are "shown". There
	// is nothing to show, so this is the same continuation the QML client runs
	// after dealing: in an all-in state there is no betting left and the hand
	// moves on to the next street, otherwise the betting starts.
	queue([this]() {
		boost::shared_ptr<HandInterface> hand = currentHand();
		if(!hand) return;
		if(hand->getAllInCondition()) {
			hand->switchRounds();
		} else if(hand->getCurrentBeRo()) {
			hand->getCurrentBeRo()->run();
		}
	});
}

void SimGui::nextPlayerAnimation()
{
	// The current seat is done: close the action and let the hand decide what
	// comes next (next seat, next street, or showdown).
	queue([this]() {
		boost::shared_ptr<HandInterface> hand = currentHand();
		if(hand) hand->switchRounds();
	});
}

void SimGui::beRoAnimation2(int /*beRoID*/)
{
	queue([this]() {
		boost::shared_ptr<HandInterface> hand = currentHand();
		if(hand && hand->getCurrentBeRo()) hand->getCurrentBeRo()->nextPlayer();
	});
}

void SimGui::meInAction()
{
	// The engine treats seat 0 as the human at the table and waits for the GUI
	// to deliver that player's action. In the simulator every seat is played by
	// the AI, so the same call the other seats get lets seat 0 act as well.
	queue([this]() {
		boost::shared_ptr<HandInterface> hand = currentHand();
		if(hand && hand->getCurrentBeRo()) hand->getCurrentBeRo()->nextPlayer();
	});
}

void SimGui::postRiverAnimation1()
{
	// The hand is decided: pay the pot out. That in turn reports back through
	// postRiverRunAnimation1() once every pot level has been distributed.
	queue([this]() {
		boost::shared_ptr<HandInterface> hand = currentHand();
		if(hand && hand->getCurrentBeRo()) hand->getCurrentBeRo()->postRiverRun();
	});
}

void SimGui::postRiverRunAnimation1()
{
	if(onHandFinished) onHandFinished();
}
