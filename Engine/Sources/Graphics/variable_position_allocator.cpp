#include "system/variable_position_allocator.h"

namespace
{
	//	32バイト目は生存フラグで使用
	static constexpr UINT alive = 1 << 31;

	bool is_flag(UINT value, UINT flag)
	{
		return value & flag;
	}

	UINT flag_set(UINT value, UINT flag)
	{
		return value | flag;
	}

	UINT flag_erase(UINT value, UINT flag)
	{
		return value & ~flag;
	}
}

//	pool_size = 2,147,483,647バイトまで
bool variable_position_allocator::initialize(UINT pool_size)
{
	if (pool_size >= flag_erase(0xFFFFFFFF, alive))
		return false;

	node_list.clear();
	pool_size = flag_erase(pool_size, alive);
	node_list.emplace_back(pool_size);
	return true;
}

UINT variable_position_allocator::allocate(UINT size)
{
	auto it = node_list.begin();

	UINT position = 0;
	while (it != node_list.end())
	{
		if (!is_flag(*it, alive))
		{
			UINT pool_size = *it;
			if (size == pool_size)
			{
				*it = flag_set(size, alive);
				return position;
			}
			if (size < pool_size)
			{
				pool_size -= size;
				*it = flag_set(size, alive);
				node_list.insert(++it, pool_size);

				return position;
			}
		}
		//	座標に加算
		position += flag_erase(*it, alive);
		++it;
	}

	return allocate_failed_code;
}

bool variable_position_allocator::deallocate(UINT deallocate_position)
{
	if (deallocate_position == allocate_failed_code)
		return false;

	auto it = node_list.begin();
	auto it_prev = node_list.end();

	UINT position = 0;
	while (it != node_list.end())
	{
		if (deallocate_position == position && is_flag(*it, alive))
		{
			*it = flag_erase(*it, alive);

			auto it_next = ++it;
			--it;

			//	一つ前と一つ後ろを合成できるなら合成する
			if (it_prev != node_list.end() && !is_flag(*it_prev, alive))
			{
				*it_prev += *it;
				node_list.erase(it);
				it = it_prev;
			}
			if (it_next != node_list.end() && !is_flag(*it_next, alive))
			{
				*it += *it_next;
				it = node_list.erase(it_next);
			}

			return true;
		}
		//	座標に加算
		position += flag_erase(*it, alive);
		it_prev = it;
		++it;
	}

	return false;
}
