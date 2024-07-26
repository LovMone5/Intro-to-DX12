#pragma once

#include <Windows.h>

class GameTimer {
public:
	GameTimer();
	GameTimer(const GameTimer& rhs) = delete;

	DOUBLE TotalTime() const;
	DOUBLE DeltaTime() const;

	void Reset();
	void Stop();
	void Tick();
	void Start();
private:
	DOUBLE mSecondsPerCount;

	LONGLONG mBaseTime;
	LONGLONG mStopTime;
	LONGLONG mPausedTime;
	LONGLONG mPrevTime;
	LONGLONG mCurrTime;
	LONGLONG mDeltaTime;

	BOOL mPaused;
};