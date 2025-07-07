#include <cmcpp.hpp>
#include "../../../test/host-util.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>

using namespace cmcpp;

// Resource Examples for Component Model C++
// This demonstrates various resource types and operations

// Example 1: Simple Resource with Destructor
class FileHandle
{
public:
    std::string filename;
    bool is_open;

    FileHandle(const std::string &name) : filename(name), is_open(true)
    {
        std::cout << "FileHandle: Opening file '" << filename << "'" << std::endl;
    }

    ~FileHandle()
    {
        if (is_open)
        {
            std::cout << "FileHandle: Closing file '" << filename << "'" << std::endl;
        }
    }

    void close()
    {
        if (is_open)
        {
            std::cout << "FileHandle: Manually closing file '" << filename << "'" << std::endl;
            is_open = false;
        }
    }

    std::string read_content() const
    {
        if (!is_open)
            return "";
        return "Content of " + filename;
    }
};

// Resource destructor function
void file_destructor(const FileHandle &handle)
{
    std::cout << "Resource destructor called for: " << handle.filename << std::endl;
    // In a real implementation, this would clean up the resource
}

// Example 2: Database Connection Resource
class DatabaseConnection
{
public:
    std::string connection_string;
    uint32_t connection_id;

    DatabaseConnection(const std::string &conn_str, uint32_t id)
        : connection_string(conn_str), connection_id(id)
    {
        std::cout << "DB: Connected to " << connection_string << " (ID: " << connection_id << ")" << std::endl;
    }

    std::vector<std::string> query(const std::string &sql) const
    {
        std::cout << "DB: Executing query: " << sql << std::endl;
        return {"row1", "row2", "row3"};
    }
};

void db_destructor(const DatabaseConnection &conn)
{
    std::cout << "DB: Disconnecting from " << conn.connection_string
              << " (ID: " << conn.connection_id << ")" << std::endl;
}

// Host function that creates and returns an owned file resource
own_t<FileHandle> create_file(const string_t &filename)
{
    std::cout << "\n=== Host Function: create_file ===\n";
    std::cout << "Creating file resource for: " << filename << std::endl;

    // Create the resource type with destructor
    static ResourceType<FileHandle> file_rt(file_destructor, true);

    // Create and return owned resource
    FileHandle file_handle(filename);
    own_t<FileHandle> owned_file(&file_rt, std::move(file_handle));

    std::cout << "File resource created successfully\n";
    return owned_file;
}

// Host function that takes a borrowed file resource and reads from it
string_t read_file_content(borrow_t<FileHandle> file_borrow)
{
    std::cout << "\n=== Host Function: read_file_content ===\n";

    if (!file_borrow.valid())
    {
        std::cout << "Error: Invalid file resource\n";
        return "ERROR: Invalid file handle";
    }

    const FileHandle &file = file_borrow.get();
    std::cout << "Reading from borrowed file: " << file.filename << std::endl;

    std::string content = file.read_content();
    std::cout << "Content read: " << content << std::endl;

    return content;
}

// Host function that takes ownership of a file and closes it
void_t close_file(own_t<FileHandle> file_owned)
{
    std::cout << "\n=== Host Function: close_file ===\n";

    if (!file_owned.valid())
    {
        std::cout << "Error: Invalid file resource\n";
        return;
    }

    FileHandle file = file_owned.release();
    std::cout << "Taking ownership of file: " << file.filename << std::endl;

    // Manual cleanup
    file.close();
    std::cout << "File closed and ownership transferred\n";
}

// Host function that creates a database connection
own_t<DatabaseConnection> connect_database(const string_t &connection_string)
{
    std::cout << "\n=== Host Function: connect_database ===\n";
    std::cout << "Connecting to database: " << connection_string << std::endl;

    static ResourceType<DatabaseConnection> db_rt(db_destructor, true);
    static uint32_t next_id = 1;

    DatabaseConnection db_conn(connection_string, next_id++);
    own_t<DatabaseConnection> owned_db(&db_rt, std::move(db_conn));

    std::cout << "Database connection created\n";
    return owned_db;
}

