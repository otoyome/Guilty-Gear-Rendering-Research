#pragma once

#include <unordered_map>

//	メッセンジャークラス
//	☆ﾃｷﾄｰ('ω')ｰﾄｷﾃ☆
class messenger
{
public:
	using function = void(void*);
	using receiver = std::function<function>;

	static constexpr uint64_t invalid_key = 0xFFFFFFFFFFFFFFFF;

public:
	messenger();
	~messenger();

	void clear();
	void send_data(std::string_view identifier, void* data);
	uint64_t add_receiver(std::string_view identifier, receiver receiver);
	void remove_receiver(uint64_t key);

private:
	struct data
	{
		uint64_t key;
		receiver function;
		data(uint64_t key, receiver function) : key(key), function(function){}
		bool operator==(data& r) const { return key == r.key; }
	};
	std::unordered_multimap<std::string, data> receivers;
	UINT increment_key;
};
