#pragma once

#include <nbt_cpp/NBT_All.hpp>

#include <charconv>
#include <format>
#include <functional>

#define MU8CV2CTU8(mu8String) MUTF8_Tool<MUTF8_Char_Type, char16_t, char>::MU8ToU8(mu8String)

struct NbtPath
{
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

	struct NameStep
	{
		NBT_Type::String strStep = {};//默认空字符串

	public:
		size_t hash(void) const
		{
			return std::hash<NBT_Type::String>{}(strStep);
		}

		bool operator==(const NameStep &) const = default;

		std::string ToString(void) const
		{
			if (!strStep.empty() && strStep.find_first_of(MU8STR("/[]\"\\")) == strStep.npos)//普通字符且非空键名
			{
				return std::format("{}/", strStep.ToCharTypeUTF8());
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
	using PathInfo = std::vector<Step>;

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
		size_t szErrCharPos = stepName.strStep.find_first_of(MU8STR("[]\"\\"));//如果出现其它异常字符则出错
		if (szErrCharPos != stepName.strStep.npos)
		{
			throw ParseError(std::format("Invalid character '{}' in plain name", (char)stepName.strStep[szErrCharPos]), strPath.data() - pBase + szErrCharPos);
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
public:
	NbtPath(void) = delete;
	~NbtPath(void) = delete;

	static std::string ToString(const PathInfo &stPathInfo)
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

namespace std
{
	template<>
	struct hash<NbtPath::NameStep>
	{
		size_t operator()(const NbtPath::NameStep &v) const noexcept
		{
			return v.hash();
		}
	};

	template<>
	struct hash<NbtPath::IndexStep>
	{
		size_t operator()(const NbtPath::IndexStep &v) const noexcept
		{
			return v.hash();
		}
	};
};


struct EraseRequest
{
public:
	enum class EraseMode : uint8_t
	{
		REMOVE,
		CLEAR,
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
		case EraseRequest::EraseMode::REMOVE:
			strRet += "REMOVE] ";
			break;
		case EraseRequest::EraseMode::CLEAR:
			strRet += "CLEAR] ";
			break;
		case EraseRequest::EraseMode::UNKNOWN:
		default:
			strRet += "UNKNOWN] ";
			break;
		}

		strRet += std::format("Path: {}", NbtPath::ToString(stNbtPath));
		return strRet;
	}
};

using EraseRequestList = std::vector<EraseRequest>;


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

	protected:
		WalkContext(const TrieNode *_p) :p(_p)
		{}
	public:
		WalkContext(const WalkContext &_Copy) :p(_Copy.p)
		{}

		WalkContext(WalkContext &&_Move) :p(_Move.p)
		{
			_Move.p = nullptr;
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
	TrieNode::NodeChild_Type root;

public:
	template<typename KeyList_Type>
	requires(std::is_same_v<typename KeyList_Type::value_type, Key_Type>)
	void Insert(const KeyList_Type &path, Val_Type val)
	{
		if (path.empty())
		{
			return;
		}

		TrieNode *pCur = &root[path[0]];//利用方括号副作用，如果不存在那么插入默认值
		for (size_t i = 1, size = path.size(); i < size; ++i)
		{
			pCur = &pCur->nodeChild[path[i]];//利用方括号副作用，如果不存在那么插入默认值，同时遍历到指定节点
		}

		pCur->nodeValue = val;//插入值
	}


	WalkContext GetWalkContext(const Key_Type &key) const
	{
		auto it = root.find(key);
		return WalkContext{ (it != root.end() ? &it->second : nullptr) };
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
			if constexpr (std::is_same_v<Val_Type, EraseRequest::EraseMode>)
			{
				switch (v.value())
				{
				case EraseRequest::EraseMode::REMOVE:  return "REMOVE";
				case EraseRequest::EraseMode::CLEAR:   return "CLEAR";
				case EraseRequest::EraseMode::UNKNOWN: return "UNKNOWN";
				default: return "?";
				}
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
		const size_t count = root.size();
		for (const auto &[key, node] : root)
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


using NbtPathTrieTree = TrieTree<NbtPath::Step, EraseRequest::EraseMode, NbtPath::StepHash, NbtPath::StepEqual>;


void NbtParseToErase(NBT_Type::Compound &cpd, const EraseRequestList &listEraseReq)
{
	//构造前缀树
	NbtPathTrieTree tt;
	for (const auto &[k, v] : listEraseReq)
	{
		tt.Insert(k, v);
	}
	//printf("%s\n", tt.ToString().c_str());










	return;
}


void CompoundErase(NBT_Type::Compound &cpd, NbtPathTrieTree::WalkContext ctx);
void ListErase(NBT_Type::List &list, const NbtPathTrieTree::WalkContext ctx);

void EraseSwitch(NBT_Node &node, NbtPathTrieTree::WalkContext ctx)
{
	switch (node.GetTag())
	{
	case NBT_TAG::Compound:
		CompoundErase(node.GetCompound(), ctx);
		break;
	case NBT_TAG::List:
		ListErase(node.GetList(), ctx);
		break;
	default://路径不可达，忽略
		break;
	}
}


void CompoundErase(NBT_Type::Compound &cpd, NbtPathTrieTree::WalkContext ctx)
{
	std::vector<NBT_Type::Compound::Iterator> remove;

	for (auto it = cpd.begin(), end = cpd.end(); it != end; ++it)
	{
		auto &k = it->first;
		auto &v = it->second;

		auto ctxNext = ctx;//拷贝上下文
		if (!ctxNext.TryNext(k))//尝试移动
		{
			continue;//失败跳过
		}
		//成功，检查是否有值，有值则操作，否则递归步入
		auto &val = ctxNext.GetValue();
		if (val.has_value())
		{
			switch (val.value())
			{
			case EraseRequest::EraseMode::CLEAR:
				std::visit(
				[&](auto &v) -> void
				{
					v = {};//重置为默认值
				}, v.GetData());
				break;
			case EraseRequest::EraseMode::REMOVE:
				remove.push_back(it);
				break;
			default:
				break;
			}
		}
		else
		{
			EraseSwitch(v, ctxNext);
		}
	}

	auto &rawCpd = cpd.GetData();
	for (auto &it : remove)
	{
		rawCpd.erase(it);
	}

	return;
}

void ListErase(NBT_Type::List &list, const NbtPathTrieTree::WalkContext ctx)
{
	std::vector<NBT_Type::List::Iterator> remove;

	//对tree下一级进行完全展开，获取所有要处理的index，排序，并依次遍历处理




	return;
}


