#include "VipSleep.h"

#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#else
#include <errno.h>
#include <time.h>

#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#endif // _WIN32

/**********************************=> unix ************************************/
#ifndef _WIN32
static void SleepInMs(std::uint32_t ms)
{
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = ms % 1000 * 1000000;

	while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
		;
}

static void SleepInUs(std::uint32_t us)
{
	struct timespec ts;
	ts.tv_sec = us / 1000000;
	ts.tv_nsec = us % 1000000 * 1000;

	while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
		;
}

#ifndef __APPLE__
static std::uint64_t NowInUs()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return static_cast<std::uint64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
}

#else  // mac
static std::uint64_t NowInUs()
{
	clock_serv_t cs;
	mach_timespec_t ts;

	host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cs);
	clock_get_time(cs, &ts);
	mach_port_deallocate(mach_task_self(), cs);

	return static_cast<std::uint64_t>(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
}
#endif // __APPLE__


void vipSleep(double milliseconds) 
{
	if (milliseconds > 20) {
		SleepInMs(static_cast<std::uint32_t>(milliseconds));
	}
	else {
		SleepInUs(static_cast<std::uint32_t>(milliseconds * 1000));
	}

#endif // _WIN32
/************************************ unix <=**********************************/

/**********************************=> win *************************************/
#ifdef _WIN32


static NTSTATUS(__stdcall* NtDelayExecution)(BOOL Alertable, PLARGE_INTEGER DelayInterval) = (NTSTATUS(__stdcall*)(BOOL, PLARGE_INTEGER))GetProcAddress(GetModuleHandle(L"ntdll.dll"),
																			"NtDelayExecution");
static NTSTATUS(__stdcall* ZwSetTimerResolution)(IN ULONG RequestedResolution,
						 IN BOOLEAN Set,
						 OUT PULONG ActualResolution) = (NTSTATUS(__stdcall*)(ULONG, BOOLEAN, PULONG))GetProcAddress(GetModuleHandle(L"ntdll.dll"), "ZwSetTimerResolution");

void vipSleep(double milliseconds)
{
	
	static bool once = true;
	if (once) {
		ULONG actualResolution;
		ZwSetTimerResolution(1, true, &actualResolution);
		once = false;
	}
	if (milliseconds > 20) {
		::Sleep(static_cast<DWORD>(milliseconds));
	}
	else {

		LARGE_INTEGER interval;
		interval.QuadPart = -1 * (int)(milliseconds * 10000.0f);
		NtDelayExecution(false, &interval);
	}
}

#endif // _WIN32
