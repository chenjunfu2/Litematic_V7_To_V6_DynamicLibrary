#include <nbt_cpp/NBT_All.hpp>




struct EraseRequest
{
public:
	enum EraseMode
	{
		REMOVE,
		CLEAR,
	};

	struct SegPathInfo
	{
		bool bIsList;//列表特殊处理（下标处理）
		NBT_Type::String strSegmentPath;
	};

	using PathInfo = std::vector<SegPathInfo>;

	static PathInfo FullPathToPathInfo(const NBT_Type::String &strFullPath)
	{




	}

public:
	PathInfo vPath;
	EraseMode enMode;
};


using RequestList = std::vector<EraseRequest>;











