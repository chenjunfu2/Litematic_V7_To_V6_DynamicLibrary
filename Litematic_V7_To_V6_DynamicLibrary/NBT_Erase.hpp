#pragma once

#include <nbt_cpp/NBT_All.hpp>
#include <util/MyAssert.hpp>

#include <charconv>
#include <format>
#include <functional>
#include <ranges>
#include <algorithm>
#include <type_traits>

#define MU8CV2CTU8(mu8String) MUTF8_Tool<MUTF8_Char_Type, char16_t, char>::MU8ToU8(mu8String)

struct NbtPath
{
	NbtPath(void) = delete;
	~NbtPath(void) = delete;

public:
	class ParseError : public std::runtime_error
	{
	public:
		const size_t position;
	public:
		ParseError(const char *message, size_t _position):
			std::runtime_error(std::format("{} at position {}", message, _position)), position(_position)
		{}

		ParseError(const std::string &message, size_t _position):
			std::runtime_error(std::format("{} at position {}", message, _position)), position(_position)
		{}
	};

public:
	struct NameStep
	{
		NBT_Type::String strStep = {};//默认空字符串

	public:
		void SetWildcard(void)
		{
			strStep.clear();
			strStep.push_back('\0');
			strStep.push_back('*');
		}

		static NBT_Type::String MakeWildcardString(void)
		{
			NBT_Type::String ret;
			ret.push_back('\0');
			ret.push_back('*');
			return ret;
		}

		bool IsWildcard(void) const
		{
			return strStep.size() == 2 && strStep[0] == '\0' && strStep[1] == '*';
		}

	public:
		size_t hash(void) const
		{
			return std::hash<NBT_Type::String>{}(strStep);
		}

		bool operator==(const NameStep &) const = default;

		std::string ToString(void) const
		{
			if (!strStep.empty() && strStep.find_first_of(MU8STR("/[]\"\\*")) == strStep.npos)//普通字符且非空键名
			{
				return std::format("{}/", strStep.ToCharTypeUTF8());
			}

			//确定是否是通配符
			if (IsWildcard())
			{
				return std::string("*/");
			}

			//需要引号并添加转义
			std::string newStr{};
			for (const auto &ch : strStep)
			{
				if ((char)ch == '"' || (char)ch == '\\')
				{
					newStr.push_back((char)'\\');
				}
				newStr.push_back((char)ch);
			}

			return std::format("\"{}\"/", newStr);
		}
	};

	struct IndexStep//默认值为全覆盖
	{
		size_t szBeg = 0;
		size_t szEnd = SIZE_MAX;

	public:
		size_t hash(void) const
		{
			return std::hash<size_t>{}(szBeg) ^ std::hash<size_t>{}(szEnd);
		}

		bool operator==(const IndexStep &) const = default;

		std::string ToString(void) const
		{
			if (szBeg == szEnd)
			{
				return std::format("[{}]/", szBeg);
			}
			else
			{
				if (szBeg == 0 && szEnd == SIZE_MAX)
				{
					return std::string("[]/");
				}
				else
				{
					std::string strBeg = szBeg == 0 ? "" : std::format("{}", szBeg);
					std::string strEnd = szEnd == SIZE_MAX ? "" : std::format("{}", szEnd);
					return std::format("[{}..{}]/", strBeg, strEnd);
				}
			}
		}
	};

	enum class StepType : size_t
	{
		NONE = -1,
		Name = 0,
		Index = 1,
	};

	using Step = std::variant<NameStep, IndexStep>;

	struct StepHash//透明哈希，允许使用非持有数据的类型进行等价哈希
	{
		using is_transparent = void;//透明标注类型

		std::size_t operator()(const NameStep &ns) const noexcept
		{
			return ns.hash();
		}
		std::size_t operator()(const NBT_Type::String &s) const noexcept
		{
			return std::hash<NBT_Type::String>{}(s);
		}
		std::size_t operator()(const IndexStep &is) const noexcept
		{
			return is.hash();
		}
		std::size_t operator()(const Step &s) const noexcept
		{
			return std::visit(
				[](const auto &v)
				{
					return v.hash();
				}, s);
		}
	};

