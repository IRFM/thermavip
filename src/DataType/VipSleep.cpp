/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

/*#ifndef __APPLE__ static std::uint64_t NowInUs()
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
*/
void vipSleep(double milliseconds)
{
	if (milliseconds > 20) {
		SleepInMs(static_cast<std::uint32_t>(milliseconds));
	}
	else {
		SleepInUs(static_cast<std::uint32_t>(milliseconds * 1000));
	}
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
