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
		MU8STRV("group/\"*\"/\"*\""),
		MU8STRV("simple/[0]"),
		MU8STRV("list/a"),
		MU8STRV("list/[0]/*/extra"),
		MU8STRV("\"weird/key/inner\""),

		// 错误路径（应报告错误）
		MU8STRV("no\"\"/\"*"),
		MU8STRV("nono\"*/\"/"),
		MU8STRV("\"bad\\escape\""),
		MU8STRV("\"unclosed"),
		MU8STRV("ab\"cd"),
		MU8STRV("a//b"),
		MU8STRV("list/[-1]"),
		MU8STRV("list/[2]"),
		MU8STRV("list/[1..0]"),
		MU8STRV("list/[0..]extra"),
		MU8STRV("*a"),
		MU8STRV("a/b*/c"),
		MU8STRV("a/*b"),
		MU8STRV("foo/[*]"),
		MU8STRV("list/[0]/"),
		MU8STRV("/*"),
		MU8STRV("/"),
		MU8STRV("*/"),
		

		// 边界/特殊用例
		MU8STRV("list"),
		MU8STRV("empty"),
		MU8STRV("\"empty\"/\"\""),
		MU8STRV("\"list\""),
		MU8STRV("*"),
		MU8STRV("\"*\""),
		MU8STRV("\"*\"/*/\"*\"/*"),
		MU8STRV("*/*/*/*"),
		MU8STRV("*/foo"),
		MU8STRV("group/*/val"),
		MU8STRV("group/*"),
		MU8STRV("\"star_key\"/\"*\""),
	};

	for (const auto &it : test_paths)
	{
		NbtPath::PathInfo path;
		try
		{
			print("Parse Path: {}\n", MU8CV2CTU8(it));
			path = NbtPath::PathParser(it);
			print("Parse Success: {}\n\n", NbtPath::PathToString(path));
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

void testNbtErase()
{
	// ========== 构造测试 NBT 数据 ==========
	NBT_Type::Compound root;

	root.PutInt(MU8STR("Damage"), 1);                     // 用 REMOVE 删除
	root.PutFloat(MU8STR("TestFloat"), 0.0001f);          // 用 CLEAR 清空

	// Enchantments 列表（嵌套复合标签）
	NBT_Type::List enchantments;
	{
		NBT_Type::Compound ench1;
		ench1.PutString(MU8STR("id"), MU8STR("minecraft:infinity"));
		ench1.PutShort(MU8STR("lvl"), 1);
		enchantments.AddBack(std::move(ench1));

		NBT_Type::Compound ench2;
		ench2.PutString(MU8STR("id"), MU8STR("minecraft:flame"));
		ench2.PutShort(MU8STR("lvl"), 1);
		enchantments.AddBack(std::move(ench2));
	}
	root.Put(MU8STR("Enchantments"), std::move(enchantments));

	// 简单索引列表
	NBT_Type::List list_test;
	list_test.AddBack(NBT_Type::Int{ 1 });
	list_test.AddBack(NBT_Type::Int{ 2 });
	list_test.AddBack(NBT_Type::Int{ 3 });
	list_test.AddBack(NBT_Type::Int{ 4 });
	root.Put(MU8STR("list_test"), std::move(list_test));

	// 字节数组
	root.PutByteArray(MU8STR("byte_array_test"),
		{ NBT_Type::Byte(0x10), NBT_Type::Byte(0x20), NBT_Type::Byte(0x30),
		  NBT_Type::Byte(0x40), NBT_Type::Byte(0x50) });

	// ========== 定义删除/清空请求 ==========
	NBTErase::RequestList requests = {
		{ NbtPath::PathParser(MU8STRV("Damage")),                NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("TestFloat")),             NBTErase::Request::EraseMode::CLEAR  },
		{ NbtPath::PathParser(MU8STRV("Enchantments/[]/lvl")),   NBTErase::Request::EraseMode::CLEAR  },
		{ NbtPath::PathParser(MU8STRV("list_test/[0]")),         NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("byte_array_test/[1..2]")),NBTErase::Request::EraseMode::REMOVE },
	};

	// ========== 执行删除流程 ==========
	try
	{
		NBTErase::NbtParseToErase(root, NBTErase::EraseRequest2NbtPathTrieTree(requests));
	}
	catch (const NBTErase::EraseError &e)
	{
		print("NbtParseToErase Fail: {}\n", e.what());
		return;
	}
	catch (const std::exception &e)
	{
		print("NbtParseToErase exception: {}\n", e.what());
		return;
	}
	catch (...)
	{
		print("uunknown exception\n");
		return;
	}

	// ========== 验证 ==========
	auto &data = root.GetData();

	// 1. Damage 应被移除
	MyAssert(data.find(MU8STR("Damage")) == data.end());

	// 2. TestFloat 应被清空（float 默认值 0.0f）
	auto it_float = root.HasFloat(MU8STR("TestFloat"));
	MyAssert(it_float != nullptr);
	MyAssert(*it_float == 0.0f);

	// 3. Enchantments 列表中每个元素的 lvl 应为 Short(0)
	const auto &ench_list = root.GetList(MU8STR("Enchantments"));
	MyAssert(ench_list.Size() == 2);
	for (const auto &item : ench_list)
	{
		const auto &comp = item.GetCompound();
		auto lvl_it = comp.HasShort(MU8STR("lvl"));
		MyAssert(lvl_it != nullptr);
		MyAssert(*lvl_it == NBT_Type::Short{ 0 });
	}

	// 4. list_test 应变为 [2,3,4]
	const auto &lt = root.GetList(MU8STR("list_test"));
	MyAssert(lt.Size() == 3);
	MyAssert(lt[0].GetInt() == NBT_Type::Int{ 2 });
	MyAssert(lt[1].GetInt() == NBT_Type::Int{ 3 });
	MyAssert(lt[2].GetInt() == NBT_Type::Int{ 4 });

	// 5. byte_array_test 应变为 [0x10, 0x40, 0x50]
	const auto &ba = root.GetByteArray(MU8STR("byte_array_test"));
	MyAssert(ba.size() == 3);
	MyAssert(ba[0] == NBT_Type::Byte{ 0x10 });
	MyAssert(ba[1] == NBT_Type::Byte{ 0x40 });
	MyAssert(ba[2] == NBT_Type::Byte{ 0x50 });

	print("All tests passed!\n");
}

