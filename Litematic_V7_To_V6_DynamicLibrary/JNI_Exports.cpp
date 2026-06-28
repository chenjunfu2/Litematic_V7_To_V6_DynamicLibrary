#include "LitematicConversion.hpp"
#include "NBT_Erase.hpp"

#include <stdexcept>
#include <format>
#include <stdint.h>
#include <jni.h>
#include <compare>

#define JBYTEARRAY_MAXSIZE (INT32_MAX - 8)

/// @brief JNI 输入流适配器，用于从 jbyteArray 读取数据
class JNIInputStream
{
private:
	JNIEnv *env;
	jbyteArray data;
	jbyte *buffer;
	size_t size;
	size_t position;

public:
	/// @brief 构造函数，从 jbyteArray 创建输入流
	/// @param env JNI 环境指针
	/// @param input Java 字节数组
	JNIInputStream(JNIEnv *env, jbyteArray input)
		: env(env), data(input), buffer(nullptr), size(0), position(0)
	{
		if (input == nullptr)
		{
			throw std::runtime_error("Input jbyteArray is null");
		}

		size = env->GetArrayLength(input);
		buffer = env->GetByteArrayElements(input, nullptr);

		if (buffer == nullptr)
		{
			throw std::runtime_error("Failed to get byte array elements");
		}
	}

	/// @brief 析构函数，自动释放 JNI 资源
	~JNIInputStream()
	{
		if (buffer != nullptr)
		{
			env->ReleaseByteArrayElements(data, buffer, JNI_ABORT);
			buffer = nullptr;
		}
	}

	// 禁止拷贝和移动
	JNIInputStream(const JNIInputStream &) = delete;
	JNIInputStream(JNIInputStream &&) = delete;
	JNIInputStream &operator=(const JNIInputStream &) = delete;
	JNIInputStream &operator=(JNIInputStream &&) = delete;

	/// @brief 下标访问运算符
	uint8_t operator[](size_t index) const noexcept
	{
		return buffer[index];
	}

	/// @brief 获取下一个字节并推进读取位置
	uint8_t GetNext() noexcept
	{
		return buffer[position++];
	}

	/// @brief 从流中读取一段数据
	void GetRange(void *pDest, size_t szSize) noexcept
	{
		memcpy(pDest, &buffer[position], szSize);
		position += szSize;
	}

	/// @brief 回退一个字节
	void UnGet() noexcept
	{
		if (position > 0)
		{
			--position;
		}
	}

	/// @brief 获取当前读取位置的指针
	const uint8_t *CurData() const noexcept
	{
		return (uint8_t *)&buffer[position];
	}

	/// @brief 向后推进读取
	size_t AddIndex(size_t szSize) noexcept
	{
		position += szSize;
		return position;
	}

	/// @brief 向前撤销读取
	size_t SubIndex(size_t szSize) noexcept
	{
		position -= szSize;
		return position;
	}

	/// @brief 检查是否已到达流末尾
	bool IsEnd() const noexcept
	{
		return position >= size;
	}

	/// @brief 获取流的总大小
	size_t Size() const noexcept
	{
		return size;
	}

	/// @brief 检查是否还有足够的数据
	bool HasAvailData(size_t szSize) const noexcept
	{
		return (size - position) >= szSize;
	}

	/// @brief 重置流读取位置
	void Reset() noexcept
	{
		position = 0;
	}

	/// @brief 获取底层数据的起始指针
	const uint8_t *BaseData() const noexcept
	{
		return (const uint8_t *)buffer;
	}

	/// @brief 获取当前读取位置（只读）
	size_t Index() const noexcept
	{
		return position;
	}

	/// @brief 获取当前读取位置（可写）
	size_t &Index() noexcept
	{
		return position;
	}
};

/// @brief JNI 输出流适配器，用于将数据写入到 jbyteArray
/// @note 符合你定义的 DefaultOutputStream 鸭子类型接口
class JNIOutputStream
{
private:
	JNIEnv *env;
	std::vector<uint8_t> buffer;  // 先写入到 vector，最后再转为 jbyteArray

public:
	using StreamType = jbyteArray;
	using ValueType = uint8_t;

