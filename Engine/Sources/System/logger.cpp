#include "System/logger.h"

#include <crtdbg.h>
#include <stdarg.h>
#include <stdio.h>

logger::logger()
: log_max(1024 * 2)
{
}

logger::~logger()
{
}

void logger::print(categorys category, const char* format, ...)
{
	char message[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	OutputDebugStringA(message);
	register_log(category, levels::normal, message);
}

void logger::warning(categorys category, const char* format, ...)
{
	char message[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	OutputDebugStringA(message);
	register_log(category, levels::warning, message);
}

void logger::error(categorys category, const char* format, ...)
{
	char message[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	OutputDebugStringA(message);
	register_log(category, levels::error, message);
}

std::vector<const char*> logger::get_category_texts()
{
	std::vector<const char*> texts;
	for (auto category : magic_enum::enum_values<categorys>())
	{
		const char* text = magic_enum::enum_names<categorys>()[static_cast<int>(category)].data();
		if (categorys::max == category)
			text = "all";
		texts.push_back(text);
	}
	return texts;
}

std::vector<const char*> logger::get_levels_texts()
{
	std::vector<const char*> texts;
	for (auto level : magic_enum::enum_values<levels>())
	{
		const char* text = magic_enum::enum_names<levels>()[static_cast<int>(level)].data();
		if (levels::max == level)
			text = "all";
		texts.push_back(text);
	}
	return texts;
}

void logger::clear()
{
	logs.clear();
}

void logger::register_log(categorys category, levels level, const char* text)
{
	logs.emplace_back(category, level, text);
	while (logs.size() > log_max)
	{
		logs.pop_front();
	}
}
