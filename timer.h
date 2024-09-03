#pragma once

#include "framework.h"

#include "monitor.h"

#define IDT_TIMER1 1001

// How long to wait between the screens changing and changing rotations
constexpr int MONITOR_TIMER_INTERVAL = 2000;

HANDLE monitorTimerMutex;
bool paused = false;

void executeMonitorTimer(HWND hwnd, UINT, UINT_PTR, DWORD)
{
	KillTimer(hwnd, IDT_TIMER1);
	auto res = WaitForSingleObject(monitorTimerMutex, INFINITE);
	nassert(res == WAIT_OBJECT_0);

	if (!paused)
	{
		fixMonitorRotations();
	}

	nassert(ReleaseMutex(monitorTimerMutex));
}

void setMonitorTimer(HWND hwnd)
{
	KillTimer(hwnd, IDT_TIMER1);
	SetTimer(hwnd, IDT_TIMER1, MONITOR_TIMER_INTERVAL, executeMonitorTimer);
}

void initMonitorTimer()
{
	monitorTimerMutex = CreateMutex(
		NULL, // default security attributes
		FALSE, // initially not owned
		NULL); // unnamed mutex

	nassert(monitorTimerMutex != NULL);
}