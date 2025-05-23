#include <TMQCore/refdata.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace TMQ;

// Mock RefDataSource class
class MockRefDataSource : public RefDataSource
{
public:
    MOCK_METHOD( std::vector<FetchData>, fetchLatest, ( const std::string_view table ), ( override ) );
    MOCK_METHOD( std::vector<FetchData>, fetchAsOf, ( const std::string_view table, const std::chrono::system_clock::time_point ts ), ( override ) );
    MOCK_METHOD( void, insert, ( const std::string_view table, const std::vector<InsertData>& insData ), ( override ) );
};

class RefDataTest : public ::testing::Test
{
protected:
    std::shared_ptr<MockRefDataSource> m_mockSource;
    std::shared_ptr<LiveRDCache<User>> m_lrdCache;

    void SetUp() override
    {
        m_mockSource = std::make_shared<MockRefDataSource>();
        m_lrdCache = std::make_shared<LiveRDCache<User>>( m_mockSource );
    }

    User createTestUser( const std::string& firstname, const std::string& surname, const std::chrono::system_clock::time_point& ts )
    {
        User user;
        user.firstname = firstname;
        user.surname = surname;
        user.desk = "A1";
        user.age = 30;
        user._active = true;
        user._lastUpdatedTs = ts;
        return user;
    }
};

TEST_F( RefDataTest, InsertAndFetchTest )
{
    User user = createTestUser( "Evan", "James", std::chrono::system_clock::now() );

    // Create RefData instance
    RefData<User> refData( m_lrdCache );
    auto fetched = refData.get( "EvanJames" );
    EXPECT_FALSE( fetched.has_value() ); // No data should be found

    // Insert data
    std::vector<User> users = { user };
    EXPECT_CALL( *m_mockSource, insert( "Users", testing::_ ) ).Times( 1 );
    RefDBInserter ins( m_mockSource );
    ins.insert<User>( users, m_lrdCache );

    std::vector<RefDataSource::FetchData> fetchData;
    fetchData.emplace_back( user._lastUpdatedTs, "Evan", serialise( user ) );

    // Now fetch the data after insert
    EXPECT_CALL( *m_mockSource, fetchLatest( "Users" ) )
        .WillOnce( testing::Return( std::move( fetchData ) ) );

    m_lrdCache->reload();
    auto fetchedData = refData.get( "EvanJames" );
    EXPECT_TRUE( fetchedData.has_value() ); // Data should now be available
    EXPECT_EQ( fetchedData->get()._lastUpdatedBy, "Evan" );
}

TEST_F( RefDataTest, StaleDataInsertTest )
{
    User user = createTestUser( "Evan", "James", std::chrono::system_clock::now() );
    User staleUser = createTestUser( "Evan", "James", std::chrono::system_clock::now() - std::chrono::hours( 1 ) );

    std::vector<RefDataSource::FetchData> fetchData;
    fetchData.emplace_back( user._lastUpdatedTs, "Evan", serialise( user ) );

    // Mock fetchLatest to return the newer entity
    EXPECT_CALL( *m_mockSource, fetchLatest( "Users" ) )
        .WillOnce( testing::Return( std::move( fetchData ) ) );

    // Create RefData instance and insert stale data
    RefDBInserter ins( m_mockSource, RefDBInserter::StaleCheck::FROM_LIVERD_FORCE_REFRESH );
    EXPECT_FALSE( ins.insert<User>( staleUser, m_lrdCache ) ); // Insert should fail due to stale data
}

TEST_F( RefDataTest, CacheReloadTest )
{
    User user = createTestUser( "Evan", "James", std::chrono::system_clock::now() );
    
    // Mock fetchLatest to return the entity
    EXPECT_CALL( *m_mockSource, fetchLatest( "Users" ) )
        .WillRepeatedly( [user] ()
        {
            std::vector<RefDataSource::FetchData> fetchDataResult;
            fetchDataResult.emplace_back( RefDataSource::FetchData{
                user._lastUpdatedTs,
                "Evan",
                serialise( user )
            } );
            return fetchDataResult;
    } );

    // Create RefData instance and load data
    m_lrdCache->reload();
    RefData<User> refData( m_lrdCache );
    auto fetchedData = refData.get( "EvanJames" );
    EXPECT_TRUE( fetchedData.has_value() ); // Data should be fetched

    // Now reload the cache
    m_lrdCache->reload();
    auto reloadedData = refData.get( "EvanJames" );
    EXPECT_TRUE( reloadedData.has_value() ); // Data should still be available after reload
}

TEST_F( RefDataTest, GlobalRefDataSourceSet )
{
    const auto oldGlobal = GlobalRefDataSource::get();
    GlobalRefDataSource::CreatorFunc func = [this] () { return m_mockSource; };
    GlobalRefDataSource::setFunc( func );
    EXPECT_EQ( GlobalRefDataSource::get(), m_mockSource );
    GlobalRefDataSource::setFunc( [oldGlobal] () { return oldGlobal; } );
}

//TMQ::User createTestUser()
//{
//    TMQ::User user;
//    user.firstname = "Jeff";
//    user.surname = "Jones";
//    user.desk = "A1";
//    user.age = 30;
//    user._active = true;
//    // Set during insert
//    /*user._lastUpdatedBy = "admin";
//    user._lastUpdatedTm = std::chrono::system_clock::now();*/
//    return user;
//}

//TEST( RefDataTests, GeneralUse )
//{
//    //TMQ::RefDBInserter ins;
//
//    //std::vector<TMQ::User> users;
//    //users.reserve( 10'000 ); // Reserve space to avoid reallocations
//
//    //for( int i = 0; i < 10'000; ++i )
//    //{
//    //    TMQ::User user;
//    //    user.firstname = "John" + std::to_string( i );
//    //    user.surname = "Doe";
//    //    user.desk = "A" + std::to_string( i % 100 ); // Cycle desks from A0 to A99
//    //    user.age = 20 + ( i % 40 ); // Ages between 20 and 59
//    //    user._active = ( i % 10 ) != 0; // Mark every 10th user as inactive
//
//    //    users.push_back( std::move( user ) );
//    //}
//
//    //ins.insert<TMQ::User>( std::move( users ) );
//
//    TMQ::RefDBInserter ins( TMQ::RefDBInserter::StaleCheck::FROM_LIVERD_FORCE_REFRESH );
//
//    bool valid = ins.insert<TMQ::User>( createTestUser() );
//    std::cout << valid << std::endl;
//}
//TEST( RefDataTests, TempUserTest )
//{
//    auto start = std::chrono::system_clock::now();
//    RefData<User> rd;
//    auto end = std::chrono::system_clock::now();
//    std::cout << "Time to load users: " << std::chrono::duration_cast<std::chrono::milliseconds>( end - start ) << std::endl;
//    auto tmp = rd.get( "JeffJones" );
//    std::cout << tmp->get()._lastUpdatedTs;
//}
