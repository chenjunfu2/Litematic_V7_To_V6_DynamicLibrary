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

	static PathInfo FullPathToPathInfo(const NBT_Type::String &strFullPath)
	{




	}

public:
	PathInfo vPath;
	EraseMode enMode;
};


using RequestList = std::vector<EraseRequest>;











