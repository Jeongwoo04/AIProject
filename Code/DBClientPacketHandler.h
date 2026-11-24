#pragma once
#include "DBProtocol.pb.h"

//#define USE_OPTIMIZED_SENDBUFFER_CHUNK

#if UE_BUILD_DEBUG + UE_BUILD_DEVELOPMENT + UE_BUILD_TEST + UE_BUILD_SHIPPING >= 1
#include "S1.h"
#endif

using PacketHandlerFunc = std::function<bool(PacketSessionRef&, BYTE*, int32)>;

enum : uint16
{
	PKT_GS2DB_LOGIN = 1000,
	PKT_DB2GS_LOGIN = 1001,
	PKT_GS2DB_MESSAGE = 1002,
	PKT_DB2GS_MESSAGE = 1003,
	PKT_GS2DB_PLAYER_LOGIN_REQ = 1004,
	PKT_DB2GS_PLAYER_LOGIN_RES = 1005,
	PKT_GS2DB_ITEM_ACQUIRE_REQ = 1006,
	PKT_DB2GS_ITEM_ACQUIRE_RES = 1007,
	PKT_GS2DB_INVENTORY_MOVE_REQ = 1008,
	PKT_DB2GS_INVENTORY_MOVE_RES = 1009,
};

// Custom Handlers
#ifndef HANDLE_INVALID_DEFINED
#define HANDLE_INVALID_DEFINED
inline bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	// TODO : Log
	return false;
}
#endif
bool Handle_DB2GS_LOGIN(PacketSessionRef& session, Protocol::DB2GS_LOGIN& pkt);
bool Handle_DB2GS_MESSAGE(PacketSessionRef& session, Protocol::DB2GS_MESSAGE& pkt);
bool Handle_DB2GS_PLAYER_LOGIN_RES(PacketSessionRef& session, Protocol::DB2GS_PLAYER_LOGIN_RES& pkt);
bool Handle_DB2GS_ITEM_ACQUIRE_RES(PacketSessionRef& session, Protocol::DB2GS_ITEM_ACQUIRE_RES& pkt);
bool Handle_DB2GS_INVENTORY_MOVE_RES(PacketSessionRef& session, Protocol::DB2GS_INVENTORY_MOVE_RES& pkt);

class DBClientPacketHandler
{
public:
	static PacketHandlerFunc* GetPacketHandler()
	{
		static PacketHandlerFunc packetHandler[UINT16_MAX];
		return packetHandler;
	}

	static void Init()
	{
		PacketHandlerFunc* packetHandler = GetPacketHandler();
		for (int32 i = 0; i < UINT16_MAX; i++)
			packetHandler[i] = Handle_INVALID;
		packetHandler[PKT_DB2GS_LOGIN] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::DB2GS_LOGIN>(Handle_DB2GS_LOGIN, session, buffer, len); };
		packetHandler[PKT_DB2GS_MESSAGE] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::DB2GS_MESSAGE>(Handle_DB2GS_MESSAGE, session, buffer, len); };
		packetHandler[PKT_DB2GS_PLAYER_LOGIN_RES] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::DB2GS_PLAYER_LOGIN_RES>(Handle_DB2GS_PLAYER_LOGIN_RES, session, buffer, len); };
		packetHandler[PKT_DB2GS_ITEM_ACQUIRE_RES] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::DB2GS_ITEM_ACQUIRE_RES>(Handle_DB2GS_ITEM_ACQUIRE_RES, session, buffer, len); };
		packetHandler[PKT_DB2GS_INVENTORY_MOVE_RES] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::DB2GS_INVENTORY_MOVE_RES>(Handle_DB2GS_INVENTORY_MOVE_RES, session, buffer, len); };
	}

	static bool HandlePacket(PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
		return GetPacketHandler()[header->id](session, buffer, len);
	}
	static SendBufferRef MakeSendBuffer(Protocol::GS2DB_LOGIN& pkt) { return MakeSendBuffer(pkt, PKT_GS2DB_LOGIN); }
	static SendBufferRef MakeSendBuffer(Protocol::GS2DB_MESSAGE& pkt) { return MakeSendBuffer(pkt, PKT_GS2DB_MESSAGE); }
	static SendBufferRef MakeSendBuffer(Protocol::GS2DB_PLAYER_LOGIN_REQ& pkt) { return MakeSendBuffer(pkt, PKT_GS2DB_PLAYER_LOGIN_REQ); }
	static SendBufferRef MakeSendBuffer(Protocol::GS2DB_ITEM_ACQUIRE_REQ& pkt) { return MakeSendBuffer(pkt, PKT_GS2DB_ITEM_ACQUIRE_REQ); }
	static SendBufferRef MakeSendBuffer(Protocol::GS2DB_INVENTORY_MOVE_REQ& pkt) { return MakeSendBuffer(pkt, PKT_GS2DB_INVENTORY_MOVE_REQ); }

private:
	template<typename PacketType, typename ProcessFunc>
	static bool HandlePacket(ProcessFunc func, PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketType pkt;
		if (pkt.ParseFromArray(buffer + sizeof(PacketHeader), len - sizeof(PacketHeader)) == false)
			return false;

		return func(session, pkt);
	}

	template<typename T>
	static SendBufferRef MakeSendBuffer(T& pkt, uint16 pktId)
	{
		const uint16 dataSize = static_cast<uint16>(pkt.ByteSizeLong());
		const uint16 packetSize = dataSize + sizeof(PacketHeader);

#if UE_BUILD_DEBUG + UE_BUILD_DEVELOPMENT + UE_BUILD_TEST + UE_BUILD_SHIPPING >= 1
		SendBufferRef sendBuffer = MakeShared<SendBuffer>(packetSize);
#else
#ifdef USE_OPTIMIZED_SENDBUFFER_CHUNK
		SendBufferRef sendBuffer = GSendBufferManager->Open(packetSize);
#else
		SendBufferRef sendBuffer = make_shared<SendBuffer>(packetSize);
#endif
#endif

		PacketHeader* header = reinterpret_cast<PacketHeader*>(sendBuffer->Buffer());
		header->size = packetSize;
		header->id = pktId;
		pkt.SerializeToArray(&header[1], dataSize);
		sendBuffer->Close(packetSize);

		return sendBuffer;
	}
};