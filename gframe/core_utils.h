#ifndef CORE_UTILS_H_
#define CORE_UTILS_H_
#include <vector>
#include <cstdint>
#include "bufferio.h"
namespace ygo {
class ClientCard;
}
namespace CoreUtils {
class Packet {
public:
	Packet() {}
	Packet(const char* buf, int len) {
		uint8_t msg = BufferIO::Read<uint8_t>(buf);
		Set(msg, buf, len);
	};
	Packet(int msg, const char* buf, int len) {
		Set(msg, buf, len);
	};
	void Set(int msg, const char* buf, int len) {
		message = msg;
		buffer.resize(len);
		if(len)
			memcpy(buffer.data(), buf, len);
	};
	char* data() { return reinterpret_cast<char*>(buffer.data()); }
	const char* data() const { return reinterpret_cast<const char*>(buffer.data()); }
	uint8_t message;
	std::vector<uint8_t> buffer;
	auto size() const { return buffer.size() + sizeof(uint8_t); }
	auto buff_size() const { return buffer.size(); }
};
class PacketStream {
public:
	std::vector<Packet> packets;
	PacketStream() {}
	PacketStream(char* buf, uint32_t len);
};
struct loc_info {
	uint8_t controler;
	uint8_t location;
	uint32_t sequence;
	uint32_t position;
};
loc_info ReadLocInfo(const char*& p, bool compat);
loc_info ReadLocInfo(char*& p, bool compat);
class Query {
public:
	friend class QueryStream;
	friend class ygo::ClientCard;
	Query() = delete;
	Query(const char* buff, bool compat = false, uint32_t len = 0) { if(compat) ParseCompat(buff, len); else Parse(buff); };
	void GenerateBuffer(std::vector<uint8_t>& len, bool is_for_public_buffer, bool check_hidden) const;
	struct Token {};
	Query(Token, const char*& buff) { Parse(buff); };
private:
	void Parse(const char*& buff);
	void ParseCompat(const char* buff, uint32_t len);
	bool onfield_skipped = false;
	uint32_t flag;
	uint32_t code;
	uint32_t position;
	uint32_t alias;
	uint32_t type;
	uint32_t level;
	uint32_t rank;
	uint32_t link;
	uint32_t attribute;
	uint32_t race;
	int32_t attack;
	int32_t defense;
	int32_t base_attack;
	int32_t base_defense;
	uint32_t reason;
	uint8_t owner;
	uint32_t status;
	uint8_t is_public;
	uint32_t lscale;
	uint32_t rscale;
	uint32_t link_marker;
	loc_info reason_card;
	loc_info equip_card;
	uint8_t is_hidden;
	uint32_t cover;
	std::vector<loc_info> target_cards;
	std::vector<uint32_t> overlay_cards;
	std::vector<uint32_t> counters;
	bool IsPublicQuery(uint32_t to_check_flag) const;
	uint32_t GetFlagSize(uint32_t to_check_flag) const;
	uint32_t GetSize() const;
};
class QueryStream {
public:
	QueryStream() = delete;
	QueryStream(const char* buff, bool compat = false, uint32_t len = 0) { if(compat) ParseCompat(buff, len); else Parse(buff); };
	void GenerateBuffer(std::vector<uint8_t>& buffer, bool check_hidden) const;
	void GeneratePublicBuffer(std::vector<uint8_t>& buffer) const;
	const std::vector<Query>& GetQueries() const { return queries; }
private:
	std::vector<Query> queries;
	void Parse(const char* buff);
	void ParseCompat(const char* buff, uint32_t len);
	uint32_t GetSize() const;
};
using OCG_Duel = void*;
PacketStream ParseMessages(OCG_Duel duel);
};

#define HINT_SKILL        200
#define HINT_SKILL_COVER  201
#define HINT_SKILL_FLIP   202
#define HINT_SKILL_REMOVE 203

#endif
