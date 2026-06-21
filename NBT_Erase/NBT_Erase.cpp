#include <nbt_cpp/NBT_All.hpp>




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

	struct IndexStep
	{
		size_t szBeg = 0;
		size_t szEnd = SIZE_MAX;
	};

	using Step = std::variant<NameStep, IndexStep>;

	using PathInfo = std::vector<Step>;

	class ParseError : public std::runtime_error
	{
	public:
		ParseError(const std::string &message, size_t position)
			: std::runtime_error(message + " at position " + std::to_string(position))
		{}
	};


	static PathInfo PathParser(const NBT_Type::String &strFullPath)
	{
		if (strFullPath.empty())
		{
			return {};
		}

		const auto *pPathStr = strFullPath.data();
		size_t szSize = strFullPath.size();
		size_t szPos = 0;

		if (pPathStr[szPos] == '/')
		{
			throw ParseError("Path cannot start with '/'", szPos);
		}

		PathInfo path;
		bool bIsLastNameStep = false;

		while (szPos < szSize)
		{
			Step step;
			switch (pPathStr[szPos])
			{
			case '[':
				if (!bIsLastNameStep)
				{
					throw ParseError("", szPos);
				}

				ParseIndexStep(pPathStr, szSize, szPos);
				bIsLastNameStep = false;
				break;
			case '\"':
				step = ParseQuotedNameStep(pPathStr, szSize, szPos);
				bIsLastNameStep = true;
				break;
			default:
				step = ParsePlainNameStep(pPathStr, szSize, szPos);
				bIsLastNameStep = true;
				break;
			}
			path.push_back(step);

			// 步后必须是 '/' 或字符串结束
			if (szPos < szSize)
			{
				if (pPathStr[szPos] != '/')
				{
					throw ParseError("Expected '/' after step", szPos);
				}
				++szPos; // 跳过 '/'

				if (szPos == szSize)// 禁止以 '/' 结尾（会导致下一个步为空名字）
				{
					throw ParseError("Path cannot end with '/'", szPos - 1);
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











