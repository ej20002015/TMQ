#pragma once

#include <TMQCore/data_entity.h>

namespace TMQ
{

struct RDEntity : public DataEntity
{
	bool _active = false;
};

template<typename T>
concept c_RDEntity = std::is_base_of_v<RDEntity, T>;

template<c_RDEntity T>
class RDEntityTraits
{
public:
	static consteval const char* const table()    { static_assert( false ); }
	static std::string getID( const T& rdEntity ) { static_assert( false ); }
};

/*
* -------------- REFDATA ENTITY DEFINITIONS --------------
*/

// USER

struct User : public RDEntity
{
	std::string firstname;
	std::string surname;
	std::string desk;
	uint32_t age = 0;
};

template<>
class RDEntityTraits<User>
{
public:
	static consteval const char* const table()   { return "Users"; }
	static std::string getID( const User& user ) { return user.firstname + user.surname; }
};

}