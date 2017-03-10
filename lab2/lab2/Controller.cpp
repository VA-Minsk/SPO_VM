﻿#ifdef _WIN32

#include <windows.h>

#elif (defined(__linux__) || defined(__unix__))

#include <queue>
#include <cstring>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#else
#error Bad operation system. Please, recompile me to Linux, Unix or Windows
#endif


#include "Controller.h"

#ifdef _WIN32
namespace
{
	const LPCTSTR isMachineFree = TEXT("isMachineFree");
	const LPCTSTR fromUser = TEXT("fromUser");
	const LPCTSTR fromMachine = TEXT("fromMachine");
}
#elif (defined(__linux__) || defined(__unix__))
namespace
{
	const char ServerPIDfilename[] = "serverPID.db";
	const int SIGF0 = 10;
	const int SIGF1 = 11;
	const int SIGF2 = 12;

	std::queue<pid_t> PIDq;
	bool signalIsHere[] = {false, false, false};
}
#endif


void SelectMode()
{
	std::cout << "Выберите, пожалуйста, режим работы:" << std::endl;
	std::cout << "1) Пользователь" << std::endl;
	std::cout << "2) Автомат" << std::endl;
	std::cout << "Пожалуйста, сделайте свой выбор" << std::endl;

	auto k = std::cin.get();
	Stream::Clear();

	switch (k)
	{
	case '1':
		{
			WorkAsPerson();
			break;
		}
	case '2':
		{
			WorkAsCoffeeMachine();
			break;
		}
	default:
		{
			std::cout << "Извините, такого варианта не существует. Выходим..." << std::endl;
			break;
		}
	}
}

#ifdef _WIN32

void WorkAsPerson()
{
	HANDLE EVENT[3];

	//check machine (chek opening flags)
	EVENT[0] = OpenEvent(EVENT_ALL_ACCESS, NULL, isMachineFree);
	if (EVENT[0] == NULL)
	{
		std::cout << "Ошибка! Автомат не запущен"; //error. Machine is not started
		CloseHandle(EVENT[0]);
		return;
	}

	EVENT[1] = OpenEvent(EVENT_ALL_ACCESS, NULL, fromUser);
	if (EVENT[1] == NULL)
	{
		std::cout << "Ошибка! Автомат не запущен"; //error. Machine is not started
		CloseHandle(EVENT[0]);
		return;
	}

	EVENT[2] = OpenEvent(EVENT_ALL_ACCESS, NULL, fromMachine);
	if (EVENT[2] == NULL)
	{
		std::cout << "Ошибка! Автомат не запущен"; //error. Machine is not started
		CloseHandle(EVENT[0]);
		return;
	}
	Person person;

	//start loop
	do
	{
		if (!person.runConsole())
		{
			break;
		}
		//wait 1
		WaitForSingleObject(EVENT[0],INFINITE);

		person.sendRequest();
		//raise flag2
		if (!SetEvent(EVENT[1]))
		{
			std::cout << "Ошибка! Не уделось провзаимодействовать с автоматом"; //error. Event is not pulsed
			break;
		}

		//wait flag3
		WaitForSingleObject(EVENT[2],INFINITE);

		person.getResponce();
		//raise flag2
		if (!SetEvent(EVENT[1]))
		{
			std::cout << "Ошибка! Не уделось провзаимодействовать с автоматом"; //error. Event is not pulsed
			break;
		}
	}
	while (true);

	for (int i = 0; i < 3; i ++)
	{
		CloseHandle(EVENT[i]);
	}
}

