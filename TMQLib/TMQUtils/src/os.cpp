#include <TMQUtils/os.h>

#include <TMQUtils/error.h>
#include <TMQUtils/str.h>

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <processthreadsapi.h>
#else
	#include <unistd.h>
	#include <sys/types.h>
	#include <pthread.h>
#endif

#include <filesystem>
#include <atomic>

namespace TMQ
{

namespace OS
{

std::string iProcName()
{
	std::string procName;

	#ifdef _WIN32

	char buffer[4096];
	const DWORD pathLen = GetModuleFileNameA( nullptr, buffer, sizeof( buffer ) );
	if( pathLen == 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER )
		throw TMQException( "Could not get process name: GetModuleFileNameA failed" );
	else
	{
		const std::filesystem::path fullExePath = buffer;
		procName = fullExePath.filename().string();
	}

	#else

	char buffer[4096];
	const ssize_t len = readlink( "/proc/self/exe", buffer, sizeof( buffer ) - 1 ); // Readlink doesn't null-terminate
	if( len == -1 )
		throw TMQException( "Could not get process name: failed to get /proc/self/exe symlink" );
	else
	{
		buffer[len] = '\0'; // Null-terminate the buffer
		const std::filesystem::path fullExePath = buffer;
		procName = fullExePath.filename().string();
	}

	#endif

	return procName;
}

std::string_view procName()
{
	static const std::string procName = iProcName();
	return procName;
}

int32_t iProcID()
{
	#ifdef _WIN32
	return static_cast<int32_t>( GetCurrentProcessId() );
	#else
	return static_cast<int32_t>( getpid() );
	#endif
}

int32_t procID()
{
	static const int32_t procID = iProcID();
	return procID;
}

static thread_local std::atomic<bool> t_updatedThreadName = false;

std::string iThreadName()
{
	std::string threadName = "unnamed_thread";

	#ifdef _WIN32

	PWSTR threadDescr = nullptr;
	HRESULT hr = GetThreadDescription( GetCurrentThread(), &threadDescr );
	if( SUCCEEDED( hr ) && threadDescr != nullptr )
	{
		std::wstring wstr = threadDescr;
		if( !wstr.empty() )
			threadName = Str::wstr2Str( wstr );
		LocalFree( threadDescr );
	}

	#else

	char buffer[256] = { 0 };
	if( pthread_getname_np( pthread_self(), buffer, sizeof( buffer ) ) == 0 && buffer[0] != '\0' )
		threadName = buffer;

	#endif

	return threadName;
}

std::string_view threadName()
{
	static thread_local std::string threadName = iThreadName();
	bool expectChanged = true;
	if( t_updatedThreadName.compare_exchange_strong( expectChanged, false ) )
		threadName = iThreadName();

	return threadName;
}

void setThreadName( const std::string_view name )
{
	static constexpr auto MAX_NAME_LEN = 15; // Linux limits thread names to 15 chars long (excl null term)
	if( name.size() > MAX_NAME_LEN )
		throw TMQException( std::format( "Length of given thread name [{0}] exceeds maximum of ", name.size(), MAX_NAME_LEN ) );

	#ifdef _WIN32

	std::wstring wideName = Str::str2Wstr( name.data() );
	HRESULT hr = SetThreadDescription( GetCurrentThread(), wideName.c_str() );
	if( FAILED( hr ) )
		throw TMQException( std::format( "Could not set thread name: SetThreadDescription failed: {0}", hr ) );

	#else

	int result = pthread_setname_np( pthread_self(), name.data() );
	if( result != 0  )
		throw TMQException( std::format( "Could not set thread name: pthread_setname_np failed: {0}", result ) );

	#endif

	t_updatedThreadName.store( true );
}

int32_t iThreadID()
{
	#ifdef _WIN32
	return static_cast<int32_t>( GetCurrentThreadId() );
	#else
	return static_cast<int32_t>( gettid() );
	#endif
}

int32_t threadID()
{
	static const thread_local int32_t threadID = iThreadID();
	return threadID;
}

OS::DynaLib::DynaLib( const std::string_view name )
{
	load( name );
}

DynaLib::~DynaLib()
{
	unload();
}

void DynaLib::load( const std::string_view name )
{
	if( isLoaded() )
		return;

	setName( name );
	resetError();

	#ifdef _WIN32

	HINSTANCE hInst = LoadLibrary( m_name.c_str() );
	if( !hInst )
		throw TMQException( std::format( "Could not load dynalib {0}: {1}", m_name, getLastError() ) );

	m_nativeHandle = reinterpret_cast<void*>( hInst );

	#else

	void* dl = dlopen( m_name.c_str(), RTLD_LAZY );
	if( !dl )
		throw TMQException( std::format( "Could not load dynalib {0}: {1}", m_name, getLastError() ) );

	m_nativeHandle = handle;

	#endif
}

void DynaLib::unload()
{
	if( !isLoaded() )
		return;

	resetError();

	#ifdef _WIN32

	BOOL freeSuccess = FreeLibrary( reinterpret_cast<HMODULE>( m_nativeHandle ) );
	if( !freeSuccess )
		throw TMQException( std::format( "Could not unload dynalib {0}: {1}", m_name, getLastError() ) );

	#else

	int32_t errCode = dlclose( m_nativeHandle );
	if( errCode )
		throw TMQException( std::format( "Could not unload dynalib {0}: {1}", m_name, getLastError() ) );

	#endif

	m_nativeHandle = nullptr;
	m_name.clear();
}

void DynaLib::setName( const std::string_view name )
{
	#ifdef _WIN32
	m_name = std::format( "{0}.dll", name );
	#else
	m_name = std::format( "{0}.so", name );
	#endif
}

void DynaLib::resetError() const
{
	#ifdef _WIN32
	SetLastError( 0 );
	#else
	dlerror();
	#endif
}

std::string DynaLib::getLastError() const
{
	std::string errMsg;

	#ifdef _WIN32

	DWORD errorCode = GetLastError();
	if( errorCode != 0 )
	{
		LPSTR messageBuffer = nullptr;
		// Ask Win32 to allocate the buffer and format the message.
		size_t size = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, errorCode, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPSTR)&messageBuffer, 0, NULL );

		errMsg = std::string( messageBuffer, size );

		LocalFree( messageBuffer );

		// Remove trailing newline characters often included by FormatMessage
		while( !errMsg.empty() && ( errMsg.back() == '\n' || errMsg.back() == '\r' ) )
			errMsg.pop_back();
	}

	#else

	const char* errCStr = dlerror();
	if( errCStr )
		errStr = errCStr;

	#endif

	return errMsg;
}

void* DynaLib::iGetFunc( const std::string_view funcName ) const
{
	if( !isLoaded() )
		throw TMQException( std::format( "Could not get {0} function from dynalib as it hasn't been loaded", funcName ) );

	resetError();
	void* funcPtr = nullptr;

	#ifdef _WIN32

	FARPROC procAddr = GetProcAddress( reinterpret_cast<HMODULE>( m_nativeHandle ), funcName.data() );
	funcPtr = static_cast<void*>( procAddr );

	#else

	funcPtr = dlsym( m_nativeHandle, funcName.data() );

	#endif

	if( !funcPtr )
		throw TMQException( std::format( "Could not get {0} function from dynalib {1}: {2}", funcName, m_name, getLastError() ) );

	return funcPtr;
}

}

}