void testNbtErase2()
{
	// ================================================================
	// 构造更丰富的测试数据
	// ================================================================
	NBT_Type::Compound root;

	// -- 基础值 --
	root.PutInt(MU8STR("Damage"), 1);              // REMOVE
	root.PutFloat(MU8STR("TestFloat"), 3.14f);     // CLEAR
	root.PutString(MU8STR("Name"), MU8STR("Sword"));

	// -- 嵌套复合标签 --
	{
		NBT_Type::Compound inner;
		inner.PutInt(MU8STR("x"), 10);
		inner.PutInt(MU8STR("y"), 20);
		root.PutCompound(MU8STR("Coordinates"), std::move(inner));
	}

	// -- 通配符测试 --
	{
		NBT_Type::Compound group;
		group.PutInt(MU8STR("alpha"), 10);
		group.PutInt(MU8STR("beta"), 20);
		group.PutString(MU8STR("gamma"), MU8STR("hello"));
		root.PutCompound(MU8STR("group"), std::move(group));
	}

	{
		NBT_Type::Compound region;
		// 第一个子区域
		NBT_Type::Compound chunk1;
		chunk1.PutString(MU8STR("entity"), MU8STR("zombie"));
		chunk1.PutString(MU8STR("block"), MU8STR("stone"));
		region.PutCompound(MU8STR("chunk_1"), std::move(chunk1));

		// 第二个子区域
		NBT_Type::Compound chunk2;
		chunk2.PutString(MU8STR("entity"), MU8STR("skeleton"));
		chunk2.PutString(MU8STR("block"), MU8STR("dirt"));
		region.PutCompound(MU8STR("chunk_2"), std::move(chunk2));

		// 第三个子区域（没有 entity，用来验证不会误删）
		NBT_Type::Compound chunk3;
		chunk3.PutString(MU8STR("block"), MU8STR("sand"));
		region.PutCompound(MU8STR("chunk_3"), std::move(chunk3));

		root.PutCompound(MU8STR("region"), std::move(region));
	}

	// -- Enchantments 列表（每个元素为复合标签） --
	NBT_Type::List enchantments;
	{
		NBT_Type::Compound e1;
		e1.PutString(MU8STR("id"), MU8STR("sharpness"));
		e1.PutShort(MU8STR("lvl"), 3);
		enchantments.AddBack(std::move(e1));

		NBT_Type::Compound e2;
		e2.PutString(MU8STR("id"), MU8STR("unbreaking"));
		e2.PutShort(MU8STR("lvl"), 1);
		enchantments.AddBack(std::move(e2));

		NBT_Type::Compound e3;
		e3.PutString(MU8STR("id"), MU8STR("mending"));
		e3.PutShort(MU8STR("lvl"), 1);
		enchantments.AddBack(std::move(e3));
	}
	root.Put(MU8STR("Enchantments"), std::move(enchantments));

	NBT_Type::List listInt;
	for (int i = 0; i < 6; ++i)
		listInt.AddBack(NBT_Type::Int{ i });
	root.Put(MU8STR("IntList"), std::move(listInt));

	// -- 嵌套列表（列表中的列表） --
	NBT_Type::List nestedList;
	{
		NBT_Type::List inner1;
		inner1.AddBack(NBT_Type::Int{ 1 });
		inner1.AddBack(NBT_Type::Int{ 2 });
		nestedList.AddBack(std::move(inner1));

		NBT_Type::List inner2;
		inner2.AddBack(NBT_Type::Int{ 10 });
		inner2.AddBack(NBT_Type::Int{ 20 });
		inner2.AddBack(NBT_Type::Int{ 30 });
		nestedList.AddBack(std::move(inner2));
	}
	root.Put(MU8STR("NestedLists"), std::move(nestedList));

	// -- 字节数组 --
	root.PutByteArray(MU8STR("ByteArr"),
		{ NBT_Type::Byte(0xAA), NBT_Type::Byte(0xBB), NBT_Type::Byte(0xCC),
		  NBT_Type::Byte(0xDD), NBT_Type::Byte(0xEE) });

	// -- 整数数组 --
	root.PutIntArray(MU8STR("IntArr"),
		{ NBT_Type::Int{100}, NBT_Type::Int{200}, NBT_Type::Int{300},
		  NBT_Type::Int{400}, NBT_Type::Int{500} });

	// -- 长整数数组 --
	root.PutLongArray(MU8STR("LongArr"),
		{ NBT_Type::Long{1}, NBT_Type::Long{2}, NBT_Type::Long{3},
		  NBT_Type::Long{4}, NBT_Type::Long{5} });

	// -- 深层嵌套复合标签 --
	{
		NBT_Type::Compound level1;
		NBT_Type::Compound level2;
		NBT_Type::Compound level3;
		level3.PutString(MU8STR("leaf"), MU8STR("deep"));
		level2.PutCompound(MU8STR("L3"), std::move(level3));
		level2.PutInt(MU8STR("value"), 999);
		level1.PutCompound(MU8STR("L2"), std::move(level2));
		root.PutCompound(MU8STR("L1"), std::move(level1));
	}

	// ================================================================
	// 定义擦除请求（覆盖多种模式）
	// ================================================================
	NBTErase::RequestList requests = {
		{ NbtPath::PathParser(MU8STRV("Damage")),      NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("TestFloat")),   NBTErase::Request::EraseMode::CLEAR  },
		{ NbtPath::PathParser(MU8STRV("Coordinates/x")), NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("Enchantments/[]/lvl")), NBTErase::Request::EraseMode::CLEAR },
		{ NbtPath::PathParser(MU8STRV("IntList/[0]")),  NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("IntList/[2..3]")), NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("NestedLists/[1]/[1]")), NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("ByteArr/[1..3]")), NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("IntArr/[0]")),    NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("LongArr/[]")),    NBTErase::Request::EraseMode::CLEAR  },
		{ NbtPath::PathParser(MU8STRV("L1/L2/L3/leaf")), NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("L1/L2/value")),   NBTErase::Request::EraseMode::CLEAR  },
		{ NbtPath::PathParser(MU8STRV("NoSuchPath")),     NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("Enchantments/[10]/id")), NBTErase::Request::EraseMode::REMOVE },
		{ NbtPath::PathParser(MU8STRV("group/*")), NBTErase::Request::EraseMode::CLEAR },
		{ NbtPath::PathParser(MU8STRV("region/*/entity")), NBTErase::Request::EraseMode::REMOVE },
	};

	// ================================================================
	// 执行擦除
	// ================================================================
	try
	{
		NBTErase::NbtParseToErase(root, NBTErase::EraseRequest2NbtPathTrieTree(requests));
	}
	catch (const NBTErase::EraseError &e)
	{
		print("NbtParseToErase Fail: {}\n", e.what());
		return;
	}
	catch (const std::exception &e)
	{
		print("NbtParseToErase exception: {}\n", e.what());
		return;
	}
	catch (...)
	{
		print("uunknown exception\n");
		return;
	}

	// ================================================================
	// 验证（全部使用 printf 格式）
	// ================================================================
	// 1. Damage 已被删除
	MyAssert(root.HasInt(MU8STR("Damage")) == nullptr,
		"Damage should be removed");

	// 2. TestFloat 被清空为 0.0f
	float *pFloat = root.HasFloat(MU8STR("TestFloat"));
	MyAssert(pFloat != nullptr, "TestFloat should still exist");
	MyAssert(*pFloat == 0.0f, "TestFloat should be 0.0f");

	// 3. Name 未受影响
	auto *pName = root.HasString(MU8STR("Name"));
	MyAssert(pName != nullptr, "Name should still exist");
	MyAssert(*pName == MU8STR("Sword"), "Name should be Sword");

	// 4. Coordinates/y 仍存在，x 已删除
	const auto &coords = root.GetCompound(MU8STR("Coordinates"));
	MyAssert(coords.HasInt(MU8STR("y")) != nullptr,
		"Coordinates/y should still exist");
	MyAssert(coords.HasInt(MU8STR("x")) == nullptr,
		"Coordinates/x should be removed");

	// 5. Enchantments 列表仍为 3 个元素，每个 lvl 被清空为 0
	const auto &ench = root.GetList(MU8STR("Enchantments"));
	MyAssert(ench.Size() == 3, "Enchantments size should be 3, got %zu", ench.Size());
	for (size_t i = 0; i < ench.Size(); ++i)
	{
		const auto &comp = ench[i].GetCompound();
		const short *lvlPtr = comp.HasShort(MU8STR("lvl"));
		MyAssert(lvlPtr != nullptr, "Enchantment[%zu] should have lvl", i);
		MyAssert(*lvlPtr == 0, "Enchantment[%zu] lvl should be 0, got %d", i, (int)*lvlPtr);
		auto *idPtr = comp.HasString(MU8STR("id"));
		MyAssert(idPtr != nullptr, "Enchantment[%zu] should have id", i);
	}

	// 6. IntList 操作验证
	const auto &intList = root.GetList(MU8STR("IntList"));
	MyAssert(intList.Size() == 3, "IntList size should be 3, got %zu", intList.Size());
	MyAssert(intList[0].GetInt() == 1, "IntList[0] should be 1, got %d", (int)intList[0].GetInt());
	MyAssert(intList[1].GetInt() == 4, "IntList[1] should be 4, got %d", (int)intList[1].GetInt());
	MyAssert(intList[2].GetInt() == 5, "IntList[2] should be 5, got %d", (int)intList[2].GetInt());

	// 7. NestedLists 验证
	const auto &nested = root.GetList(MU8STR("NestedLists"));
	MyAssert(nested.Size() == 2, "NestedLists size should be 2, got %zu", nested.Size());
	const auto &innerList = nested[1].GetList();
	MyAssert(innerList.Size() == 2, "Inner list size should be 2, got %zu", innerList.Size());
	MyAssert(innerList[0].GetInt() == 10, "Inner[0] should be 10, got %d", (int)innerList[0].GetInt());
	MyAssert(innerList[1].GetInt() == 30, "Inner[1] should be 30, got %d", (int)innerList[1].GetInt());

	// 8. ByteArr 验证
	const auto &byteArr = root.GetByteArray(MU8STR("ByteArr"));
	MyAssert(byteArr.size() == 2, "ByteArr size should be 2, got %zu", byteArr.size());
	MyAssert(byteArr[0] == NBT_Type::Byte{ (int8_t)0xAA }, "ByteArr[0] should be 0xAA, got 0x%02X", (unsigned char)byteArr[0]);
	MyAssert(byteArr[1] == NBT_Type::Byte{ (int8_t)0xEE }, "ByteArr[1] should be 0xEE, got 0x%02X", (unsigned char)byteArr[1]);

	// 9. IntArr 验证
	const auto &intArr = root.GetIntArray(MU8STR("IntArr"));
	MyAssert(intArr.size() == 4, "IntArr size should be 4, got %zu", intArr.size());
	MyAssert(intArr[0] == NBT_Type::Int{ 200 }, "IntArr[0] should be 200, got %d", (int)intArr[0]);
	MyAssert(intArr[3] == NBT_Type::Int{ 500 }, "IntArr[3] should be 500, got %d", (int)intArr[3]);

	// 10. LongArr 全清空验证
	const auto &longArr = root.GetLongArray(MU8STR("LongArr"));
	MyAssert(longArr.size() == 5, "LongArr size should be 5, got %zu", longArr.size());
	for (size_t i = 0; i < longArr.size(); ++i)
	{
		MyAssert(longArr[i] == NBT_Type::Long{ 0 },
			"LongArr[%zu] should be 0, got %lld", i, (long long)longArr[i]);
	}

	// 11. 深层嵌套 leaf 删除验证
	const auto &l1 = root.GetCompound(MU8STR("L1"));
	const auto &l2 = l1.GetCompound(MU8STR("L2"));
	const auto &l3 = l2.GetCompound(MU8STR("L3"));
	MyAssert(l3.HasString(MU8STR("leaf")) == nullptr,
		"L3/leaf should be removed");

	// 12. L1/L2/value 被清空为 0
	const int *pVal = l2.HasInt(MU8STR("value"));
	MyAssert(pVal != nullptr, "L2/value should still exist");
	MyAssert(*pVal == 0, "L2/value should be 0, got %d", *pVal);

	// 13. 通配符删除验证
	const auto &grp = root.GetCompound(MU8STR("group"));
	MyAssert(grp.HasInt(MU8STR("alpha")) != nullptr && *grp.HasInt(MU8STR("alpha")) == 0);
	MyAssert(grp.HasInt(MU8STR("beta")) != nullptr && *grp.HasInt(MU8STR("beta")) == 0);
	auto *gamma = grp.HasString(MU8STR("gamma"));
	MyAssert(gamma != nullptr && gamma->empty());

	// 14. 通配符验证2
	const auto &region = root.GetCompound(MU8STR("region"));
	// chunk_1: entity 被删除，block 仍存在
	const auto &c1 = region.GetCompound(MU8STR("chunk_1"));
	MyAssert(c1.HasString(MU8STR("entity")) == nullptr, "chunk_1/entity should be removed");
	auto *b1 = c1.HasString(MU8STR("block"));
	MyAssert(b1 != nullptr && *b1 == MU8STR("stone"), "chunk_1/block should be 'stone'");

	// chunk_2: entity 被删除，block 仍存在
	const auto &c2 = region.GetCompound(MU8STR("chunk_2"));
	MyAssert(c2.HasString(MU8STR("entity")) == nullptr, "chunk_2/entity should be removed");
	auto *b2 = c2.HasString(MU8STR("block"));
	MyAssert(b2 != nullptr && *b2 == MU8STR("dirt"), "chunk_2/block should be 'dirt'");

	// chunk_3: 本来就没有 entity，block 不受影响
	const auto &c3 = region.GetCompound(MU8STR("chunk_3"));
	MyAssert(c3.HasString(MU8STR("entity")) == nullptr, "chunk_3/entity should not exist");
	auto *b3 = c3.HasString(MU8STR("block"));
	MyAssert(b3 != nullptr && *b3 == MU8STR("sand"), "chunk_3/block should be 'sand'");

	print("All extended tests passed!\n");
}

