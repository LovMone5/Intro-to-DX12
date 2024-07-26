#include "GameTimer.h"

GameTimer::GameTimer()
{
	LONGLONG countsPerSecond;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSecond);
	mSecondsPerCount = 1.0 / countsPerSecond;
}

DOUBLE GameTimer::TotalTime() const
{
	if (mPaused) return (mStopTime - mBaseTime - mPausedTime) * mSecondsPerCount;
	else return (mCurrTime - mBaseTime - mPausedTime) * mSecondsPerCount;
}

DOUBLE GameTimer::DeltaTime() const
{
	return mDeltaTime * mSecondsPerCount;
}

void GameTimer::Reset()
{
	LONGLONG curentTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&curentTime);

	mBaseTime = curentTime;
	mPrevTime = curentTime;
	mStopTime = 0;
	mPausedTime = 0;
	mPaused = FALSE;
}

void GameTimer::Stop()
{
	if (!mPaused) {
		mPaused = TRUE;

		LONGLONG curentTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&curentTime);
		mStopTime = curentTime;
	}
}

void GameTimer::Tick()
{
	if (mPaused) {
		return;
	}
	LONGLONG curentTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&curentTime);

	mCurrTime = curentTime;
	mDeltaTime = mCurrTime - mPrevTime;
	mPrevTime = mCurrTime;

	mDeltaTime = max(mDeltaTime, 0);
}

void GameTimer::Start()
{
	if (mPaused) {
		LONGLONG curentTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&curentTime);

		mPausedTime += curentTime - mStopTime;
		mStopTime = 0;
		mPrevTime = curentTime;
		mPaused = FALSE;
	}
}
