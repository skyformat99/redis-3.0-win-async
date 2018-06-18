#pragma once
#include <iostream>
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <functional>

struct aeEventLoop;
struct redisAsyncContext;
struct redisContext;
class RedisProxy
{
public:
	enum EConnectModel
	{
		ECM_BOTH,
		ECM_ASYNC,
		ECM_SYNC,
	};
	typedef std::function<void(bool, const std::vector<std::string>&&)> callback_t;
public:
	bool Init(std::string& strIP, int nPort, EConnectModel eModel);
	void Destory();

	/* sync interface */
	void SendSyncCommand(const callback_t& cb, const char* fmt, ...);

	/* async interface */
	void FrameBegin();													//帧开始调用
	void SendAsyncCommand(int key = 0);									//帧中调用
	void SendAsyncCommand(const callback_t& cb, const char* fmt, ...);	//帧中调用
	void FrameEnd();													//帧结束调用

	void DisconnectCallback(const struct redisAsyncContext* c, int status);
	void ConnectCallback(const struct redisAsyncContext* c, int status);
	void CommandCallback(redisAsyncContext *c, void *r, void *privdata);
private:
	/* sync interface */
	bool SyncConnect(const char* strIP, int nPort);

	/* async interface */
	void AsyncReconnect();
	bool AsyncConnect(const char* strIP, int nPort);

private:
	std::string				m_strIP;
	int						m_nPort = 0;

	/* sync member */
	redisContext*			m_Cxt = nullptr;
	bool					m_bSyncConnect = false;

	/* async member */
	int						m_AsyncCommandCount = 0;
	aeEventLoop*			m_ELoop = nullptr;
	redisAsyncContext*		m_ACxt = nullptr;
	bool					m_bAsyncConnect = false;
	std::list<callback_t>	m_listCallback;

#if _DEBUG
	LARGE_INTEGER	litmp;
	LONGLONG		llLastCounter;
	double			dfFreq;
#endif
};