/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#include "silentgui.h"

#include <berointerface.h>
#include <game.h>
#include <handinterface.h>

using namespace std;

SilentGui::SilentGui(ConfigFile *config)
	: ServerGuiWrapper(config, NULL, NULL)
{
}

SilentGui::~SilentGui()
{
}

void SilentGui::queue(function<void ()> step)
{
	m_steps.push_back(step);
}

bool SilentGui::pump()
{
	if(m_steps.empty()) return false;

	function<void ()> step = m_steps.front();
	m_steps.pop_front();
	if(step) step();
	return true;
}

boost::shared_ptr<HandInterface> SilentGui::currentHand() const
{
	if(!m_game) return boost::shared_ptr<HandInterface>();
	return m_game->getCurrentHand();
}

void SilentGui::queueRunBeRo()
{
	// Start of the betting of a street: BeRo::run() deals the cards of that
	// street on its first call and runs the betting afterwards.
	queue([this]() {
		boost::shared_ptr<HandInterface> hand = currentHand();
		if(hand && hand->getCurrentBeRo()) hand->getCurrentBeRo()->run();
	});
}

void SilentGui::preflopAnimation1()
{
	queueRunBeRo();
}

void SilentGui::flopAnimation1()
{
	queueRunBeRo();
}

void SilentGui::turnAnimation1()
{
	queueRunBeRo();
}

void SilentGui::riverAnimation1()
{
	queueRunBeRo();
}

void SilentGui::dealBeRoCards(int /*beRoID*/)
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

void SilentGui::nextPlayerAnimation()
{
	// The current seat is done: close the action and let the hand decide what
	// comes next (next seat, next street, or showdown).
	queue([this]() {
		boost::shared_ptr<HandInterface> hand = currentHand();
		if(hand) hand->switchRounds();
	});
}

void SilentGui::beRoAnimation2(int /*beRoID*/)
{
	queue([this]() {
		boost::shared_ptr<HandInterface> hand = currentHand();
		if(hand && hand->getCurrentBeRo()) hand->getCurrentBeRo()->nextPlayer();
	});
}

void SilentGui::meInAction()
{
	// The engine treats seat 0 as the human at the table and waits for the GUI
	// to deliver that player's action. At a table nobody watches every seat is
	// played by the AI, so the same call the other seats get lets seat 0 act.
	queue([this]() {
		boost::shared_ptr<HandInterface> hand = currentHand();
		if(hand && hand->getCurrentBeRo()) hand->getCurrentBeRo()->nextPlayer();
	});
}

void SilentGui::postRiverAnimation1()
{
	// The hand is decided: pay the pot out. That in turn reports back through
	// postRiverRunAnimation1() once every pot level has been distributed.
	queue([this]() {
		boost::shared_ptr<HandInterface> hand = currentHand();
		if(hand && hand->getCurrentBeRo()) hand->getCurrentBeRo()->postRiverRun();
	});
}

void SilentGui::postRiverRunAnimation1()
{
	if(onHandFinished) onHandFinished();
}