	/// @brief 构造函数
	JNIOutputStream(JNIEnv *env, size_t szReserve = 1024) : env(env)
	{
		//初始预留空间
		buffer.reserve(szReserve);
	}

	// 禁止拷贝和移动
	JNIOutputStream(const JNIOutputStream &) = delete;
	JNIOutputStream(JNIOutputStream &&) = delete;
	JNIOutputStream &operator=(const JNIOutputStream &) = delete;
	JNIOutputStream &operator=(JNIOutputStream &&) = delete;

	/// @brief 下标访问运算符（只读）
	const ValueType &operator[](size_t index) const noexcept
	{
		return buffer[index];
	}

	/// @brief 向流中写入单个值
	template<typename V>
	requires(std::is_constructible_v<ValueType, V &&>)
		void PutOnce(V &&c)
	{
		buffer.push_back(static_cast<ValueType>(std::forward<V>(c)));
	}

	/// @brief 向流中写入一段数据
	void PutRange(const ValueType *pData, size_t szSize)
	{
		size_t currentSize = buffer.size();
		buffer.resize(currentSize + szSize);
		memcpy(&buffer[currentSize], pData, szSize);
	}

	/// @brief 预分配额外容量
	void AddReserve(size_t szAddSize)
	{
		buffer.reserve(buffer.size() + szAddSize);
	}

	/// @brief 删除最后一个写入的字节
	void UnPut() noexcept
	{
		if (!buffer.empty())
		{
			buffer.pop_back();
		}
	}

	/// @brief 获取当前字节流大小
	size_t Size() const noexcept
	{
		return buffer.size();
	}

	/// @brief 重置流，清空所有数据
	void Reset() noexcept
	{
		buffer.clear();
	}

	/// @brief 获取底层数据的常量引用
	const std::vector<uint8_t> &Data() const noexcept
	{
		return buffer;
	}

	/// @brief 获取底层数据的非常量引用
	std::vector<uint8_t> &Data() noexcept
	{
		return buffer;
	}

	/// @brief 创建 jbyteArray 并转移数据所有权
	/// @return 新创建的 jbyteArray，需要调用者用 DeleteLocalRef 释放
	jbyteArray ToJByteArray()
	{
		if (buffer.size() > JBYTEARRAY_MAXSIZE)
		{
			throw std::runtime_error(std::format("JNI byte array size exceeds maximum allowed limit: {} bytes (max: {} bytes)", buffer.size(), JBYTEARRAY_MAXSIZE));
		}

		jsize len = (jsize)buffer.size();
		jbyteArray result = env->NewByteArray(len);

		if (result != nullptr && len > 0)
		{
			env->SetByteArrayRegion(result, 0, len, (const jbyte *)buffer.data());
		}

		return result;
	}
};

struct MyCompoundSort
{
	static inline uint8_t u8Enabled;

	static void Reset()
	{
		u8Enabled = 2;
	}

	/// @brief 对给定的 Compound 对象进行排序，返回指向其元素的迭代器向量。
	/// @param cpdSort 需要排序的 Compound 对象。
	/// @return `std::vector<NBT_Type::Compound::Const_Iterator>`，其中迭代器按排序顺序排列。
	std::vector<NBT_Type::Compound::Const_Iterator> operator()(const NBT_Type::Compound &cpdSort)
	{
		if (u8Enabled == 0 || u8Enabled-- > 1)
		{
			return cpdSort.KeySortIt<>();
		}

		//第二层使用自定义排序
		std::vector<NBT_Type::Compound::Const_Iterator> vSortCompound{};
		vSortCompound.reserve(cpdSort.Size());
		for (auto it = cpdSort.begin(), end = cpdSort.end(); it != end; ++it)
		{
			vSortCompound.push_back(it);
		}

		std::sort(vSortCompound.begin(), vSortCompound.end(),
			[](const auto &l, const auto &r) -> bool
			{
				static std::unordered_map<NBT_Type::String, uint64_t> mapPriority =
				{
					{MU8STR("MinecraftDataVersion"),	0},
					{MU8STR("Version"),					1},
					{MU8STR("SubVersion"),				2},
					{MU8STR("Metadata"),				3},
					{MU8STR("Regions"),					4},
				};

				auto itL = mapPriority.find(l->first);
				auto itR = mapPriority.find(r->first);

				uint64_t u64LPriority = itL == mapPriority.end() ? (uint64_t)-1 : itL->second;
				uint64_t u64RPriority = itR == mapPriority.end() ? (uint64_t)-1 : itR->second;

				if (u64LPriority != u64RPriority)//都没找到才不成立
				{
					return u64LPriority < u64RPriority;
				}
				else
				{
					return l->first < r->first;
				}
			}
		);

		return vSortCompound;
	}
};

