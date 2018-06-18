#include "RedisProxy.h"

#include <hiredis.h>
#include <async.h>
extern "C" {
#include <adapters\ae.h>
}
#pragma comment(lib, "hiredis.lib")  
#pragma comment(lib, "Win32_Interop.lib")


static void sDisconnectCallback(const struct redisAsyncContext* c, int status)
{ 
	if (c && c->data)
	{
		 ((RedisProxy*)(c->data))->DisconnectCallback(c, status);
	}
}
static void sConnectCallback(const struct redisAsyncContext* c, int status)
{
	if (c && c->data)
	{
		((RedisProxy*)(c->data))->ConnectCallback(c, status);
	}
}

static void sCommandCallback(redisAsyncContext *c, void *r, void *privdata)
{
	if (c && c->data)
	{
		((RedisProxy*)(c->data))->CommandCallback(c, r, privdata);
	}
}

void RedisProxy::DisconnectCallback(const struct redisAsyncContext* c, int status)
{
	if (m_AsyncCommandCount <= 0)
		aeStop(m_ELoop);

	if (status != REDIS_OK) 
	{
		std::cout <<"Redis DisconnectCallback Error : " << c->errstr << std::endl;
	}
	m_bAsyncConnect = false;
}
void RedisProxy::ConnectCallback(const struct redisAsyncContext* c, int status)
{
	if (m_AsyncCommandCount <= 0)
		aeStop(m_ELoop);

	if (status != REDIS_OK)
	{
		std::cout << "Redis ConnectCallback Error : " << c->errstr << std::endl;
	}
	m_bAsyncConnect = true;
}
void RedisProxy::CommandCallback(redisAsyncContext *c, void *r, void *privdata)
{
	redisReply *reply = (redisReply *)r;
	bool suc = false;
	std::vector<std::string> vecReply;
	if (reply == NULL)
	{
		suc = false;
	}
	else
	{
		suc = true;
		if (reply->type == REDIS_REPLY_STRING || 
			reply->type == REDIS_REPLY_STATUS)
		{
			vecReply.push_back(reply->str);
		}
		else if (reply->type == REDIS_REPLY_INTEGER)
		{
			vecReply.push_back(std::to_string(reply->integer));
		}
		else if (reply->type == REDIS_REPLY_ARRAY)
		{
			for (int i = 0; i < reply->elements; ++i)
			{
				if (reply->element[i]->str)
				{
					vecReply.push_back(reply->element[i]->str);
				}
			}
		}
		else if (reply->type == REDIS_REPLY_ERROR)
		{
			vecReply.push_back(reply->str);
			suc = false;
		}
	}

	if (privdata)
	{
		callback_t* pCb = (callback_t*)privdata;
		(*pCb)(suc, std::move(vecReply));
	}

	if (--m_AsyncCommandCount <= 0)
		aeStop(m_ELoop);
}

