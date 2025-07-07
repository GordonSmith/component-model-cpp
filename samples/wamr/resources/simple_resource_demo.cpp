// Simple Resource Examples for Component Model C++
// This demonstrates basic resource types and operations

#include <cmcpp.hpp>
#include "../../../test/host-util.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <string>

using namespace cmcpp;

// Example 1: Simple Resource Type
class FileHandle
{
public:
    std::string filename;
    bool is_open;

    FileHandle() : filename(""), is_open(false) {} // Default constructor for stub implementations
    FileHandle(const std::string &name) : filename(name), is_open(true) {}

    void close()
    {
        is_open = false;
    }

    std::string read_content() const
    {
        return is_open ? ("Content of " + filename) : "";
    }
};

// Resource destructor function
void file_destructor(const FileHandle &handle)
{
    std::cout << "Destructor called for: " << handle.filename << std::endl;
}

// Example 2: Database Connection Resource
class DatabaseConnection
{
public:
    std::string connection_string;
    uint32_t connection_id;

    DatabaseConnection() : connection_string(""), connection_id(0) {} // Default constructor
    DatabaseConnection(const std::string &conn_str, uint32_t id)
        : connection_string(conn_str), connection_id(id) {}

    std::vector<std::string> query(const std::string &sql) const
    {
        return {"row1", "row2", "row3"};
    }
};

void db_destructor(const DatabaseConnection &conn)
{
    std::cout << "DB Destructor called for connection ID: " << conn.connection_id << std::endl;
}

void test_basic_resources()
{
    std::cout << "\n=== Testing Basic Resource Operations ===\n"
              << std::endl;

    // Create resource types with destructors
    ResourceType<FileHandle> file_rt(file_destructor, true);
    ResourceType<DatabaseConnection> db_rt(db_destructor, true);

    std::cout << "1. Creating owned file resource..." << std::endl;
    own_t<FileHandle> owned_file(&file_rt, FileHandle("test.txt"));
    std::cout << "   Created file: " << owned_file.get().filename << std::endl;

    std::cout << "\n2. Creating borrowed reference..." << std::endl;
    borrow_t<FileHandle> borrowed_file(&file_rt, owned_file.get());
    std::cout << "   Borrowed file: " << borrowed_file.get().filename << std::endl;
    std::cout << "   Reading content: " << borrowed_file.get().read_content() << std::endl;

    std::cout << "\n3. Testing move semantics..." << std::endl;
    own_t<FileHandle> moved_file = std::move(owned_file);
    std::cout << "   Moved file: " << moved_file.get().filename << std::endl;
    std::cout << "   Original valid: " << (owned_file.valid() ? "true" : "false") << std::endl;
    std::cout << "   Moved valid: " << (moved_file.valid() ? "true" : "false") << std::endl;

    std::cout << "\n4. Creating database resource..." << std::endl;
    own_t<DatabaseConnection> owned_db(&db_rt, DatabaseConnection("localhost:5432", 42));
    std::cout << "   Created DB connection: " << owned_db.get().connection_string << std::endl;

    std::cout << "\n5. Testing database query..." << std::endl;
    borrow_t<DatabaseConnection> borrowed_db(&db_rt, owned_db.get());
    auto results = borrowed_db.get().query("SELECT * FROM users");
    std::cout << "   Query returned " << results.size() << " rows" << std::endl;

    std::cout << "\n=== Resources will be destroyed when going out of scope ===\n"
              << std::endl;
}

void test_resource_table()
{
    std::cout << "\n=== Testing Resource Table ===\n"
              << std::endl;

    ResourceTable table;
    std::cout << "Initial table size: " << table.size() << std::endl;

    // Add some resources
    uint32_t handle1 = table.add(FileHandle("file1.txt"));
    uint32_t handle2 = table.add(FileHandle("file2.txt"));
    uint32_t handle3 = table.add(DatabaseConnection("db1", 101));

    std::cout << "Added 3 resources, table size: " << table.size() << std::endl;
    std::cout << "Handle 1: " << handle1 << std::endl;
    std::cout << "Handle 2: " << handle2 << std::endl;
    std::cout << "Handle 3: " << handle3 << std::endl;

    // Retrieve resources
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

    // Test wrong type access
    auto wrong_type = table.get<DatabaseConnection>(handle1);
    std::cout << "Wrong type access result: " << (wrong_type.has_value() ? "SUCCESS" : "FAILED (Expected)") << std::endl;

    // Test handle reuse
    table.remove(handle2);
    uint32_t handle4 = table.add(FileHandle("file4.txt"));
    std::cout << "After removing handle " << handle2 << ", new handle is: " << handle4 << std::endl;
    std::cout << "Handle reuse working: " << (handle4 == handle2 ? "YES" : "NO") << std::endl;
}

