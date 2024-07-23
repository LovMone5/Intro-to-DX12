#pragma once

#include <Windows.h>

class GameTimer {
public:
	GameTimer();
	GameTimer(const GameTimer& rhs) = delete;

	DOUBLE TotolTime() const;
	DOUBLE DeltaTime() const;

	void Reset();
	void Stop();
	void Tick();
	void Start();

	LONGLONG FPS();
private:
	void CalculateFrame();

	DOUBLE mSecondsPerCount;

	LONGLONG mBaseTime;
	LONGLONG mStopTime;
	LONGLONG mPausedTime;
	LONGLONG mPrevTime;
	LONGLONG mCurrTime;
	LONGLONG mDeltaTime;
	
	LONGLONG mFrameCount;
	LONGLONG mFPS;
	DOUBLE mElapsedTime;

	BOOL mPaused;
};