#pragma once
#include <string>
#include <DirectXColors.h>
#include "D3DApp.h"

class TestApp: public D3DApp {
public:
	TestApp(HINSTANCE hInstance);

	bool Initialize();
	void Update(const GameTimer& gt);
	void Draw(const GameTimer& gt);
};