// Host function that queries a database using borrowed connection
list_t<string_t> query_database(borrow_t<DatabaseConnection> db_borrow, const string_t &sql)
{
    std::cout << "\n=== Host Function: query_database ===\n";

    if (!db_borrow.valid())
    {
        std::cout << "Error: Invalid database connection\n";
        return {};
    }

    const DatabaseConnection &db = db_borrow.get();
    std::cout << "Querying database " << db.connection_id << " with: " << sql << std::endl;

    auto results = db.query(sql);
    list_t<string_t> result_list;
    for (const auto &row : results)
    {
        result_list.push_back(row);
    }

    std::cout << "Query returned " << result_list.size() << " rows\n";
    return result_list;
}

// Complex function that demonstrates resource composition
tuple_t<own_t<FileHandle>, own_t<DatabaseConnection>> setup_session(
    const string_t &config_file,
    const string_t &db_connection)
{

    std::cout << "\n=== Host Function: setup_session ===\n";
    std::cout << "Setting up session with config: " << config_file
              << " and database: " << db_connection << std::endl;

    auto file = create_file(config_file);
    auto db = connect_database(db_connection);

    std::cout << "Session setup complete\n";
    return std::make_tuple(std::move(file), std::move(db));
}

// Function that works with multiple borrowed resources
result_t<string_t, string_t> process_file_with_db(
    borrow_t<FileHandle> file,
    borrow_t<DatabaseConnection> db)
{

    std::cout << "\n=== Host Function: process_file_with_db ===\n";

    if (!file.valid())
    {
        return result_t<string_t, string_t>::err("Invalid file handle");
    }

    if (!db.valid())
    {
        return result_t<string_t, string_t>::err("Invalid database connection");
    }

    // Read file content
    std::string content = file.get().read_content();

    // Use content as SQL query
    std::string sql = "SELECT * FROM data WHERE name = '" + content + "'";
    auto results = db.get().query(sql);

    std::string summary = "Processed " + content + " with " +
                          std::to_string(results.size()) + " results";

    std::cout << "Processing complete: " << summary << std::endl;
    return result_t<string_t, string_t>::ok(summary);
}

// Native symbol tables for WAMR
NativeSymbol file_resource_symbols[] = {
    host_function("create-file", create_file),
    host_function("read-file-content", read_file_content),
    host_function("close-file", close_file),
};

NativeSymbol database_resource_symbols[] = {
    host_function("connect-database", connect_database),
    host_function("query-database", query_database),
};

NativeSymbol session_resource_symbols[] = {
    host_function("setup-session", setup_session),
    host_function("process-file-with-db", process_file_with_db),
};

// Test function to demonstrate resource operations without WASM
void test_resource_operations()
{
    std::cout << "\n"
              << std::string(50, '=') << std::endl;
    std::cout << "TESTING RESOURCE OPERATIONS" << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    try
    {
        // Test 1: Create and use file resource
        std::cout << "\nTest 1: File Resource Operations\n";
        std::cout << std::string(30, '-') << std::endl;

        auto file = create_file("test_config.txt");

        // Create a borrow from the owned resource
        borrow_t<FileHandle> file_borrow(&file.rt, file.get());
        auto content = read_file_content(file_borrow);
        std::cout << "Read content: " << content << std::endl;

        // Close the file (transfers ownership)
        close_file(std::move(file));

        // Test 2: Database resource operations
        std::cout << "\nTest 2: Database Resource Operations\n";
        std::cout << std::string(30, '-') << std::endl;

        auto db = connect_database("postgresql://localhost:5432/testdb");

        // Create a borrow for querying
        borrow_t<DatabaseConnection> db_borrow(&db.rt, db.get());
        auto results = query_database(db_borrow, "SELECT * FROM users");

        std::cout << "Query results:\n";
        for (size_t i = 0; i < results.size(); ++i)
        {
            std::cout << "  [" << i << "] " << results[i] << std::endl;
        }

        // Test 3: Session setup with multiple resources
        std::cout << "\nTest 3: Session Setup (Multiple Resources)\n";
        std::cout << std::string(30, '-') << std::endl;

        auto session = setup_session("session_config.json", "mysql://localhost:3306/app");
        auto &[session_file, session_db] = session;

        // Test 4: Complex processing with borrowed resources
        std::cout << "\nTest 4: Complex Processing with Borrowed Resources\n";
        std::cout << std::string(30, '-') << std::endl;

        borrow_t<FileHandle> file_borrow2(&session_file.rt, session_file.get());
        borrow_t<DatabaseConnection> db_borrow2(&session_db.rt, session_db.get());

        auto process_result = process_file_with_db(file_borrow2, db_borrow2);

        if (process_result.is_ok())
        {
            std::cout << "Processing succeeded: " << process_result.unwrap() << std::endl;
        }
        else
        {
            std::cout << "Processing failed: " << process_result.unwrap_err() << std::endl;
        }

        std::cout << "\nAll tests completed successfully!\n";
    }
    catch (const std::exception &e)
    {
        std::cout << "Error during testing: " << e.what() << std::endl;
    }
}