/*
 * java.lang 包下的所有基础异常类（按字母序排列）：
 *
 * 一、RuntimeException 及其子类（非检查型异常）：
 *   - ArithmeticException: 算术条件异常，如整数除以零
 *   - ArrayIndexOutOfBoundsException: 数组索引越界
 *   - ArrayStoreException: 向数组中存储了错误类型的对象
 *   - ClassCastException: 试图将对象强制转换为非其子类的类型
 *   - IllegalArgumentException: 向方法传递了非法或不合适的参数
 *   - IllegalMonitorStateException: 线程试图在未持有对象监视器的情况下等待或通知
 *   - IllegalStateException: 在不合适的时间调用了方法，或当前环境不满足操作要求
 *   - IllegalThreadStateException: 线程在不合适的状态下被执行了操作
 *   - IndexOutOfBoundsException: 索引（如数组、字符串或向量）超出范围
 *   - NegativeArraySizeException: 试图创建大小为负的数组
 *   - NullPointerException: 在需要对象的地方使用了 null 引用
 *   - NumberFormatException: 试图将字符串转换为数值类型，但格式不正确（IllegalArgumentException 的子类）
 *   - SecurityException: 安全管理器抛出的异常，表示安全违规
 *
 * 二、其他 Exception 子类（检查型异常）：
 *   - ClassNotFoundException: 类加载器无法找到指定的类
 *   - CloneNotSupportedException: 试图克隆一个未实现 Cloneable 接口的对象
 *   - IllegalAccessException: 试图访问（如反射）一个不可访问的类、方法或字段
 *   - InstantiationException: 试图通过 newInstance() 创建一个抽象类或接口的实例
 *   - InterruptedException: 线程在等待、休眠等操作中被中断
 *
 * 注：Exception 和 RuntimeException 本身也是 java.lang 下的，但通常不作为“具体异常”直接抛出。
 */
#define ThrowJavaException(except_name, except_reason, return_value)\
do\
{\
	/*如果这里类找不到，那么JVM会自动设置ClassNoFoundException，下面检测到nullptr直接返回即可*/\
	jclass exceptionClass = env->FindClass(except_name);\
	if (exceptionClass != nullptr)\
	{\
		env->ThrowNew(exceptionClass, (except_reason));\
		env->DeleteLocalRef(exceptionClass);\
	}\
	return return_value;\
} while (false)

enum class JNI_Operator : jint
{
	UNKNOWN = 0,
	CONVERT_V7_TO_V6 = 1,
	SORT_FIELDS = 2,
	ERASE_FIELDS = 3,

	ENUM_END,
};

std::strong_ordering operator<=>(JNI_Operator l, jint r)
	{
		return (jint)l <=> r;
	}

std::strong_ordering operator<=>(jint l, JNI_Operator r)
	{
		return l <=> (jint)r;
	}

bool operator==(JNI_Operator l, jint r)
	{
		return (jint)l == r;
	}

bool operator==(jint l, JNI_Operator r)
	{
		return l == (jint)r;
	}

bool operator!=(JNI_Operator l, jint r)
	{
		return (jint)l != r;
	}

bool operator!=(jint l, JNI_Operator r)
	{
		return l != (jint)r;
	}