	struct StepEqual//透明比较，允许使用非持有数据的类型进行等价比较
	{
		using is_transparent = void;//透明标注类型

		// Step ↔ Step（默认 variant 相等）
		bool operator()(const Step &lhs, const Step &rhs) const
		{
			return lhs == rhs;
		}

		// Step ↔ NameStep
		bool operator()(const Step &s, const NameStep &ns) const
		{
			const auto *p = std::get_if<NameStep>(&s);
			return p && *p == ns;
		}
		bool operator()(const NameStep &ns, const Step &s) const
		{
			return (*this)(s, ns);
		}

		// Step ↔ NBT_String
		bool operator()(const Step &s, const NBT_Type::String &ns) const
		{
			const auto *p = std::get_if<NameStep>(&s);
			return p && p->strStep == ns;
		}
		bool operator()(const NBT_Type::String &ns, const Step &s) const
		{
			return (*this)(s, ns);
		}

		// Step ↔ IndexStep
		bool operator()(const Step &s, const IndexStep &is) const
		{
			const auto *p = std::get_if<IndexStep>(&s);
			return p && *p == is;
		}
		bool operator()(const IndexStep &is, const Step &s) const
		{
			return (*this)(s, is);
		}
	};

protected:
	static size_t ParseNumber(const MUTF8_Char_Type *pBase, const NBT_Type::String::View &strNumber)
	{
		size_t szRet = 0;
		auto result = std::from_chars((const char *)strNumber.data(), (const char *)strNumber.data() + strNumber.size(), szRet);
		if (result.ptr != (const char *)strNumber.data() + strNumber.size())
		{
			throw ParseError(std::format("Invalid number format: '{}'", MU8CV2CTU8(strNumber)), result.ptr - (const char *)pBase);
		}

		switch (result.ec)
		{
		case std::errc{}:
			return szRet;
			break;
		case std::errc::invalid_argument:
			throw ParseError(std::format("Invalid number format: '{}'", MU8CV2CTU8(strNumber)), result.ptr - (const char *)pBase);
		case std::errc::result_out_of_range:
			throw ParseError(std::format("Number out of range: '{}'", MU8CV2CTU8(strNumber)), result.ptr - (const char *)pBase);
		default:
			throw ParseError("", result.ptr - (const char *)pBase);
			break;
		}
	}

	static Step ParseIndexStep(const MUTF8_Char_Type *pBase, NBT_Type::String::View &strPath)
	{
		//进入此处，当前字符[0]为'['
		size_t szBaseLSqBracketPos = strPath.data() - pBase;//记录 '[' 的位置
		size_t szRSqBracketPos = strPath.find_first_of(']', 1);//从[的下一个字符开始查找
		if (szRSqBracketPos == strPath.npos)
		{
			throw ParseError("Missing ']'", szBaseLSqBracketPos);
		}

		//获取不带右方括号的内部值
		NBT_Type::String::View strRange = strPath.substr(1, szRSqBracketPos - 1);
		strPath.remove_prefix(szRSqBracketPos + 1);//删除原先字符串方括号部分的内容

		//处理分隔符
		size_t szDelimiterBegPos = strRange.find_first_of('.');
		size_t szDelimiterEndPos = strRange.npos;
		if (szDelimiterBegPos != strRange.npos)
		{
			size_t szNextDelimiterPos = szDelimiterBegPos + 1;//下一个.的位置
			if (szNextDelimiterPos >= strRange.size())
			{
				throw ParseError("Expected '..' but only found '.' and nothing after", szBaseLSqBracketPos + 1 + szDelimiterBegPos);
			}
			if (strRange[szNextDelimiterPos] != '.')
			{
				throw ParseError("Expected '..' for range, found single '.'", szBaseLSqBracketPos + 1 + szDelimiterBegPos);
			}

			szDelimiterEndPos = szDelimiterBegPos + 2;//设置到尾后
		}
		else//[]格式或[I]格式
		{
			if (strRange.empty())//[]格式
			{
				return Step{ IndexStep{} };//默认值，全覆盖
			}
			else//[I]格式
			{
				size_t szIVal = ParseNumber(pBase, strRange);
				return Step{ IndexStep{szIVal, szIVal} };//单索引形式
			}
		}

		//处理区间
		IndexStep stepIdx{};

		NBT_Type::String::View strNumLeft = strRange.substr(0, szDelimiterBegPos);
		if (!strNumLeft.empty())
		{
			stepIdx.szBeg = ParseNumber(pBase, strNumLeft);
		}

		NBT_Type::String::View strNumRight = strRange.substr(szDelimiterEndPos);
		if (!strNumRight.empty())
		{
			stepIdx.szEnd = ParseNumber(pBase, strNumRight);
		}

		if (stepIdx.szBeg > stepIdx.szEnd)//允许相等，退化为单索引形式
		{
			throw ParseError(std::format("Start index ({}) must not be greater than end index ({})", stepIdx.szBeg, stepIdx.szEnd), szBaseLSqBracketPos + 1 + szDelimiterBegPos);
		}

		if (strNumLeft.empty() && strNumRight.empty())
		{
			throw ParseError("Range must specify at least one bound (e.g., [N..], [..M], [N..M])", szBaseLSqBracketPos + 1 + szDelimiterBegPos);
		}

		return Step{ stepIdx };
	}

