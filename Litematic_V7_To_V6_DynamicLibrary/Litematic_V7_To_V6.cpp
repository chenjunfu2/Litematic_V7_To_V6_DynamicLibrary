#include "Process_Litematic_Data.hpp"

#include "util/CodeTimer.hpp"

#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <stdexcept>

#define V6_MINECRAFT_DATA_VERSION 3700
#define V6_LITEMATIC_VERSION 6
#define V6_LITEMATIC_SUBVERSION 1

//找到一个唯一文件名
std::string GenerateUniqueFilename(const std::string &sBeg, const std::string &sEnd, uint32_t u32TryCount = 10)//默认最多重试10次
{
	while (u32TryCount != 0)
	{
		//时间用[]包围
		auto tmpPath = std::format("{}[{}]{}", sBeg, CodeTimer::GetSystemTime(), sEnd);//获取当前系统时间戳作为中间的部分
		if (!NBT_IO::IsFileExist(tmpPath))
		{
			return tmpPath;
		}

		//等几ms在继续
		CodeTimer::Sleep(std::chrono::milliseconds(10));
		--u32TryCount;
	}

	//次数到上限直接返回空
	return std::string{};
}


void ProcessEntity(NBT_Type::Compound &cpdV7EntityData, NBT_Type::Compound &cpdV6EntityData, const NBT_Type::Int iV7McDataVersion)
{
	using std::placeholders::_1;
	using std::placeholders::_2;
	using std::placeholders::_3;
	using std::placeholders::_4;

	const static std::unordered_map<NBT_Type::String, MapValFunc_T> mapProccess =
	{
		{ MU8STR("has_egg"),			std::bind(RenameProcess,			MU8STR("HasEgg"),			_1, _2, _3, _4) },
		{ MU8STR("life_ticks"),			std::bind(RenameProcess,			MU8STR("LifeTicks"),		_1, _2, _3, _4) },
		{ MU8STR("size"),				std::bind(RenameProcess,			MU8STR("Size"),				_1, _2, _3, _4) },
		{ MU8STR("fall_distance"),		std::bind(RenameProcess,			MU8STR("FallDistance"),		_1, _2, _3, _4) },

		{ MU8STR("anchor_pos"),			std::bind(ProcessBlockPosExternal,	MU8STR("A"),				_1, _2, _3, _4) },
		{ MU8STR("block_pos"),			std::bind(ProcessBlockPosExternal,	MU8STR("Tile"),				_1, _2, _3, _4) },
		{ MU8STR("bound_pos"),			std::bind(ProcessBlockPosExternal,	MU8STR("Bound"),			_1, _2, _3, _4) },
		{ MU8STR("home_pos"),			std::bind(ProcessBlockPosExternal,	MU8STR("HomePos"),			_1, _2, _3, _4) },
		{ MU8STR("sleeping_pos"),		std::bind(ProcessBlockPosExternal,	MU8STR("Sleeping"),			_1, _2, _3, _4) },

		{ MU8STR("attributes"),			std::bind(DefaultProcess,			MU8STR("Attributes"),	ProcessAttributes,	_1, _2, _3, _4) },
		{ MU8STR("flower_pos"),			std::bind(DefaultProcess,			MU8STR("FlowerPos"),	ProcessBlockPos,	_1, _2, _3, _4) },
		{ MU8STR("hive_pos"),			std::bind(DefaultProcess,			MU8STR("HivePos"),		ProcessBlockPos,	_1, _2, _3, _4) },
		{ MU8STR("Item"),				std::bind(DefaultProcess,			MU8STR("Item"),			ProcessSingleItem,	_1, _2, _3, _4) },
		{ MU8STR("Items"),				std::bind(DefaultProcess,			MU8STR("Items"),		ProcessItems,		_1, _2, _3, _4) },

		{ MU8STR("ArmorItems"),			std::bind(DefaultProcess,			MU8STR("ArmorItems"),	(TagProcessFunc_T)std::bind(ProcessEntityItems, _1, _2, _3, 4),		_1, _2, _3, _4) },
		{ MU8STR("HandItems"),			std::bind(DefaultProcess,			MU8STR("HandItems"),	(TagProcessFunc_T)std::bind(ProcessEntityItems, _1, _2, _3, 2),		_1, _2, _3, _4) },
		{ MU8STR("Inventory"),			std::bind(DefaultProcess,			MU8STR("Inventory"),	(TagProcessFunc_T)std::bind(ProcessEntityItems, _1, _2, _3, 1),		_1, _2, _3, _4) },

		{ MU8STR("equipment"),			ProcessEntityEquipment },
		{ MU8STR("drop_chances"),		ProcessEntityDropChances },
	};


	for (auto &[itV7TagKey, itV7TagVal] : cpdV7EntityData)
	{
		//查找是否有匹配的处理过程
		auto itFind = mapProccess.find(itV7TagKey);
		if (itFind == mapProccess.end())
		{
			//不匹配直接移动处理
			cpdV6EntityData.Put(itV7TagKey, std::move(itV7TagVal));
			continue;
		}

		//进行处理
		auto &funcProcess = itFind->second;
		funcProcess(itV7TagKey, itV7TagVal, cpdV6EntityData, iV7McDataVersion);
	}

	return;
}