void WorkAsCoffeeMachine() //TVS
{
	HANDLE EVENT[3];

	//check existing
	EVENT[0] = OpenEvent(NULL, NULL, isMachineFree);
	if (EVENT[0])
	{
		std::cout << "Ошибка! Автомат уже запущен."; //error. Machine already started
		CloseHandle(EVENT[0]);
		return;
	}

	EVENT[1] = OpenEvent(NULL, NULL, fromUser);
	if (EVENT[1])
	{
		std::cout << "Ошибка! Автомат уже запущен."; //error. Machine already started
		CloseHandle(EVENT[1]);
		return;
	}

	EVENT[2] = OpenEvent(NULL, NULL, fromMachine);
	if (EVENT[2])
	{
		std::cout << "Ошибка! Автомат уже запущен."; //error. Machine already started
		CloseHandle(EVENT[2]);
		return;
	}

	//create events
	EVENT[0] = CreateEvent(NULL, false, false, isMachineFree);
	EVENT[1] = CreateEvent(NULL, false, false, fromUser);
	EVENT[2] = CreateEvent(NULL, false, false, fromMachine);


	CoffeMachine machine;
	do
	{
		//raise flag1
		if (!SetEvent(EVENT[0]))
		{
			std::cout << "Ошибка! Не уделось провзаимодействовать с пользователем."; //error. Event is not pulsed
			return;
		}

		// wait	flag2
		WaitForSingleObject(EVENT[1],INFINITE);

		machine.proceed();

		//raise flag3
		if (!SetEvent(EVENT[2]))
		{
			std::cout << "Ошибка! Не уделось провзаимодействовать с пользователем"; //error. Event is not pulsed
			return;
		}

		//wait flag2
		WaitForSingleObject(EVENT[1],INFINITE);

		machine.writeToFile();
	}
	while (true);

	for (int i = 0; i < 3; i ++)
	{
		CloseHandle(EVENT[i]);
	}
}

#elif (defined(__linux__) || defined(__unix__))

void StartWorkingWithNewUser()
{
	if (isWorkWithUserNow || PIDq.empty())
	{
		return 0;
	}

	pid_t currPID = PIDq.front();
	PIDq.pop();
	if (currPID == 0)
	{
		return 0;
	}

	isWorkWithUserNow = true;

	kill(currPID, SIGF0);

	return currPID;
}

//save pid to queue
void hdlF0Machine(int sig, siginfo_t* sigptr, void*)
{
	if (!sigptr)
	{
		return;
	}

	PIDq.push(sigptr -> si_pid);

	signalIsHere[0] = true;
}

//нужно обработать результаты
void hdlF1Machine(int sig, siginfo_t* sigptr, void*)
{
	signalIsHere[1] = true;
}

//предыдущий пользователь закончил работу
void hdlF2Machine(int sig, siginfo_t* sigptr, void*)
{
	signalIsHere[2] = true;
}

int setSigAction(int sig, void (*handleFun) (int, siginfo_t*, void*))
{
	struct sigaction act;
	memset(&act, NULL, sizeof(act));	//clear all struct
	act.sa_sigaction = handleFun;
	sct.sa_flags = SA_SIGINFO;
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, sig);
	act.sa_mask = set;
	return sigaction(sig, &act, NULL);
}

void setSigsMachine()
{
	setSigAction(SIGF0, hdlF0Machine);
	setSigAction(SIGF1, hdlF1Machine);
	setSigAction(SIGF2, hdlF2Machine);
}

void WritePID(pid_t pid)
{
//TODO
}

void WorkAsCoffeeMachine()
{
	setSigsMachine();

	WritePID(getpid());

	pid_t currPID = 0;
	bool isWorkWithUserNow;
	CoffeMachine machine;

	isWorkWithUserNow = false;

	currPID = 0;	

	while (true)
	{
		if (!isWorkWithUserNow)
		{
			currPID = StartWorkingWithNewUser();
		}

		if (signalIsHere[2])
		{
			signalIsHere[2] = false;
			isWorkWithUserNow = false;
			continue;
		}

		if (signalIsHere[1])
		{
			signalIsHere[1] = false;
			machine.proceed();
			machine.writeToFile();

			kill(currPID, SIGF1);

			continue;
		}
	}
}

//------------------Person--------------
//значит, можно начинать работу
void hdlF0Person(int sig, siginfo_t* sigptr, void*)
{
	signalIsHere[0] = true;
}

//можем читать результат
void hdlF1Person(int sig, siginfo_t* sigptr, void*)
{
	signalIsHere[1] = true;
}


void setSigsPerson()
{
	setSigAction(SIGF0, hdlF0Person);
	setSigAction(SIGF1, hdlF1Person);
}


pid_t getServerPID()
{
	pid_t res;

	res = 0;	//TODO: read PID

	return res;
}

void WorkAsPerson()
{
	setSigsPerson();

	pid_t serverPID = getServerPID();

	kill(serverPID, SIGF0);

	while (!signalIsHere[0]) {}

	signalIsHere[0] = false;

	while (true)
	{
		if (!person.runConsole())
		{
			break;
		}

		person.sendRequest();
		
		kill(serverPID, SIGF1);

		while (!signalIsHere[1]) {}

		signalIsHere[1] = false;

		person.getResponce();
	}

	kill(serverPID, SIGF2);
}

#endif