void TrieTreeTest(void)
{
	NBTErase::RequestList req
	{
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("test/[0..3]/foo/bar")),	.enMode = NBTErase::NBTErase::Request::EraseMode::REMOVE },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("test/[0..3]/foo/baz")),	.enMode = NBTErase::NBTErase::Request::EraseMode::REMOVE },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("test/[0..3]/foo")),		.enMode = NBTErase::NBTErase::Request::EraseMode::CLEAR },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("test/[1..2]/foo/bar")),	.enMode = NBTErase::NBTErase::Request::EraseMode::REMOVE },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("test/[0..3]/qux")),		.enMode = NBTErase::NBTErase::Request::EraseMode::UNKNOWN },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("other/leaf")),			.enMode = NBTErase::NBTErase::Request::EraseMode::REMOVE },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("other/branch/deep")),	.enMode = NBTErase::NBTErase::Request::EraseMode::REMOVE },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("a/b/c/d/e")),			.enMode = NBTErase::NBTErase::Request::EraseMode::UNKNOWN },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("a/b/c/d")),				.enMode = NBTErase::NBTErase::Request::EraseMode::REMOVE },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("a/b/c")),				.enMode = NBTErase::NBTErase::Request::EraseMode::REMOVE },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("a/b/c/[9..]")),			.enMode = NBTErase::NBTErase::Request::EraseMode::REMOVE },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("x/[0]/y")),				.enMode = NBTErase::NBTErase::Request::EraseMode::CLEAR },
		NBTErase::NBTErase::Request{.stNbtPath = NbtPath::PathParser(MU8STRV("x/[1]/z")),				.enMode = NBTErase::NBTErase::Request::EraseMode::CLEAR },
	};

	auto tt = NBTErase::EraseRequest2NbtPathTrieTree(req);
	print("{}\n", tt.ToString());
}

int main(void)
{
	ParseTest();
	TrieTreeTest();
	testNbtErase();
	testNbtErase2();

	return 0;
}