JNI_Operator JintToJNI_Operator(jint op)
	{
		if (op < 0 || op >= JNI_Operator::ENUM_END)
		{
			return JNI_Operator::UNKNOWN;
		}

		return (JNI_Operator)op;
	}


template <typename JArrayType>
struct JNIArrayHelper;

template <>
struct JNIArrayHelper<jbyteArray>
	{
		using value_type = jbyte;

		static value_type *GetElements(JNIEnv *env, jbyteArray arr, jboolean *isCopy)
		{
			return env->GetByteArrayElements(arr, isCopy);
		}
		static void ReleaseElements(JNIEnv *env, jbyteArray arr, value_type *elems, jint mode)
		{
			env->ReleaseByteArrayElements(arr, elems, mode);
		}
	};

template <>
struct JNIArrayHelper<jintArray>
	{
		using value_type = jint;

		static value_type *GetElements(JNIEnv *env, jintArray arr, jboolean *isCopy)
		{
			return env->GetIntArrayElements(arr, isCopy);
		}
		static void ReleaseElements(JNIEnv *env, jintArray arr, value_type *elems, jint mode)
		{
			env->ReleaseIntArrayElements(arr, elems, mode);
		}
	};

template <>
struct JNIArrayHelper<jlongArray>
	{
		using value_type = jlong;

		static value_type *GetElements(JNIEnv *env, jlongArray arr, jboolean *isCopy)
		{
			return env->GetLongArrayElements(arr, isCopy);
		}
		static void ReleaseElements(JNIEnv *env, jlongArray arr, value_type *elems, jint mode)
		{
			env->ReleaseLongArrayElements(arr, elems, mode);
		}
	};


template<typename JNIArrayType>
struct JNIReadOnlyArray
	{
	public:
		using ArrHelper = JNIArrayHelper<JNIArrayType>;

	public:
		JNIEnv * const env;
		const JNIArrayType jarray;
		const jsize size;
		const typename ArrHelper::value_type *buffer;

	public:
		JNIReadOnlyArray(JNIEnv *_env, JNIArrayType _jarray) :
			env(_env),
			jarray(_jarray),
			size(env->GetArrayLength(_jarray)),
			buffer(ArrHelper::GetElements(env, jarray, nullptr))
		{}
		~JNIReadOnlyArray(void)
		{
			ArrHelper::ReleaseElements(env, jarray, const_cast<ArrHelper::value_type *>(buffer), JNI_ABORT);
		}

		const typename ArrHelper::value_type &operator[](jsize index) const
		{
			return buffer[index];
		}
	};

struct NBT_Print2String
{
public:
	using Level = NBT_Print_Level;

public:
	std::string &strPrint;

public:
	template<typename... Args>
	void operator()(Level lvl, const std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		strPrint += std::format(std::move(fmt), std::forward<Args>(args)...);
	}

	template<typename... Args>
	void operator()(const std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		strPrint += std::format(std::move(fmt), std::forward<Args>(args)...);
	}
};