// Resource table management demonstration
void test_resource_table()
{
    std::cout << "\n"
              << std::string(50, '=') << std::endl;
    std::cout << "TESTING RESOURCE TABLE OPERATIONS" << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    ResourceTable table;

    std::cout << "Initial table size: " << table.size() << std::endl;

    // Add some resources
    std::cout << "\nAdding resources to table:\n";
    uint32_t handle1 = table.add(FileHandle("file1.txt"));
    uint32_t handle2 = table.add(FileHandle("file2.txt"));
    uint32_t handle3 = table.add(DatabaseConnection("db1", 101));

    std::cout << "Handle 1: " << handle1 << " (file)" << std::endl;
    std::cout << "Handle 2: " << handle2 << " (file)" << std::endl;
    std::cout << "Handle 3: " << handle3 << " (database)" << std::endl;
    std::cout << "Table size: " << table.size() << std::endl;

    // Retrieve and use resources
    std::cout << "\nRetrieving resources:\n";
    auto file1 = table.get<FileHandle>(handle1);
    if (file1.has_value())
    {
        std::cout << "Retrieved file: " << file1->filename << std::endl;
    }

    auto db1 = table.get<DatabaseConnection>(handle3);
    if (db1.has_value())
    {
        std::cout << "Retrieved database: " << db1->connection_string << std::endl;
    }

    // Test wrong type retrieval
    auto wrong_type = table.get<DatabaseConnection>(handle1);
    if (!wrong_type.has_value())
    {
        std::cout << "Correctly rejected wrong type access\n";
    }

    // Remove a resource and test reuse
    std::cout << "\nTesting handle reuse:\n";
    table.remove(handle2);
    uint32_t handle4 = table.add(FileHandle("file4.txt"));
    std::cout << "After removing handle " << handle2 << ", new handle is: " << handle4 << std::endl;

    if (handle4 == handle2)
    {
        std::cout << "Handle reuse working correctly!\n";
    }
}

int main(int argc, char *argv[])
{
    std::cout << "WAMR Component Model C++ - Resource Examples" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "This sample demonstrates resource management in Component Model C++" << std::endl;
    std::cout << "including own/borrow semantics, destructors, and resource tables." << std::endl;

    if (argc > 1 && std::string(argv[1]) == "--table-only")
    {
        test_resource_table();
        return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "--resources-only")
    {
        test_resource_operations();
        return 0;
    }

    // Run both tests by default
    test_resource_table();
    test_resource_operations();

    std::cout << "\n"
              << std::string(60, '=') << std::endl;
    std::cout << "Resource examples completed!" << std::endl;
    std::cout << "\nUsage:" << std::endl;
    std::cout << "  " << argv[0] << "                  # Run all tests" << std::endl;
    std::cout << "  " << argv[0] << " --table-only     # Test resource table only" << std::endl;
    std::cout << "  " << argv[0] << " --resources-only # Test resource operations only" << std::endl;

    return 0;
}