void ProcessTileEntity(NBT_Type::Compound &cpdV7TileEntityData, NBT_Type::Compound &cpdV6TileEntityData, const NBT_Type::Int iV7McDataVersion)
{
	FixTileEntityId(cpdV7TileEntityData, iV7McDataVersion);

	//特殊处理
	const static auto funcJukeboxProcess =
	[](const NBT_Type::String &strV7TagKey, NBT_Node &nodeV7TagVal, NBT_Type::Compound &cpdV6TileEntityData, const NBT_Type::Int iV7McDataVersion) -> void
	{
		cpdV6TileEntityData.PutLong(MU8STR("RecordStartTick"), 0);
		cpdV6TileEntityData.PutLong(MU8STR("TickCount"), nodeV7TagVal.IsLong() ? nodeV7TagVal.GetLong() : 0);
		cpdV6TileEntityData.PutByte(MU8STR("IsPlaying"), 0);
	};

	using std::placeholders::_1;
	using std::placeholders::_2;
	using std::placeholders::_3;
	using std::placeholders::_4;

	const static std::unordered_map<NBT_Type::String, MapValFunc_T> mapProccess =
	{
		{ MU8STR("custom_name"),				std::bind(RenameProcess,	MU8STR("CustomName"),	_1, _2, _3, _4) },//重命名处理

		{ MU8STR("ticks_since_song_started"),	funcJukeboxProcess }, //音符盒特殊处理

		{ MU8STR("Items"),						std::bind(DefaultProcess,	MU8STR("Items"),		ProcessItems,			_1, _2, _3, _4) },
		{ MU8STR("patterns"),					std::bind(DefaultProcess,	MU8STR("Patterns"),		ProcessPatterns,		_1, _2, _3, _4) },
		{ MU8STR("profile"),					std::bind(DefaultProcess,	MU8STR("SkullOwner"),	ProcessSkullProfile,	_1, _2, _3, _4) },
		{ MU8STR("flower_pos"),					std::bind(DefaultProcess,	MU8STR("FlowerPos"),	ProcessBlockPos,		_1, _2, _3, _4) },
		{ MU8STR("exit_portal"),				std::bind(DefaultProcess,	MU8STR("ExitPortal"),	ProcessBlockPos,		_1, _2, _3, _4) },
		{ MU8STR("bees"),						std::bind(DefaultProcess,	MU8STR("Bees"),			ProcessBees,			_1, _2, _3, _4) },
		{ MU8STR("item"),						std::bind(DefaultProcess,	MU8STR("item"),			ProcessSingleItem,		_1, _2, _3, _4) },
		{ MU8STR("RecordItem"),					std::bind(DefaultProcess,	MU8STR("RecordItem"),	ProcessSingleItem,		_1, _2, _3, _4) },
		{ MU8STR("Book"),						std::bind(DefaultProcess,	MU8STR("Book"),			ProcessSingleItem,		_1, _2, _3, _4) },

		{ MU8STR("front_text"),					std::bind(DefaultProcess,	MU8STR("front_text"),	ProcessSignText,		_1, _2, _3, _4) },
		{ MU8STR("back_text"),					std::bind(DefaultProcess,	MU8STR("back_text"),	ProcessSignText,		_1, _2, _3, _4) },
	};

	for (auto &[itV7TagKey, itV7TagVal] : cpdV7TileEntityData)
	{
		//查找是否有匹配的处理过程
		auto itFind = mapProccess.find(itV7TagKey);
		if (itFind == mapProccess.end())
		{
			//不匹配直接移动处理
			cpdV6TileEntityData.Put(itV7TagKey, std::move(itV7TagVal));
			continue;
		}

		//进行处理
		auto &funcProcess = itFind->second;
		funcProcess(itV7TagKey, itV7TagVal, cpdV6TileEntityData, iV7McDataVersion);
	}

	return;
}

