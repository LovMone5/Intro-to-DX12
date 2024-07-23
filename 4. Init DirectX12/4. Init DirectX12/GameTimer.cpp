#include "GameTimer.h"

GameTimer::GameTimer()
{
	LONGLONG countsPerSecond;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSecond);
	mSecondsPerCount = 1.0 / countsPerSecond;
}

DOUBLE GameTimer::TotolTime() const
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

	mFrameCount = 0;
	mElapsedTime = 0.0f;
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

	CalculateFrame();
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

LONGLONG GameTimer::FPS()
{
	return mFPS;
}

void GameTimer::CalculateFrame()
{
	mFrameCount++;

	if (TotolTime() - mElapsedTime >= 1.0f) {
		mFPS = mFrameCount;
		double mspf = 1000.0f / mFPS;

		mFrameCount = 0;
		mElapsedTime += 1.0f;
	}
}