	static Step ParsePlainNameStep(const MUTF8_Char_Type *pBase, NBT_Type::String::View &strPath)
	{
		//进入时第一个字符为正常字符
		size_t szEndPos = strPath.find_first_of('/');//直接找到路径结束
		if (szEndPos == 0)
		{
			throw ParseError("Empty unquoted name not allowed", strPath.data() - pBase);
		}
		if (szEndPos == strPath.npos)
		{
			szEndPos = strPath.size();
		}

		NameStep stepName{ .strStep = strPath.substr(0, szEndPos) };
		if (stepName.strStep.size() == 1 && stepName.strStep[0] == '*')
		{
			stepName.SetWildcard();//通配符处理
		}
		else
		{
			size_t szErrCharPos = stepName.strStep.find_first_of(MU8STR("[]\"\\*"));//如果出现其它异常字符则出错
			if (szErrCharPos != stepName.strStep.npos)
			{
				throw ParseError(std::format("Invalid character '{}' in plain name", (char)stepName.strStep[szErrCharPos]), strPath.data() - pBase + szErrCharPos);
			}
		}
		

		//删除
		strPath.remove_prefix(szEndPos);

		return Step{ std::move(stepName) };
	}

	static Step ParseQuotedNameStep(const MUTF8_Char_Type *pBase, NBT_Type::String::View &strPath)
	{
		//进入时第一个字符为起始引号
		strPath.remove_prefix(1);//移除

		//空键名""
		if (!strPath.empty() && strPath.front() == '"')
		{
			strPath.remove_prefix(1);//移除结束的"
			return Step{ NameStep{} };
		}

		NameStep stepName{};
		while (!strPath.empty())
		{
			const auto it = strPath.front();
			strPath.remove_prefix(1);
			switch (it)
			{
			case '\\':
				if (strPath.empty())//转义符后是空
				{
					throw ParseError("Unexpected end after escape '\\'", strPath.data() - pBase);
				}

				if (strPath.front() == '\\' || strPath.front() == '\"')//下一字符
				{
					stepName.strStep.push_back(strPath.front());//丢弃转义，只保留下一字符
					strPath.remove_prefix(1);//移除
				}
				else//非法字符
				{
					throw ParseError(std::format("Invalid escape sequence: '\\{}'", (char)strPath.front()), strPath.data() - pBase - 1);
				}
				break;
			case '\"'://结束字符（引号结束）
				return Step{ std::move(stepName) };//右引号正常返回，如果不为空，下一字符必须是路径分隔符/，否则出错（此处判定在外侧存在，无需重复判定）
				break;
			default://其它字符，正常插入
				stepName.strStep.push_back(it);
				break;
			}
		}

		//没找到右引号就退出
		throw ParseError("Unterminated quoted name", strPath.data() - pBase);
	}

public:
	using PathInfo = std::vector<Step>;

