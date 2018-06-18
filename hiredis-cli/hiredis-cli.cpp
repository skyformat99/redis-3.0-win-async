// hiredis-cli.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "RedisProxy.h"

int main()
{
	///////////////////////0////////////////////////
	RedisProxy proxy;
	proxy.Init(std::string("127.0.0.1"), 6379, RedisProxy::ECM_BOTH);
	proxy.FrameBegin();
	for (int i = 0; i < 10; ++i)
	{
		proxy.SendAsyncCommand([](bool suc, auto reply)->void {
			for (auto iter = reply.begin(); iter != reply.end(); ++iter)
			{
				std::cout << *iter << std::endl;
			}
		}, "get key%d", i);
	}
	proxy.FrameEnd();
	//system("pause");

	///////////////////////1////////////////////////
	RedisProxy proxy1;
	proxy1.Init(std::string("127.0.0.1"), 6379, RedisProxy::ECM_BOTH);
	proxy1.FrameBegin();
	for (int i = 0; i < 100000; ++i)
	{
		proxy1.SendAsyncCommand(i);
	}
	proxy1.FrameEnd();
	//system("pause");

	///////////////////////0////////////////////////
	proxy.FrameBegin();
	for (int i = 0; i < 100000; ++i)
	{
		proxy.SendAsyncCommand([](bool suc, auto reply) {

		}, "set key%d %d", i, i);
	}
	proxy.FrameEnd();
	//system("pause");

	///////////////////////1////////////////////////
	proxy1.FrameBegin();
	for (int i = 0; i < 100000; ++i)
	{
		proxy1.SendAsyncCommand([](bool suc, auto reply) {

		}, "set key%d %d", i, i);
	}
	proxy1.FrameEnd();
	//system("pause");

#if _DEBUG
	LARGE_INTEGER	litmp;
	LONGLONG		llLastCounter;
	double			dfFreq;
#endif
#if _DEBUG
	QueryPerformanceFrequency(&litmp);
	dfFreq = (double)litmp.QuadPart;
	QueryPerformanceCounter(&litmp);
	llLastCounter = litmp.QuadPart;
#endif

	for (int i = 0; i < 100000; ++i)
	{
		proxy1.SendSyncCommand([](bool suc, auto reply) {
		}, "set key%d %d", 1, 1);
	}

#if _DEBUG
	QueryPerformanceCounter(&litmp);
	double dfIntervel = (double)(litmp.QuadPart - llLastCounter) * 1000 / dfFreq;
	std::cout << "ms : " << dfIntervel << std::endl;
#endif


	proxy1.Destory();
	proxy.Destory();
	system("pause");
	return 0;
}

