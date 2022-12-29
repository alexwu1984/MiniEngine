#pragma once
#include "core/memory_manager.h"

namespace Engine
{
	class RenderCommand
	{
	public:
		RenderCommand(bool MustFlush = false) :_MustFlush(MustFlush) {}
		virtual ~RenderCommand() {}
		virtual unsigned int Execute() = 0;
		virtual const wchar_t* DescribeCommand() = 0;
		inline bool GetMustFlush() const
		{
			return _MustFlush;
		}
	protected:
		bool _MustFlush;
	};

	class  CommandConstantBuffer : public win32::memory_object
	{
	public:
		enum
		{
			Constant_BUFFER_SIZE = 6 * 1024 * 1024
		};
		//VSUserConstant Type
		CommandConstantBuffer() = default;
		~CommandConstantBuffer();
		uint8_t* Assign(uint32_t uiSize);
		template<typename T>
		uint8_t* Assign( uint32_t uiRegisterNum);
		void Clear();
	
	protected:
		std::mutex _Lock;
		std::vector<uint8_t> _Buffer;
		uint32_t _uiCurBufferP = 0;
	};

#define DECLARE_UNIQUE_RENDER_COMMAND(TypeName,Code) \
	class TypeName : public RenderCommand \
	{ \
	public: \
		TypeName() \
		{}\
		~TypeName()\
		{ \
		} \
		virtual unsigned int Execute() \
		{ \
			Code; \
			return sizeof(*this); \
		} \
		virtual const wchar_t* DescribeCommand() \
		{ \
			return _T( #TypeName ); \
		} \
	};

#define DECLARE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(TypeName,ParamType1,ParamName1,Code) \
	class TypeName : public RenderCommand \
	{ \
	public: \
		typedef ParamType1 _ParamType1; \
		TypeName() \
		{}\
		TypeName(const _ParamType1& In##ParamName1): \
			ParamName1(In##ParamName1) \
		{} \
		~TypeName()\
		{ \
		} \
		virtual unsigned int Execute() \
		{ \
			Code; \
			return sizeof(*this); \
		} \
		virtual const wchar_t* DescribeCommand() \
		{ \
			return _T( #TypeName ); \
		} \
	private: \
		ParamType1 ParamName1; \
	}; 

#define DECLARE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(TypeName,ParamType1,ParamName1,ParamType2,ParamName2,Code) \
	class TypeName : public RenderCommand \
	{ \
	public: \
		typedef ParamType1 _ParamType1; \
		typedef ParamType2 _ParamType2; \
		TypeName() \
		{}\
		~TypeName()\
		{ \
		} \
		TypeName(const _ParamType1& In##ParamName1,const _ParamType2& In##ParamName2): \
			ParamName1(In##ParamName1), \
			ParamName2(In##ParamName2) \
		{} \
		virtual unsigned int Execute() \
		{ \
			Code; \
			return sizeof(*this); \
		} \
		virtual const wchar_t* DescribeCommand() \
		{ \
			return _T( #TypeName ); \
		} \
	private: \
		ParamType1 ParamName1; \
		ParamType2 ParamName2; \
	};
#define DECLARE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(TypeName,ParamType1,ParamName1,ParamType2,ParamName2,ParamType3,ParamName3,Code) \
	class TypeName : public RenderCommand \
	{ \
	public: \
		typedef ParamType1 _ParamType1; \
		typedef ParamType2 _ParamType2; \
		typedef ParamType3 _ParamType3; \
		TypeName() \
		{}\
		TypeName(const _ParamType1& In##ParamName1,const _ParamType2& In##ParamName2,const _ParamType3& In##ParamName3): \
			ParamName1(In##ParamName1), \
			ParamName2(In##ParamName2), \
			ParamName3(In##ParamName3) \
		{} \
		~TypeName()\
		{ \
		} \
		virtual unsigned int Execute() \
		{ \
			Code; \
			return sizeof(*this); \
		} \
		virtual const wchar_t* DescribeCommand() \
		{ \
			return _T( #TypeName ); \
		} \
	private: \
		ParamType1 ParamName1; \
		ParamType2 ParamName2; \
		ParamType3 ParamName3; \
	};
}