	static PathInfo PathParser(NBT_Type::String::View strPath)
	{
		if (strPath.empty())
		{
			return {};
		}

		if (strPath.front() == '/')//至少有1元素，确定不以/开头
		{
			throw ParseError("Path cannot start with '/'", 0);
		}

		const MUTF8_Char_Type *pBase = strPath.data();

		PathInfo path;
		while (!strPath.empty())
		{
			Step step;
			switch (strPath.front())
			{
			case '[':
				step = ParseIndexStep(pBase, strPath);
				break;
			case '\"':
				step = ParseQuotedNameStep(pBase, strPath);
				break;
			default:
				step = ParsePlainNameStep(pBase, strPath);
				break;
			}
			path.push_back(step);

			// 步后必须是 '/' 或字符串结束
			if (!strPath.empty())
			{
				if (strPath.front() != '/')
				{
					throw ParseError(std::format("Expected '/' after step, but found '{}'", (char)strPath.front()), strPath.data() - pBase);
				}
				strPath.remove_prefix(1);//删除一个（跳过'/'）
				if (strPath.empty())// 禁止以 '/' 结尾（会导致下一个步为空名字）
				{
					throw ParseError("Path cannot end with '/'", strPath.data() - 1 - pBase);
				}
			}
		}

		return path;
	}

	static std::string PathToString(const PathInfo &stPathInfo)
	{
		std::string strRet;

		for (const auto &it : stPathInfo)
		{
			switch ((StepType)it.index())
			{
			case NbtPath::StepType::NONE:
				strRet += std::format("[[None Value]]");
				break;
			case NbtPath::StepType::Name:
				strRet += std::get<NameStep>(it).ToString();
				break;
			case NbtPath::StepType::Index:
				strRet += std::get<IndexStep>(it).ToString();
				break;
			default:
				strRet += std::format("[[Unknown Value]]");
				break;
			}
		}

		strRet.pop_back();//删除末尾/
		return strRet;
	}

};

template<typename Key_Type, typename Val_Type, typename Hash_Type, typename Equal_Type>
class TrieTree
{
public:
	struct TrieNode
	{
	public:
		using NodeChild_Type = std::unordered_map<Key_Type, TrieNode, Hash_Type, Equal_Type>;
	public:
		std::optional<Val_Type> nodeValue{};
		NodeChild_Type nodeChild{};
	public:
		template<typename T_Ky>
		TrieNode *FindNext(const T_Ky &key)
		{
			auto it = nodeChild.find(key);
			return it != nodeChild.end() ? &it->second : nullptr;
		}

		template<typename T_Ky>
		const TrieNode *FindNext(const T_Ky &key) const
		{
			auto it = nodeChild.find(key);
			return it != nodeChild.end() ? &it->second : nullptr;
		}
	};

	class WalkContext
	{
		friend class TrieTree<Key_Type, Val_Type, Hash_Type, Equal_Type>;
	private:
		const TrieNode *p{};

	public:
		WalkContext(const TrieNode *_p) :p(_p)
		{}

		WalkContext(const WalkContext &_Copy) :p(_Copy.p)
		{}

		WalkContext(WalkContext &&_Move) :p(_Move.p)
		{
			_Move.p = nullptr;
		}

		WalkContext &operator=(const TrieNode *_p)
		{
			p = _p;
			return *this;
		}

		WalkContext &operator=(const WalkContext &_Copy)
		{
			p = _Copy.p;
			return *this;
		}
		WalkContext &operator=(WalkContext &&_Move)
		{
			p = _Move.p;
			_Move.p = nullptr;
			return *this;
		}

		~WalkContext(void)
		{
			p = nullptr;
		}

		const TrieNode &GetCurNode(void) const
		{
			return *p;
		}

		operator bool() const
		{
			return p != nullptr;
		}

		const std::optional<Val_Type> &GetValue(void) const
		{
			return p->nodeValue;
		}

		void Next(const Key_Type &key)
		{
			p = p->FindNext(key);
		}

		template<typename T_Ky>
		bool TryNext(const T_Ky &key)
		{
			const auto *tmp = p->FindNext(key);
			if (tmp == nullptr)
			{
				return false;
			}

			p = tmp;
			return true;
		}

