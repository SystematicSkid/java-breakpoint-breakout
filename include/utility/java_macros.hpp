#pragma once
#include <windows.h>
#include <cstdint>

//
// Structure padding
//
#define STR_MERGE_IMPL(a, b)            a##b
#define STR_MERGE(a,b)                  STR_MERGE_IMPL(a,b)
#define MAKE_PAD(size)                  char STR_MERGE(pad_, __COUNTER__ )[ size ]

//
// Property macros
//
#define PROPERTY(type, name)            __declspec(property(get = property_get_##name, put = property_put_##name)) type name
#define PROPERTY_GET(type, name)        inline type property_get_##name()
#define PROPERTY_PUT(type, name)        inline void property_put_##name(type val)

//
// JVM macros
//

#define SET_STATIC_CLASS( klass )	static jclass get_class( ) noexcept					                                \
{																											            \
	thread_local static jclass java_klass = nullptr;																    \
	if( !java_klass )																							        \
	{																										            \
		java_klass = java_interop->find_class( #klass );																\
	}																										            \
	return java_klass;																						            \
}

#define SET_MEMBER(type, name, signature) PROPERTY(type, name);									\
PROPERTY_GET(type, name)                                                                        \
{                                                                                               \
	static std::size_t STR_MERGE(name, _offset) = 0x10;											\
	if(!STR_MERGE(name, _offset))																\
	{																							\
		auto field = java_interop->find_field(get_class( ), #name, signature);			        \
		if(!field) return NULL;																	\
		STR_MERGE(name, _offset) = (uint32_t)field >> 2;					                    \
	}																							\
	return *reinterpret_cast<type*>(                                                            \
		reinterpret_cast<std::uintptr_t>( this ) +	STR_MERGE(name, _offset)					\
	);                                                                                          \
}																								