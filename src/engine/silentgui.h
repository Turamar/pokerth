/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#ifndef SILENTGUI_H
#define SILENTGUI_H

#include <gui/generic/serverguiwrapper.h>

#include <boost/shared_ptr.hpp>
#include <deque>
#include <functional>

class ConfigFile;
class Game;
class HandInterface;

/* GuiInterface for a table nobody watches.
 *
 * The local engine does not run a loop of its own: every step of a hand is
 * announced to the GUI and the GUI is expected to call back into the engine -
 * "preflopAnimation1" starts the betting, "beRoAnimation2" lets the next seat
 * act, "nextPlayerAnimation" closes the action, "postRiverAnimation1" pays the
 * pot out. The Qt clients spread that chain over timers and animations, which
 * is how the player gets to see what happens.
 *
 * Here there is nothing to show, so every callback simply enqueues the engine
 * call it stands for, and the caller executes the queue step by step. That
 * keeps the recursion depth constant (the QML client has the same problem and
 * solves it the same way with singleShot timers) and makes a hand a plain
 * sequence of steps that can be counted, limited and timed.
 *
 * ServerGuiWrapper is the GUI-less implementation the dedicated server uses:
 * it already answers every other method of the interface with a no-op, so only
 * the steps of the game loop have to be replaced here.
 *
 * Used by the headless simulator and by the tournament runner, both of which
 * play tables nobody is sitting at (PLAN.md section 3, "Тихие столы").
 */
class SilentGui : public ServerGuiWrapper
{
public:
	explicit SilentGui(ConfigFile *config);
	virtual ~SilentGui();

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

#endif // SILENTGUI_H