		bool HasNext(void) const
		{
			return !p->nodeChild.empty();
		}
	};

protected:
	TrieNode root;

public:
	template<typename KeyList_Type>
	requires(std::is_same_v<typename KeyList_Type::value_type, Key_Type>)
	void Insert(const KeyList_Type &path, Val_Type val)
	{
		if (path.empty())
		{
			return;
		}

		TrieNode *pCur = &root.nodeChild[path[0]];//利用方括号副作用，如果不存在那么插入默认值
		for (size_t i = 1, size = path.size(); i < size; ++i)
		{
			pCur = &pCur->nodeChild[path[i]];//利用方括号副作用，如果不存在那么插入默认值，同时遍历到指定节点
		}

		pCur->nodeValue = val;//插入值
	}


	WalkContext GetWalkContext() const
	{
		return WalkContext{ &root };
	}

	std::string ToString() const
	{
		auto key_to_str = [](const Key_Type &k) -> std::string
		{
			if constexpr (std::is_same_v<Key_Type, NbtPath::Step>)
			{
				switch (static_cast<NbtPath::StepType>(k.index()))
				{
				case NbtPath::StepType::Name:
					return std::get<NbtPath::NameStep>(k).ToString();
				case NbtPath::StepType::Index:
					return std::get<NbtPath::IndexStep>(k).ToString();
				default:
					return "?";
				}
			}
			else
			{
				return std::format("{}", k);
			}
		};

		auto val_to_str = [](const std::optional<Val_Type> &v) -> std::string
		{
			if (!v.has_value()) return "";
			if constexpr (std::is_enum_v<Val_Type>)
			{
				return std::format("{}", (std::underlying_type_t<Val_Type>)v.value());
			}
			else
			{
				return std::format("{}", v.value());
			}
		};

		std::function<void(std::string &, const std::string &, const std::string &, const TrieNode &)> dump_node;
		dump_node = [&]
		(
			std::string &out,
			const std::string &prefix,
			const std::string &indent,
			const TrieNode &node
		)
		{
			if (node.nodeValue.has_value())
			{
				out += std::format(" ({})", val_to_str(node.nodeValue));
			}
			out += "\n";

			if (node.nodeChild.empty()) return;

			size_t idx = 0;
			const size_t count = node.nodeChild.size();
			for (const auto &[child_key, child_node] : node.nodeChild)
			{
				bool is_last = (++idx == count);
				out += prefix;
				out += (is_last ? "└── " : "├── ");
				out += key_to_str(child_key);

				std::string child_prefix = prefix + (is_last ? "    " : "│   ");
				dump_node(out, child_prefix, indent, child_node);
			}
		};

		std::string result = "(root)\n";
		size_t idx = 0;
		const size_t count = root.nodeChild.size();
		for (const auto &[key, node] : root.nodeChild)
		{
			bool is_last = (++idx == count);
			result += (is_last ? "└── " : "├── ");
			result += key_to_str(key);
			std::string child_prefix = (is_last ? "    " : "│   ");
			dump_node(result, child_prefix, "", node);
		}
		return result;
	}
};

class NBTErase
{
public:
	struct Request
	{
	public:
		enum class EraseMode : uint8_t
		{
			CLEAR,
			REMOVE,
			UNKNOWN,
		};

	public:
		NbtPath::PathInfo stNbtPath{};
		EraseMode enMode = EraseMode::UNKNOWN;

	public:
		std::string ToString(void) const
		{
			std::string strRet = "EraseMode: [";
			switch (enMode)
			{
			case EraseMode::REMOVE:
				strRet += "REMOVE] ";
				break;
			case EraseMode::CLEAR:
				strRet += "CLEAR] ";
				break;
			case EraseMode::UNKNOWN:
			default:
				strRet += "UNKNOWN] ";
				break;
			}

			strRet += std::format("Path: {}", NbtPath::PathToString(stNbtPath));
			return strRet;
		}
	};

	using RequestList = std::vector<Request>;
	using NbtPathTrieTree = TrieTree<NbtPath::Step, Request::EraseMode, NbtPath::StepHash, NbtPath::StepEqual>;

public:
	class EraseError : public std::runtime_error
	{
	public:
		EraseError(const char *message) :
			std::runtime_error(message)
		{}

