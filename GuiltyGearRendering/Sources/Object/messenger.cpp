#include "object/messenger.h"

messenger::messenger()
: receivers()
, increment_key(0)
{
}

messenger::~messenger()
{
}

void messenger::clear()
{
	receivers.clear();
}

void messenger::send_data(std::string_view identifier, void* data)
{
	auto itRange = receivers.equal_range(std::string(identifier));
	for(decltype(itRange.first) it = itRange.first; it != itRange.second; ++it )
	{
		it->second.function(data);
	}
}

uint64_t messenger::add_receiver(std::string_view identifier, receiver receiver)
{
	receivers.insert(std::make_pair(std::string(identifier), data(increment_key, receiver)));
	return increment_key++;
}

void messenger::remove_receiver(uint64_t key)
{
	auto it = receivers.begin();
	auto itE = receivers.end();
	while(it != itE)
	{
		if( it->second.key == key )
		{
			receivers.erase(it);
			break;
		}
		++it;
	}
}

