#pragma once
#include <Types.h>

using Result = std::expected<void, std::string>;
class IocpCore;
class IocpObject;
class Session;
class PacketSession;
class Service;
class ServerService;
class ClientService;
class Listener;
class SendBuffer;
class Job;
class JobQueue;
using IocpCorePtr = std::shared_ptr<IocpCore>;
using IocpObjectPtr = std::shared_ptr<IocpObject>;
using SessionPtr = std::shared_ptr<Session>;
using PacketSessionPtr = std::shared_ptr<PacketSession>;
using ServerServicePtr = std::shared_ptr<ServerService>;
using ClientServicePtr = std::shared_ptr<ClientService>;
using ListenerPtr = std::shared_ptr<Listener>;
using SendBufferPtr = std::shared_ptr<const SendBuffer>;
using JobPtr = std::shared_ptr<Job>;
using JobQueuePtr = std::shared_ptr<JobQueue>;