		EraseError(const std::string &message) :
			std::runtime_error(message)
		{}
	};

public:
	static void CompoundErase(NBT_Type::Compound &cpd, NbtPathTrieTree::WalkContext ctx)
	{
		auto &rawCpd = cpd.GetData();

		static const NBT_String strWildcard = NbtPath::NameStep::MakeWildcardString();

		//只要包含通配符，那么所有其它同级的都无意义，直接遍历全部并处理
		if (auto wc = ctx.GetCurNode().nodeChild.find(strWildcard); wc != ctx.GetCurNode().nodeChild.end())
		{
			//区分两种情况，如果没有值，那么遍历当前级别并深入，否则直接清空当前级
			auto ctxNext = NbtPathTrieTree::WalkContext{ &wc->second };//切换上下文
			auto &val = ctxNext.GetValue();
			if (val.has_value())
			{
				switch (val.value())
				{
				case Request::EraseMode::CLEAR:
					for (auto &[k, v] : rawCpd)
					{
						std::visit(
							[&](auto &v) -> void
							{
								v = {};//重置为默认值
							}, v.GetData());//重置值
					}
					break;
				case Request::EraseMode::REMOVE:
					rawCpd.clear();//remove所有相当于清空当前容器
					break;
				default:
					break;
				}
			}
			else
			{
				for (auto &[k, v] : rawCpd)
				{
					EraseSwitch(v, ctxNext);
				}
			}

			return;//完成返回
		}

		for (const auto &[k, v] : ctx.GetCurNode().nodeChild)
		{
			auto p = std::get_if<NbtPath::NameStep>(&k);
			if (p == nullptr)
			{
				//抛出异常，对compound进行index步
				throw EraseError("");
			}

			//在目标中查找
			auto it = rawCpd.find(p->strStep);
			if (it == rawCpd.end())
			{
				continue;
			}

			//找到了，检查是否有值，有值则操作，否则递归步入
			auto ctxNext = NbtPathTrieTree::WalkContext{ &v };//切换上下文
			auto &val = ctxNext.GetValue();
			if (val.has_value())
			{
				switch (val.value())
				{
				case Request::EraseMode::CLEAR:
					std::visit(
						[&](auto &v) -> void
						{
							v = {};//重置为默认值
						}, it->second.GetData());//重置值
					break;
				case Request::EraseMode::REMOVE:
					rawCpd.erase(it);
					break;
				default:
					break;
				}
			}
			else
			{
				EraseSwitch(it->second, ctxNext);
			}
		}

		return;
	}

