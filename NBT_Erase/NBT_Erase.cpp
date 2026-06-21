#include <nbt_cpp/NBT_All.hpp>

#include <charconv>
#include <format>

struct EraseRequest
{
public:
	enum EraseMode
	{
		REMOVE,
		CLEAR,
	};

	struct NameStep
	{
		NBT_Type::String strStep;
	};

	struct IndexStep//默认值为全覆盖
	{
		size_t szBeg = 0;
		size_t szEnd = SIZE_MAX;
	};

	using Step = std::variant<NameStep, IndexStep>;

	using PathInfo = std::vector<Step>;

	class ParseError : public std::runtime_error
	{
	public:
		ParseError(const char *message, size_t position)
			: std::runtime_error(std::format("{} at position {}", message, position))
		{}
	};

	static Step ParseIndexStep(const MUTF8_Char_Type *pBase, NBT_Type::String::View &strPath)
	{
		//进入此处，当前字符[0]为'['
		size_t szRSqBracketPos = strPath.find_first_of(']', 1);//从[的下一个字符开始查找
		if (szRSqBracketPos == strPath.npos)
		{
			throw ParseError("Missing ']'", strPath.data() - pBase);
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
				throw ParseError("Expected '..' but only found '.' and nothing after", strPath.data() - pBase);
			}
			if (strRange[szNextDelimiterPos] != '.')
			{
				throw ParseError("Expected '..' for range, found single '.'", strPath.data() - pBase);
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
				size_t szIVal;
				auto result = std::from_chars((const char *)strRange.data(), (const char *)strRange.data() + strRange.size(), szIVal);
				if (result.ec != std::errc{} || result.ptr != (const char *)strRange.data() + strRange.size())
				{
					throw ParseError("Invalid index value", strPath.data() - pBase);
				}

				return Step{ IndexStep{szIVal, szIVal} };//单索引形式
			}
		}

		//处理区间
		IndexStep stepIdx{};

		NBT_Type::String::View strNumLeft = strRange.substr(0, szDelimiterBegPos);
		if (!strNumLeft.empty())
		{
			auto result = std::from_chars((const char *)strNumLeft.data(), (const char *)strNumLeft.data() + strNumLeft.size(), stepIdx.szBeg);
			if (result.ec != std::errc{} || result.ptr != (const char *)strNumLeft.data() + strNumLeft.size())
			{
				throw ParseError("Invalid start index in range", strPath.data() - pBase);
			}
		}

		NBT_Type::String::View strNumRight = strRange.substr(szDelimiterEndPos);
		if (!strNumRight.empty())
		{
			auto result = std::from_chars((const char *)strNumRight.data(), (const char *)strNumRight.data() + strNumRight.size(), stepIdx.szEnd);
			if (result.ec != std::errc{} || result.ptr != (const char *)strNumRight.data() + strNumRight.size())
			{
				throw ParseError("Invalid end index in range", strPath.data() - pBase);
			}
		}

		if (stepIdx.szBeg > stepIdx.szEnd)//允许相等，退化为单索引形式
		{
			throw ParseError("Start index must not be greater than end index", strPath.data() - pBase);
		}

		if (strNumLeft.empty() && strNumRight.empty())
		{
			throw ParseError("Range must specify at least one bound (e.g., [N..], [..M], [N..M])", strPath.data() - pBase);
		}

		return Step{ stepIdx };
	}

	static Step ParsePlainNameStep(const MUTF8_Char_Type *pBase, NBT_Type::String::View &strPath)
	{
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
			throw ParseError("Invalid character in plain name", strPath.data() - pBase + szErrCharPos);
		}

		//删除
		strPath.remove_prefix(szEndPos);

		return Step{ std::move(stepName) };
	}

	static Step ParseQuotedNameStep(const MUTF8_Char_Type *pBase, NBT_Type::String::View &strPath)
	{

	}
	

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
					throw ParseError("Expected '/' after step", strPath.data() - pBase);
				}
				strPath.remove_prefix(1);//删除一个（跳过'/'）
				if (strPath.empty())// 禁止以 '/' 结尾（会导致下一个步为空名字）
				{
					throw ParseError("Path cannot end with '/'", strPath.data() - pBase);
				}
			}
		}

		return path;
	}

public:
	PathInfo vPath;
	EraseMode enMode;
};


using RequestList = std::vector<EraseRequest>;