/*
ops[i]为第i个对nbtData数据进行的操作
paramBlocks[i]的i与前相同，为第i个对nbtData数据进行操作时需要用到的额外参数获取方式
[paramData[paramBlocks[i] >> 32] , paramData[paramBlocks[i] >> 32 + paramBlocks[i] & 0xFFFFFFFFL])为对第i个对nbtData数据进行操作时需要用到的额外参数
*/
extern "C" JNIEXPORT jbyteArray JNICALL Java_dev_shun_litematica_extra_SchematicNativeReader_nativeExecute(JNIEnv *env, [[maybe_unused]] jclass clazz, jbyteArray nbtData, jintArray ops, jlongArray paramBlocks, jbyteArray paramData) try
{
	if (nbtData == nullptr || ops == nullptr || paramBlocks == nullptr || paramData == nullptr)
	{
		ThrowJavaException("java/lang/IllegalArgumentException", "Native method received null argument", nullptr);
	}

	//初始化操作数组
	JNIReadOnlyArray<jintArray> arrOps{ env, ops };
	JNIReadOnlyArray<jlongArray> arrParamBlocks{ env, paramBlocks };
	JNIReadOnlyArray<jbyteArray> arrParamData{ env, paramData };
	if (arrOps.size != arrParamBlocks.size)
	{
		ThrowJavaException("java/lang/IllegalArgumentException", "[ops] array and [paramBlocks] array must have the same size", nullptr);
	}


	NBT_Type::Compound cpdNBTData{};
	size_t szInputStreamSize = 0;//优化用
	{
		JNIInputStream nbtInputStream{ env, nbtData };
		szInputStreamSize = nbtInputStream.Size();
		std::string strErrMsg{};
		if (!NBT_Reader::ReadNBT(nbtInputStream, cpdNBTData, 512, NBT_Print2String{ strErrMsg }))
		{
			ThrowJavaException("java/io/IOException", std::format("Failed to read NBT data from input stream, info:\n{}\n", strErrMsg).c_str(), nullptr);
		}
	}

	//是否自定义排序输出
	bool bUseMySortOutput = false;
	for (jsize i = 0; i < arrOps.size; ++i)
	{
		JNI_Operator curop = JintToJNI_Operator(arrOps[i]);
		switch (curop)
		{
		case JNI_Operator::CONVERT_V7_TO_V6:
			{
				if (arrParamBlocks[i] != (jlong)0)//必须无参数
				{
					ThrowJavaException("java/lang/IllegalArgumentException", std::format("Operation CONVERT_V7_TO_V6 expects no parameters, but paramBlocks[{}] = {} is not 0", i, arrParamBlocks[i]).c_str(), nullptr);
				}

				NBT_Type::Compound cpdNewNBTData{};
				std::string strErrMsg;
				if (!ConvertLitematicData_V7_To_V6(cpdNBTData, cpdNewNBTData, strErrMsg))//从cpdNBTData转换到cpdNewNBTData
				{
					ThrowJavaException("java/lang/IllegalStateException", std::format("Failed to convert V7 data to V6: {}", strErrMsg).c_str(), nullptr);
				}

				cpdNBTData = std::move(cpdNewNBTData);
			}
			break;
		case JNI_Operator::SORT_FIELDS:
			{
				if (arrParamBlocks[i] != (jlong)0)//必须无参数
				{
					ThrowJavaException("java/lang/IllegalArgumentException", std::format("Operation SORT_FIELDS expects no parameters, but paramBlocks[{}] = {} is not 0", i, arrParamBlocks[i]).c_str(), nullptr);
				}

				bUseMySortOutput = true;//仅设置标签，最终影响输出排序
			}
			break;
		case JNI_Operator::ERASE_FIELDS:
			{
				if (arrParamBlocks[i] == (jlong)0)//必须有参数
				{
					ThrowJavaException("java/lang/IllegalArgumentException", std::format("Operation ERASE_FIELDS expects parameters, but paramBlocks[{}] = {} is bad", i, arrParamBlocks[i]).c_str(), nullptr);
				}

				//解析高字节（偏移）低字节（长度）
				uint32_t u32BaseIndex = (uint64_t)arrParamBlocks[i] >> 32 & 0x00'00'00'00'FF'FF'FF'FFL;
				uint32_t u32DataSize = (uint64_t)arrParamBlocks[i] >> 0 & 0x00'00'00'00'FF'FF'FF'FFL;

				//io流
				std::vector<uint8_t> data{};
				data.resize(u32DataSize);
				memcpy(data.data(), &arrParamData.buffer[u32BaseIndex], u32DataSize);
				NBT_IO::DefaultInputStream<std::vector<uint8_t>> iptStream(data);

				//字节序
				uint32_t u32MapSize = 0;

				if (!iptStream.HasAvailData(sizeof(u32MapSize)))
				{
					ThrowJavaException("java/lang/IllegalArgumentException", "ERASE_FIELDS: param data too short, cannot read map size", nullptr);
				}

				iptStream.GetRange(&u32MapSize, sizeof(u32MapSize));
				u32MapSize = NBT_Endian::BigToNativeAny(u32MapSize);

				//接下来是1字节模式，2字节长度与字符串的循环，前面的size代表循环有多少个
				NBTErase::RequestList requests{};
				for (uint32_t i = 0; i < u32MapSize; ++i)
				{
					uint8_t u8Mode = 0;
					uint16_t u16StrSize = 0;

					if (!iptStream.HasAvailData(sizeof(u8Mode) + sizeof(u16StrSize)))
					{
						ThrowJavaException("java/lang/IllegalArgumentException", "ERASE_FIELDS: param data too short, cannot read erase mode and path length", nullptr);
					}

					iptStream.GetRange(&u8Mode, sizeof(u8Mode));
					iptStream.GetRange(&u16StrSize, sizeof(u16StrSize));
					u16StrSize = NBT_Endian::BigToNativeAny(u16StrSize);

					if (u8Mode > 1)//只能是0和1
					{
						ThrowJavaException("java/lang/IllegalArgumentException", std::format("ERASE_FIELDS: invalid erase mode {}, expected 0 (CLEAR) or 1 (REMOVE)", u8Mode).c_str(), nullptr);
					}

					if (!iptStream.HasAvailData(u16StrSize))
					{
						ThrowJavaException("java/lang/IllegalArgumentException", std::format("ERASE_FIELDS: param data too short, expected {} bytes for path name", u16StrSize).c_str(), nullptr);
					}

					try
					{
						NBT_Type::String::View curPath{ (MUTF8_Char_Type *)&iptStream[iptStream.Index()], (size_t)u16StrSize };
						iptStream.SkipData(u16StrSize);

						NBTErase::Request reqNew = { NbtPath::PathParser(curPath), (NBTErase::Request::EraseMode)u8Mode };
						requests.push_back(std::move(reqNew));
					}
					catch (const NbtPath::ParseError &e)
					{
						ThrowJavaException("java/lang/IllegalArgumentException", std::format("ERASE_FIELDS: NBT path parse error: {}", e.what()).c_str(), nullptr);
					}
				}

				//路径解析处理完成，进行建树后删除
				try
				{
					auto pathTrieTree = NBTErase::EraseRequest2NbtPathTrieTree(requests);
					NBTErase::NbtParseToErase(cpdNBTData, pathTrieTree);
				}
				catch (const NBTErase::EraseError &e)
				{
					ThrowJavaException("java/lang/IllegalStateException", std::format("ERASE_FIELDS: NBT erase execution error: {}", e.what()).c_str(), nullptr);
				}
			}
			break;
		case JNI_Operator::UNKNOWN:
		default:
			ThrowJavaException("java/lang/IllegalArgumentException", std::format("Unsupported Operator at index: {}", i).c_str(), nullptr);
			break;
		}
	}


	// 创建输出流
	JNIOutputStream outputStream(env, szInputStreamSize);
	if (bUseMySortOutput)
	{
		std::string strErrMsg{};
		MyCompoundSort::Reset();
		if (!NBT_Writer::WriteNBT<MyCompoundSort>(outputStream, cpdNBTData, 512, NBT_Print2String{ strErrMsg }))
		{
			ThrowJavaException("java/io/IOException", std::format("Failed to write NBT data to output stream, info:\n{}\n", strErrMsg).c_str(), nullptr);
		}
	}
	else
	{
		std::string strErrMsg{};
		if (!NBT_Writer::WriteNBT(outputStream, cpdNBTData, 512, NBT_Print2String{ strErrMsg }))
		{
			ThrowJavaException("java/io/IOException", std::format("Failed to write NBT data to output stream, info:\n{}\n", strErrMsg).c_str(), nullptr);
		}
	}
	
	// 返回结果
	return outputStream.ToJByteArray();
}
catch (std::exception &e)
{
	ThrowJavaException("java/lang/RuntimeException", std::format("Unexpected native C++ exception: {}", e.what()).c_str(), nullptr);
}
catch (...)
{
	ThrowJavaException("java/lang/RuntimeException", "Unknown native C++ exception occurred", nullptr);
}