	//改为模板，使用原始vector，array与list都在此处理
	template<typename List_Raw>
	static void GeneralListErase(List_Raw &rawList, const NbtPathTrieTree::WalkContext ctx)
	{
		//对tree进行完全展开，获取所有要处理的index，排序，并依次遍历处理
		//注意node不可能有值，因为有值的情况下已经在递归的父调用中处理了，当前是子调用
		//首先进行排序
		std::vector<NbtPath::IndexStep> listIdx;
		for (const auto &[k, v] : ctx.GetCurNode().nodeChild)
		{
			auto p = std::get_if<NbtPath::IndexStep>(&k);
			if (p == nullptr)
			{
				//抛出异常，非法访问，list后不是index步
				throw EraseError("");
			}

			listIdx.push_back(*p);
		}

		//验证完成后才进行空判断，以保证错误抛出
		if (rawList.empty())
		{
			return;
		}

		//倒排，从后往前处理的开销小于从前往后，并且按顺序从后往前的情况下，删除最后的索引后，前面的索引不变，同时不再访问后续
		std::ranges::sort(listIdx,
			[](const NbtPath::IndexStep &l, const NbtPath::IndexStep &r) -> bool//返回true表示l排在r前
			{
				//beg与end相等的单一索引排在更前
				if (l.szBeg == r.szBeg)//beg相等，排序end
				{
					//end小的在前面，小于等于使得l更优先
					return l.szEnd >= r.szEnd;
				}

				return l.szBeg > r.szBeg;
			}
		);

		//遍历列表处理，注意前面的空判断保证运行到此，列表至少有1元素，不为空
		size_t szListSize = rawList.size();
		for (const auto &v : listIdx)
		{
			//特判指定索引的情况，如果指定的索引超出范围，则跳过，其它范围情况溢出则锁定到上下界
			if (v.szBeg == v.szEnd && v.szBeg >= szListSize)
			{
				continue;
			}

			auto ctxNext = ctx;//拷贝上下文
			ctxNext.Next(v);//因为key是从map本身获取的，必然成功，不做判断
			MyAssert(ctxNext, "ctxNext is nullptr WTF?");//拉点assert防止NPE

			//处理上下界
			size_t l = v.szBeg < szListSize ? v.szBeg : szListSize - 1;
			size_t r = v.szEnd < szListSize ? v.szEnd : szListSize - 1;
			++r;//上下界都可取到，让上界变为尾后位置

			auto &val = ctxNext.GetValue();
			if (val.has_value())
			{
				switch (val.value())
				{
				case Request::EraseMode::CLEAR:
					for (size_t i = l; i < r; ++i)
					{
						rawList[i] = {};//设置为空
					}
					break;
				case Request::EraseMode::REMOVE:
					//范围删除
					rawList.erase(rawList.begin() + l, rawList.begin() + r);
					break;
				default:
					break;
				}
			}
			else//递归处理
			{
				if constexpr (std::is_same_v<std::remove_cvref_t<decltype(rawList[0])>, NBT_Node>)//值必须是NBT_Node的才是列表，否则是数组
				{
					for (size_t i = l; i < r; ++i)//没有值，那么进入递归处理子序列
					{
						EraseSwitch(rawList[i], ctxNext);
					}
				}
				else//非列表，无法递归处理，抛出异常
				{
					throw EraseError("");
				}
			}
		}

		return;
	}

	static void ListErase(NBT_Type::List &list, const NbtPathTrieTree::WalkContext ctx)
	{
		GeneralListErase(list.GetData(), ctx);
	}

	template<typename Array_Type>
	requires(NBT_Type::IsArrayType_V<Array_Type>)
	static void ArrayErase(Array_Type &tArray, const NbtPathTrieTree::WalkContext ctx)
	{
		GeneralListErase<Array_Type>(tArray, ctx);
	}

	static void EraseSwitch(NBT_Node &node, NbtPathTrieTree::WalkContext ctx)
	{
		switch (node.GetTag())
		{
		case NBT_TAG::Compound:
			CompoundErase(node.GetCompound(), ctx);
			break;
		case NBT_TAG::List:
			ListErase(node.GetList(), ctx);
			break;
			//处理数组
		case NBT_TAG::ByteArray:
			ArrayErase(node.GetByteArray(), ctx);
			break;
		case NBT_TAG::IntArray:
			ArrayErase(node.GetIntArray(), ctx);
			break;
		case NBT_TAG::LongArray:
			ArrayErase(node.GetLongArray(), ctx);
			break;
		default://路径不可达，忽略
			break;
		}
	}

public:
	static NbtPathTrieTree EraseRequest2NbtPathTrieTree(const RequestList &listEraseReq)
	{
		//构造前缀树
		NbtPathTrieTree tt;
		for (const auto &[k, v] : listEraseReq)
		{
			tt.Insert(k, v);
		}
		return tt;
	}

	static void NbtParseToErase(NBT_Type::Compound &cpd, const NbtPathTrieTree& tt)
	{
		CompoundErase(cpd, tt.GetWalkContext());
	}

	//static void NbtDirectlyErase(NBT_Type::Compound &cpd, const RequestList &listEraseReq)
	//{
	//	for (const auto &v : listEraseReq)
	//	{
	//		for (const auto &pathSeg : v.stNbtPath)
	//		{
	//			switch ((NbtPath::StepType)pathSeg.index())
	//			{
	//			case NbtPath::StepType::Name:
	//
	//				break;
	//			case NbtPath::StepType::Index:
	//
	//
	//				break;
	//			default:
	//				break;
	//			}
	//		}
	//	}
	//}
};

