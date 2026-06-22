#pragma once

#include <nbt_cpp/NBT_All.hpp>

#include <charconv>
#include <format>

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
	};

	struct IndexStep//默认值为全覆盖
	{
		size_t szBeg = 0;
		size_t szEnd = SIZE_MAX;
	};

	enum class StepType : size_t
	{
		NONE = -1,
		Name = 0,
		Index = 1,
	};

	using Step = std::variant<NameStep, IndexStep>;
	using PathInfo = std::vector<Step>;

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
	PathInfo stPathInfo;

public:
	NbtPath(const NBT_Type::String &strPath) : stPathInfo(PathParser(NBT_Type::String::View(strPath)))
	{}
	NbtPath(const NBT_Type::String::View &strvPath) : stPathInfo(PathParser(strvPath))
	{}
	NbtPath(void) = default;
	~NbtPath(void) = default;

	std::string Print(void) const
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
				{
					const auto &tmp = std::get<NameStep>(it).strStep;
					if (!tmp.empty() && tmp.find_first_of(MU8STR("/[]\"\\")) == tmp.npos)//普通字符且非空键名
					{
						strRet += std::format("{}/", tmp.ToCharTypeUTF8());
						break;
					}

					//需要引号并添加转义
					NBT_Type::String newStr{};
					for (const auto &ch : tmp)
					{
						if (ch == '"' || ch == '\\')
						{
							newStr.push_back('\\');
						}
						newStr.push_back(ch);
					}

					strRet += std::format("\"{}\"/", newStr.ToCharTypeUTF8());
				}
				break;
			case NbtPath::StepType::Index:
				{
					const auto &tmp = std::get<IndexStep>(it);

					if (tmp.szBeg == tmp.szEnd)
					{
						strRet += std::format("[{}]/", tmp.szBeg);
					}
					else
					{
						if (tmp.szBeg == 0 && tmp.szEnd == SIZE_MAX)
						{
							strRet += "[]/";
						}
						else
						{
							std::string strBeg = tmp.szBeg == 0 ? "" : std::format("{}", tmp.szBeg);
							std::string strEnd = tmp.szEnd == SIZE_MAX ? "" : std::format("{}", tmp.szEnd);
							strRet += std::format("[{}..{}]/", strBeg, strEnd);
						}
					}
				}
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
	NbtPath stNbtPath{};
	EraseMode enMode = EraseMode::UNKNOWN;

public:
	std::string Print(void) const
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

		strRet += std::format("Path: {}", stNbtPath.Print());
		return strRet;
	}
};

using RequestList = std::vector<EraseRequest>;