//V7到V6仅转换Entity与TileEntity，其余不变
void ProcessRegion(NBT_Type::Compound &cpdV7RegionData, NBT_Type::Compound &cpdV6RegionData, const NBT_Type::Int iV7McDataVersion)
{
	//先转移不变数据，然后处理实体与方块实体

	//简易处理函数
	auto TransferDirectOptionalField = [&](const NBT_Type::String &strKey, NBT_TAG tag) -> bool
	{
		auto *pField = cpdV7RegionData.Has(strKey);
		if (pField != NULL &&
			(tag == NBT_TAG::ENUM_END || pField->GetTag() == tag))//ENUM_END表示接受任意类型，否则强匹配指定类型
		{
			cpdV6RegionData.Put(strKey, std::move(*pField));
			return true;
		}

		return false;
	};

	//这两个必须有，否则失败
	if (!TransferDirectOptionalField(MU8STR("Position"), NBT_TAG::Compound))
	{
		throw std::runtime_error("Position not found!");
	}

	if (!TransferDirectOptionalField(MU8STR("Size"), NBT_TAG::Compound))
	{
		throw std::runtime_error("Size not found!");
	}

	//下面的可能没有，没有则跳过插入
	(void)TransferDirectOptionalField(MU8STR("PendingBlockTicks"), NBT_TAG::List);
	(void)TransferDirectOptionalField(MU8STR("PendingFluidTicks"), NBT_TAG::List);
	(void)TransferDirectOptionalField(MU8STR("BlockStatePalette"), NBT_TAG::List);
	(void)TransferDirectOptionalField(MU8STR("BlockStates"), NBT_TAG::LongArray);

	//如果没有则跳过转换处理

	//实体处理
	do
	{
		auto *pEntities = cpdV7RegionData.HasList(MU8STR("Entities"));
		if (pEntities == NULL)
		{
			break;
		}

		auto &listV6EntityList = cpdV6RegionData.PutList(MU8STR("Entities"), {}).first->second.GetList();
		for (auto &nodeEntity : *pEntities)
		{
			auto &cpdNode = listV6EntityList.AddBackCompound({}).GetCompound();
			ProcessEntity(GetCompound(nodeEntity), cpdNode, iV7McDataVersion);
		}
	} while (false);
	
	//方块实体处理
	do
	{
		auto *pTileEntities = cpdV7RegionData.HasList(MU8STR("TileEntities"));
		if (pTileEntities == NULL)
		{
			break;
		}

		auto &listV6TileEntityList = cpdV6RegionData.PutList(MU8STR("TileEntities"), {}).first->second.GetList();
		for (auto &nodeTileEntity : *pTileEntities)
		{
			auto &cpdNode = listV6TileEntityList.AddBackCompound({}).GetCompound();
			ProcessTileEntity(GetCompound(nodeTileEntity), cpdNode, iV7McDataVersion);
		}
	} while (false);
}


void ConvertLitematicData_V7_To_V6(NBT_Type::Compound &cpdV7Input, NBT_Type::Compound &cpdV6Output)
{
	auto *pRoot = cpdV7Input.HasCompound(MU8STR(""));
	if (pRoot == NULL)
	{
		throw std::runtime_error("Root Compound not found!");
	}

	//获取根部，并插入根部，最后获取根部引用
	auto &cpdV7DataRoot = *pRoot;
	auto &cpdV6DataRoot = cpdV6Output.PutCompound(MU8STR(""), {}).first->second.GetCompound();

	//先处理版本信息
	auto *pMinecraftDataVersion = cpdV7DataRoot.HasInt(MU8STR("MinecraftDataVersion"));
	//auto *pVersion = cpdV7DataRoot.HasInt(MU8STR("Version"));
	//auto *pSubVersion = cpdV7DataRoot.HasInt(MU8STR("SubVersion"));

	//版本验证
	if (pMinecraftDataVersion == NULL || *pMinecraftDataVersion <= V6_MINECRAFT_DATA_VERSION)// || (pVersion != NULL && *pVersion <= V6_LITEMATIC_VERSION)//取消投影版本验证，仅验证mc数据版本
	{
		throw std::runtime_error("Minecraft Data Version Error!\n");
	}

	//基础数据
	auto *pMetadata = cpdV7DataRoot.HasCompound(MU8STR("Metadata"));
	if (pMetadata == NULL)
	{
		throw std::runtime_error("Metadata not found!\n");
	}

	//直接转移所有权，消除拷贝
	cpdV6DataRoot.PutCompound(MU8STR("Metadata"), std::move(*pMetadata));

	//设置基础版本信息
	cpdV6DataRoot.PutInt(MU8STR("MinecraftDataVersion"), V6_MINECRAFT_DATA_VERSION);
	cpdV6DataRoot.PutInt(MU8STR("Version"), V6_LITEMATIC_VERSION);
	cpdV6DataRoot.PutInt(MU8STR("SubVersion"), V6_LITEMATIC_SUBVERSION);

	//获取
	auto *pRegions = cpdV7DataRoot.HasCompound(MU8STR("Regions"));
	if (pRegions == NULL)
	{
		throw std::runtime_error("Regions not found!\n");
	}

	//插入选区根
	auto &cpdV6Regions = cpdV6DataRoot.PutCompound(MU8STR("Regions"), {}).first->second.GetCompound();

	//遍历选区
	for (auto &[sV7RegionName, nodeV7RegionData] : *pRegions)
	{
		auto &cpdNewV6RegionData = cpdV6Regions.PutCompound(sV7RegionName, {}).first->second.GetCompound();
		try
		{
			ProcessRegion(GetCompound(nodeV7RegionData), cpdNewV6RegionData, *pMinecraftDataVersion);
		}
		catch (const std::exception &e)
		{
			throw std::runtime_error(std::string(e.what()) += "\nProcessRegion fail!");
		}
	}
}
