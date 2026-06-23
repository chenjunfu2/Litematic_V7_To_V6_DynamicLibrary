#include "NBT_Erase.hpp"

#include <stdio.h>


template<typename... Args>
void print(const std::format_string<Args...> fmt, Args&&... args)
{
	auto tmp = std::format(std::move(fmt), std::forward<Args>(args)...);
	fwrite(tmp.data(), sizeof(tmp.data()[0]), tmp.size(), stdout);
}

// 打印错误上下文：显示路径字符串，并用 ^ 指示错误位置
void print_error_context(const NBT_Type::String::View &str, size_t pos)
{
	auto u8Str = MU8CV2CTU8(str);
	// 计算字符偏移量（UTF‑8 列位置）
	size_t col = 0;   // 当前字符列索引
	size_t idx = 0;   // 字节索引
	while (idx < pos && idx < u8Str.size())
	{
		uint8_t c = u8Str[idx];
		size_t len = 1;
		if ((c & 0x80) == 0)
		{
			len = 1;
		}
		else if ((c & 0xE0) == 0xC0)
		{
			len = 2;
		}
		else if ((c & 0xF0) == 0xE0)
		{
			len = 3;
		}
		else if ((c & 0xF8) == 0xF0)
		{
			len = 4;
		}
		else
		{
			len = 1;
		}   // 非法字节按单字节处理
		if (idx + len > pos)
		{
			// 防止跨界点截断
			break;
		}
		idx += len;
		++col;
	}

	print("error: {}\n", u8Str); // 第一行：路径原文
	print("{:>{}}^\n", "", 7 + col);// 第二行：对齐指示符（"error: " 占 7 列，再加上 col 列空白）
}


void ParseTest(void)
{
	static constexpr NBT_Type::String::View test_paths[] =
	{
		// 有效路径（应成功定位）
		MU8STRV("simple/value"),
		MU8STRV("\"weird/key\"/inner"),
		MU8STRV("\"my\\\"quote\"/x"),
		MU8STRV("\"back\\\\slash\"/y"),
		MU8STRV("\"arr[0]\"/data"),
		MU8STRV("empty/\"\""),
		MU8STRV("list/[]"),
		MU8STRV("list/[]/a"),
		MU8STRV("list/[0]"),
		MU8STRV("list/[0]/a"),
		MU8STRV("list/[0]/b"),
		MU8STRV("list/[0..1]"),
		MU8STRV("list/[0..1]/a"),
		MU8STRV("list/[..0]"),
		MU8STRV("list/[1..]"),
		MU8STRV("nested_lists/[]"),
		MU8STRV("nested_lists/[0]"),
		MU8STRV("nested_lists/[0]/[]"),
		MU8STRV("nested_lists/[1]/[]"),
		MU8STRV("nested_lists/[]/[1]"),
		MU8STRV("nested_lists/[1]/[0]"),

		// 错误路径（应报告错误）
		MU8STRV("simple/[0]"),
		MU8STRV("list/a"),
		MU8STRV("list/[0]/a/extra"),
		MU8STRV("\"bad\\escape\""),
		MU8STRV("\"unclosed"),
		MU8STRV("ab\"cd"),
		MU8STRV("a//b"),
		MU8STRV("list/[-1]"),
		MU8STRV("list/[2]"),
		MU8STRV("list/[1..0]"),
		MU8STRV("list/[0..]extra"),
		MU8STRV("\"weird/key/inner\""),

		// 边界/特殊用例
		MU8STRV("list"),
		MU8STRV("empty"),
		MU8STRV("\"empty\"/\"\""),
		MU8STRV("\"list\""),
		MU8STRV("list/[0]/"),
	};

	for (const auto &it : test_paths)
	{
		NbtPath::PathInfo path;
		try
		{
			print("Parse Path: {}\n", MU8CV2CTU8(it));
			path = NbtPath::PathParser(it);
			print("Parse Success: {}\n\n", NbtPath::ToString(path));
		}
		catch (const NbtPath::ParseError &e)
		{
			print("Parse Fail: {}\n", e.what());
			//打印错误位置
			print_error_context(it, e.position);
		}
		catch (const std::exception &e)
		{
			print("Parse Exception: {}\n", e.what());
		}
		catch (...)
		{
			print("Unknown Exception\n");
		}
	}
}


int main(void)
{
	NBT_Type::Compound cpd{};
	NbtParseToErase(cpd, EraseRequestList{ EraseRequest{ .stNbtPath = NbtPath::PathParser(MU8STRV("test/[0..3]/foo/bar")), .enMode = EraseRequest::EraseMode::REMOVE } });










	return 0;
}