void test_canon_functions()
{
    std::cout << "\n=== Testing Canon Resource Functions ===\n"
              << std::endl;

    ResourceType<FileHandle> rt(file_destructor, true);

    std::cout << "1. Testing canon_resource_new..." << std::endl;
    uint32_t handle1 = resource::canon_resource_new(&rt, 42);
    uint32_t handle2 = resource::canon_resource_new(&rt, 43);
    std::cout << "   Created handles: " << handle1 << ", " << handle2 << std::endl;

    std::cout << "2. Testing canon_resource_rep..." << std::endl;
    uint32_t rep1 = resource::canon_resource_rep(&rt, handle1);
    std::cout << "   Handle " << handle1 << " has representation: " << rep1 << std::endl;

    std::cout << "3. Testing canon_resource_drop..." << std::endl;
    std::cout << "   Dropping handle " << handle1 << "..." << std::endl;
    // Note: In our stub implementation, canon_resource_drop tries to call dtor with handle
    // This would need proper implementation in a real system
    // resource::canon_resource_drop(&rt, handle1, true);
    std::cout << "   Drop function available (skipped due to stub implementation)" << std::endl;
}

void test_lift_lower_operations()
{
    std::cout << "\n=== Testing Lift/Lower Operations ===\n"
              << std::endl;

    // Create a simple heap for testing
    Heap heap(1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    ResourceType<FileHandle> rt;

    std::cout << "1. Testing owned resource lower/lift..." << std::endl;
    own_t<FileHandle> owned(&rt, FileHandle("test_file.txt"));

    // Lower to flat values
    auto flat_vals = resource::lower_flat(*cx, owned);
    std::cout << "   Lowered to " << flat_vals.size() << " flat values" << std::endl;

    // Lift back (note: this creates a new resource with null rt in our stub)
    CoreValueIter vi(flat_vals);
    auto lifted = resource::lift_flat<own_t<FileHandle>>(*cx, vi);
    std::cout << "   Lifted back (stub implementation)" << std::endl;

    std::cout << "2. Testing borrowed resource lower/lift..." << std::endl;
    borrow_t<FileHandle> borrowed(&rt, owned.get());

    auto flat_vals_borrow = resource::lower_flat(*cx, borrowed);
    std::cout << "   Lowered borrowed to " << flat_vals_borrow.size() << " flat values" << std::endl;

    CoreValueIter vi_borrow(flat_vals_borrow);
    auto lifted_borrow = resource::lift_flat<borrow_t<FileHandle>>(*cx, vi_borrow);
    std::cout << "   Lifted borrowed back (stub implementation)" << std::endl;

    std::cout << "3. Testing store/load operations..." << std::endl;
    uint32_t ptr = 100;
    resource::store(*cx, owned, ptr);
    std::cout << "   Stored owned resource at pointer " << ptr << std::endl;

    auto loaded = resource::load<own_t<FileHandle>>(*cx, ptr);
    std::cout << "   Loaded resource from pointer (stub implementation)" << std::endl;
}

int main(int argc, char *argv[])
{
    std::cout << "Component Model C++ - Resource Examples" << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << "This sample demonstrates resource management including:" << std::endl;
    std::cout << "- Own vs Borrow semantics" << std::endl;
    std::cout << "- Resource destructors" << std::endl;
    std::cout << "- Resource tables" << std::endl;
    std::cout << "- Canon resource functions" << std::endl;
    std::cout << "- Lift/Lower operations" << std::endl;

    try
    {
        if (argc > 1)
        {
            std::string arg = argv[1];
            if (arg == "--basic")
            {
                test_basic_resources();
            }
            else if (arg == "--table")
            {
                test_resource_table();
            }
            else if (arg == "--canon")
            {
                test_canon_functions();
            }
            else if (arg == "--lift-lower")
            {
                test_lift_lower_operations();
            }
            else
            {
                std::cout << "\nUnknown option: " << arg << std::endl;
                std::cout << "Available options: --basic, --table, --canon, --lift-lower" << std::endl;
                return 1;
            }
        }
        else
        {
            // Run all tests
            test_basic_resources();
            test_resource_table();
            test_canon_functions();
            test_lift_lower_operations();
        }

        std::cout << "\n=======================================" << std::endl;
        std::cout << "All resource examples completed successfully!" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