bool RedisProxy::Init(std::string& strIP, int nPort, EConnectModel eModel)
{
	m_ELoop = aeCreateEventLoop(1024 * 10);
	m_strIP = strIP;
	m_nPort = nPort;
	switch (eModel)
	{
	case RedisProxy::ECM_BOTH:
		return AsyncConnect(m_strIP.c_str(), m_nPort) && SyncConnect(m_strIP.c_str(), m_nPort);
		break;
	case RedisProxy::ECM_ASYNC:
		return AsyncConnect(m_strIP.c_str(), m_nPort);
		break;
	case RedisProxy::ECM_SYNC:
		break;
		return SyncConnect(m_strIP.c_str(), m_nPort);
	default:
		break;
	}
	return false;
}
void RedisProxy::Destory()
{
	if (m_ACxt)
	{
		//请不要在aeMain()返回前调用，也就是不要在事件循环前调用（官方代码一般在某个指令回调中调用也是不可行），会有可能crash
		//crash位置 async.c 370行 原因猜测是当一次性异步发送的指令过多，导致内部无法一次性发包，也无法一次性处理全部回调
		//所以在async.c 452行 仅判断输出缓冲区而不判断回调列表是否为空就disconnect会出现异常。
		//处理方案要么改async.c 452行的判断逻辑 要么就是限制redisAsyncDisconnect调用时机。
		redisAsyncDisconnect(m_ACxt);
	}

	if (m_ELoop)
	{
		aeDeleteEventLoop(m_ELoop);
	}

	if (m_Cxt)
	{
		redisFree(m_Cxt);
	}
}
bool RedisProxy::AsyncConnect(const char* strIP, int nPort)
{
	m_ACxt = redisAsyncConnect(strIP, nPort);
	if (!m_ACxt || m_ACxt->err)
	{
		if (m_ACxt)
		{
			redisAsyncFree(m_ACxt);
		}
		return false;
	}
	m_ACxt->data = this;
	redisAeAttach(m_ELoop, m_ACxt);
	redisAsyncSetConnectCallback(m_ACxt, sConnectCallback);
	redisAsyncSetDisconnectCallback(m_ACxt, sDisconnectCallback);
	return true;
}
void RedisProxy::AsyncReconnect()
{
	std::cout << "Redis Reconnect..."<< std::endl;
	AsyncConnect(m_strIP.c_str(), m_nPort);
}
void RedisProxy::FrameBegin()
{
#if _DEBUG
	QueryPerformanceFrequency(&litmp);
	dfFreq = (double)litmp.QuadPart;
	QueryPerformanceCounter(&litmp);
	llLastCounter = litmp.QuadPart;
#endif
}
void RedisProxy::FrameEnd()
{
	aeMain(m_ELoop);

#if _DEBUG
	QueryPerformanceCounter(&litmp);
	double dfIntervel = (double)(litmp.QuadPart - llLastCounter) * 1000 / dfFreq;
	std::cout << "ms : " << dfIntervel << std::endl;
#endif

	m_listCallback.clear();
	if (!m_bAsyncConnect)
	{
		AsyncReconnect();
	}

	if (m_AsyncCommandCount != 0)
	{

	}
}
void RedisProxy::SendAsyncCommand(int key/* = 0*/)
{
	++m_AsyncCommandCount;
	redisAsyncCommand(m_ACxt, sCommandCallback, nullptr, "set key%d %d", key);
}
void RedisProxy::SendAsyncCommand(const callback_t& cb, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	++m_AsyncCommandCount;
	m_listCallback.push_back(std::move(cb));
	redisvAsyncCommand(m_ACxt, sCommandCallback, (void*)(&(*m_listCallback.rbegin())), fmt, args);
	//redisvAsyncCommand(m_ACxt, sCommandCallback, nullptr, fmt, args);
	va_end(args);
}
/* sync interface */
void RedisProxy::SendSyncCommand(const callback_t& cb, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	redisReply* pReply = (redisReply*)redisvCommand(m_Cxt, fmt, args);
	if (pReply == nullptr)
	{
		redisFree(m_Cxt);
		if (SyncConnect(m_strIP.c_str(), m_nPort) == false)
		{
			m_bSyncConnect = false;
		}
		else
		{ 
			pReply = (redisReply*)redisvCommand(m_Cxt, fmt, args);
		}
	}

	bool suc = false;
	std::vector<std::string> vecReply;
	if (pReply == NULL)
	{
		suc = false;
	}
	else
	{
		suc = true;
		if (pReply->type == REDIS_REPLY_STRING ||
			pReply->type == REDIS_REPLY_STATUS)
		{
			vecReply.push_back(pReply->str);
		}
		else if (pReply->type == REDIS_REPLY_INTEGER)
		{
			vecReply.push_back(std::to_string(pReply->integer));
		}
		else if (pReply->type == REDIS_REPLY_ARRAY)
		{
			for (int i = 0; i < pReply->elements; ++i)
			{
				if (pReply->element[i]->str)
				{
					vecReply.push_back(pReply->element[i]->str);
				}
			}
		}
		else if (pReply->type == REDIS_REPLY_ERROR)
		{
			vecReply.push_back(pReply->str);
			suc = false;
		}
	}

	cb(suc, std::move(vecReply));
	va_end(args);
}
bool RedisProxy::SyncConnect(const char* strIP, int nPort)
{
	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	m_Cxt = redisConnectWithTimeout(strIP, nPort, timeout);
	if (m_Cxt == NULL || m_Cxt->err) 
	{
		if (m_Cxt) 
		{
			redisFree(m_Cxt);
		}
		m_bSyncConnect = false;
		return false;
	}
	return true;
}
