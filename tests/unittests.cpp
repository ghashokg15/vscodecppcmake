#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <filesystem>  // Requires C++17 or later.  If not available, use boost::filesystem

#include "gtest/gtest.h" // Include gtest header

namespace fs = std::filesystem;

// Forward declarations
class Database;
class Table;

// Enum for data types
enum class DataType {
    INTEGER,
    REAL,
    TEXT,
    INVALID // For error handling
};


// Helper function to convert string to DataType
DataType stringToDataType(const std::string& typeStr) {
    if (typeStr == "INTEGER") return DataType::INTEGER;
    if (typeStr == "REAL") return DataType::REAL;
    if (typeStr == "TEXT") return DataType::TEXT;
    return DataType::INVALID;
}

std::string dataTypeToString(DataType type) {
    switch (type) {
        case DataType::INTEGER: return "INTEGER";
        case DataType::REAL: return "REAL";
        case DataType::TEXT: return "TEXT";
        default: return "INVALID";
    }
}


// Helper function to check if a string is an integer
bool isInteger(const std::string& str) {
    if (str.empty() || ((!isdigit(str[0])) && (str[0] != '-') && (str[0] != '+'))) {
        return false;
    }

    char* p;
    strtol(str.c_str(), &p, 10);

    return (*p == 0);
}

// Helper function to check if a string is a double
bool isDouble(const std::string& str) {
    std::istringstream iss(str);
    double f;
    iss >> std::noskipws >> f; // noskipws considers leading whitespace invalid
    return iss.eof() && !iss.fail();
}

// Generic function to convert string to a type safely
template <typename T>
T stringToType(const std::string& str) {
    std::istringstream iss(str);
    T result;
    iss >> std::noskipws >> result;
    if (iss.fail()) {
        throw std::invalid_argument("Conversion failed: Invalid input format.");
    }
    return result;
}


class Database {
public:
    Database(const std::string& dbName) : name(dbName) {
        dbDirectory = name + ".db";
        if (!fs::exists(dbDirectory)) {
             fs::create_directory(dbDirectory);
        }
    }

    ~Database() {}

    bool createTable(const std::string& tableName, const std::vector<std::pair<std::string, std::string>>& columns) {
        if (tables.find(tableName) != tables.end()) {
            std::cerr << "Error: Table '" << tableName << "' already exists." << std::endl;
            return false;
        }

        try {
            tables[tableName] = std::make_unique<Table>(tableName, columns, dbDirectory);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error creating table '" << tableName << "': " << e.what() << std::endl;
            return false;
        }
    }


    Table* getTable(const std::string& tableName) {
        auto it = tables.find(tableName);
        if (it != tables.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    const std::string& getName() const { return name; }
    const std::string& getDirectory() const { return dbDirectory; }


private:
    std::string name;
    std::string dbDirectory; // Directory to hold table files
    std::map<std::string, std::unique_ptr<Table>> tables;
};


class Table {
public:
    Table(const std::string& tableName, const std::vector<std::pair<std::string, std::string>>& columns, const std::string& dbDirectory)
        : name(tableName), directory(dbDirectory) {
        
        for (const auto& col : columns) {
            columnNames.push_back(col.first);
            columnTypes.push_back(stringToDataType(col.second));
        }

        if (std::count(columnTypes.begin(), columnTypes.end(), DataType::INVALID) > 0) {
            throw std::invalid_argument("Invalid column type specified.");
        }

        // Determine primary key column (first column by default for simplicity)
        if (!columnNames.empty()) {
            primaryKeyColumn = columnNames[0];  // Simplification: First column is PK
        } else {
             throw std::invalid_argument("Table must have at least one column.");
        }


        filename = directory + "/" + name + ".tbl";
        loadDataFromFile();
    }

    ~Table() {
        saveDataToFile();
    }

    bool insertData(const std::vector<std::string>& rowData) {
        if (rowData.size() != columnTypes.size()) {
            std::cerr << "Error: Incorrect number of values provided for table '" << name
                      << "'. Expected " << columnTypes.size() << ", got " << rowData.size() << std::endl;
            return false;
        }


        // Data Validation
        try {
            for (size_t i = 0; i < columnTypes.size(); ++i) {
                switch (columnTypes[i]) {
                    case DataType::INTEGER:
                        if (!isInteger(rowData[i])) {
                            throw std::invalid_argument("Invalid integer value: " + rowData[i]);
                        }
                        try {
                            stringToType<int>(rowData[i]); // Check for overflow
                        } catch (const std::out_of_range& e) {
                            throw std::out_of_range("Integer overflow: " + rowData[i]);
                        }
                        break;
                    case DataType::REAL:
                        if (!isDouble(rowData[i])) {
                            throw std::invalid_argument("Invalid real value: " + rowData[i]);
                        }
                        break;
                    case DataType::TEXT:
                        // No specific validation for text
                        break;
                    default:
                        throw std::runtime_error("Unexpected data type during validation.");
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error during data validation: " << e.what() << std::endl;
            return false;  // Reject insert if validation fails
        }


        // Primary Key Check (Simple: first column)
        if (!rows.empty()) {
            std::string primaryKeyVal = rowData[0];
            for (const auto& row : rows) {
                if (row[0] == primaryKeyVal) {
                    std::cerr << "Error: Duplicate primary key '" << primaryKeyVal << "'." << std::endl;
                    return false;
                }
            }
        }


        rows.push_back(rowData);
        return true;
    }


    std::vector<std::vector<std::string>> selectData(const std::string& condition) {
        std::vector<std::vector<std::string>> results;
        for (const auto& row : rows) {
            if (evaluateCondition(row, condition)) {
                results.push_back(row);
            }
        }
        return results;
    }


    bool updateData(const std::string& condition, const std::vector<std::pair<std::string, std::string>>& updatedValues) {
        bool updated = false;
        for (auto& row : rows) {
            if (evaluateCondition(row, condition)) {
                for (const auto& update : updatedValues) {
                    std::string columnName = update.first;
                    std::string newValue = update.second;

                    // Find the column index
                    auto it = std::find(columnNames.begin(), columnNames.end(), columnName);
                    if (it == columnNames.end()) {
                        std::cerr << "Error: Column '" << columnName << "' not found in table '" << name << "'" << std::endl;
                        return false; // Or throw an exception
                    }
                    size_t columnIndex = std::distance(columnNames.begin(), it);

                    // Validate the new value against the column type
                    try {
                        switch (columnTypes[columnIndex]) {
                            case DataType::INTEGER:
                                if (!isInteger(newValue)) {
                                    throw std::invalid_argument("Invalid integer value: " + newValue);
                                }
                                try {
                                    stringToType<int>(newValue); // Check for overflow
                                } catch (const std::out_of_range& e) {
                                    throw std::out_of_range("Integer overflow: " + newValue);
                                }
                                break;
                            case DataType::REAL:
                                if (!isDouble(newValue)) {
                                    throw std::invalid_argument("Invalid real value: " + newValue);
                                }
                                break;
                            case DataType::TEXT:
                                // No specific validation for text
                                break;
                            default:
                                throw std::runtime_error("Unexpected data type during validation.");
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error during data validation for update: " << e.what() << std::endl;
                        return false;
                    }

                    row[columnIndex] = newValue;
                    updated = true; // Set updated to true if at least one row is updated
                }
            }
        }
        return updated; // Return true only if at least one row was updated
    }


    bool deleteData(const std::string& condition) {
        bool deleted = false;
        for (auto it = rows.begin(); it != rows.end();) {
            if (evaluateCondition(*it, condition)) {
                it = rows.erase(it); // erase returns the next iterator
                deleted = true;
            } else {
                ++it;
            }
        }
        return deleted; // Return true only if at least one row was deleted
    }



    const std::string& getName() const { return name; }

private:
    std::string name;
    std::string filename;
    std::string directory;
    std::vector<std::string> columnNames;
    std::vector<DataType> columnTypes;
    std::vector<std::vector<std::string>> rows; // Data stored as strings
    std::string primaryKeyColumn;


    bool evaluateCondition(const std::vector<std::string>& row, const std::string& condition) {
        if (condition.empty()) return true; // Empty condition means select all

        std::istringstream iss(condition);
        std::string colName, op, value, logicalOp;

        iss >> colName >> op >> value;

        size_t colIndex = std::distance(columnNames.begin(), std::find(columnNames.begin(), columnNames.end(), colName));
        if (colIndex == columnNames.size()) {
            std::cerr << "Error: Column '" << colName << "' not found." << std::endl;
            return false;
        }

        bool result = false;
        try {
            switch (columnTypes[colIndex]) {
                case DataType::INTEGER: {
                    int rowValue = stringToType<int>(row[colIndex]);
                    int conditionValue = stringToType<int>(value);
                    if (op == "=") result = (rowValue == conditionValue);
                    else if (op == "!=") result = (rowValue != conditionValue);
                    else if (op == ">") result = (rowValue > conditionValue);
                    else if (op == "<") result = (rowValue < conditionValue);
                    else if (op == ">=") result = (rowValue >= conditionValue);
                    else if (op == "<=") result = (rowValue <= conditionValue);
                    else {
                        std::cerr << "Error: Invalid operator '" << op << "' for INTEGER type." << std::endl;
                        return false;
                    }
                    break;
                }
                case DataType::REAL: {
                    double rowValue = stringToType<double>(row[colIndex]);
                    double conditionValue = stringToType<double>(value);
                    if (op == "=") result = (rowValue == conditionValue);
                    else if (op == "!=") result = (rowValue != conditionValue);
                    else if (op == ">") result = (rowValue > conditionValue);
                    else if (op == "<") result = (rowValue < conditionValue);
                    else if (op == ">=") result = (rowValue >= conditionValue);
                    else if (op == "<=") result = (rowValue <= conditionValue);
                    else {
                        std::cerr << "Error: Invalid operator '" << op << "' for REAL type." << std::endl;
                        return false;
                    }
                    break;
                }
                case DataType::TEXT: {
                    if (op == "=") result = (row[colIndex] == value);
                    else if (op == "!=") result = (row[colIndex] != value);
                    else {
                        std::cerr << "Error: Invalid operator '" << op << "' for TEXT type." << std::endl;
                        return false;
                    }
                    break;
                }
                default:
                    std::cerr << "Error: Unsupported data type for condition evaluation." << std::endl;
                    return false;
            }

            // Handle AND/OR for multiple conditions (simplistic, only handles one additional condition)
            if (iss >> logicalOp) {
                std::string nextColName, nextOp, nextValue;
                iss >> nextColName >> nextOp >> nextValue;
                size_t nextColIndex = std::distance(columnNames.begin(), std::find(columnNames.begin(), columnNames.end(), nextColName));
                 if (nextColIndex == columnNames.size()) {
                    std::cerr << "Error: Column '" << nextColName << "' not found." << std::endl;
                    return false;
                }

                bool nextResult = false;
                 switch (columnTypes[nextColIndex]) {
                    case DataType::INTEGER: {
                        int rowValue = stringToType<int>(row[nextColIndex]);
                        int conditionValue = stringToType<int>(nextValue);
                        if (nextOp == "=") nextResult = (rowValue == conditionValue);
                        else if (nextOp == "!=") nextResult = (rowValue != conditionValue);
                        else if (nextOp == ">") nextResult = (rowValue > conditionValue);
                        else if (nextOp == "<") nextResult = (rowValue < conditionValue);
                        else if (nextOp == ">=") nextResult = (rowValue >= conditionValue);
                        else if (nextOp == "<=") nextResult = (rowValue <= conditionValue);
                        else {
                            std::cerr << "Error: Invalid operator '" << nextOp << "' for INTEGER type." << std::endl;
                            return false;
                        }
                        break;
                    }
                    case DataType::REAL: {
                        double rowValue = stringToType<double>(row[nextColIndex]);
                        double conditionValue = stringToType<double>(nextValue);
                        if (nextOp == "=") nextResult = (rowValue == conditionValue);
                        else if (nextOp == "!=") nextResult = (rowValue != conditionValue);
                        else if (nextOp == ">") nextResult = (rowValue > conditionValue);
                        else if (nextOp == "<") nextResult = (rowValue < conditionValue);
                        else if (nextOp == ">=") nextResult = (rowValue >= conditionValue);
                        else if (nextOp == "<=") nextResult = (rowValue <= conditionValue);
                        else {
                            std::cerr << "Error: Invalid operator '" << nextOp << "' for REAL type." << std::endl;
                            return false;
                        }
                        break;
                    }
                    case DataType::TEXT: {
                        if (nextOp == "=") nextResult = (row[nextColIndex] == nextValue);
                        else if (nextOp == "!=") nextResult = (row[nextColIndex] != nextValue);
                        else {
                            std::cerr << "Error: Invalid operator '" << nextOp << "' for TEXT type." << std::endl;
                            return false;
                        }
                        break;
                    }
                    default:
                        std::cerr << "Error: Unsupported data type for condition evaluation." << std::endl;
                        return false;
                }


                if (logicalOp == "AND") {
                    result = result && nextResult;
                } else if (logicalOp == "OR") {
                    result = result || nextResult;
                } else {
                    std::cerr << "Error: Invalid logical operator '" << logicalOp << "'." << std::endl;
                    return false;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error during condition evaluation: " << e.what() << std::endl;
            return false;
        }

        return result;
    }




    void saveDataToFile() {
        std::ofstream outfile(filename);
        if (!outfile.is_open()) {
            std::cerr << "Error: Could not open file '" << filename << "' for writing." << std::endl;
            return; // Or throw an exception
        }

        // Write column names and types as the first line (metadata)
        for (size_t i = 0; i < columnNames.size(); ++i) {
            outfile << columnNames[i] << ":" << dataTypeToString(columnTypes[i]);
            if (i < columnNames.size() - 1) {
                outfile << ",";
            }
        }
        outfile << std::endl;


        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                outfile << row[i];
                if (i < row.size() - 1) {
                    outfile << ",";
                }
            }
            outfile << std::endl;
        }

        outfile.close();
    }


    void loadDataFromFile() {
        std::ifstream infile(filename);
        if (!infile.is_open()) {
             // If the file doesn't exist, it's not an error; the table is just empty.
            std::cout << "Table file not found.  Table is empty: " << filename << std::endl;
            return;
        }

        std::string line;

        // Read column names and types from the first line
        if (std::getline(infile, line)) {
            std::stringstream ss(line);
            std::string columnDef;
            size_t i = 0;
            while (std::getline(ss, columnDef, ',')) {
                size_t pos = columnDef.find(':');
                if (pos != std::string::npos) {
                    std::string colName = columnDef.substr(0, pos);
                    std::string typeStr = columnDef.substr(pos + 1);

                    if(i >= columnNames.size()){
                        std::cerr << "ERROR: More column definitions in file than in table definition" << std::endl;
                        return;
                    }

                    if (colName != columnNames[i] || dataTypeToString(columnTypes[i]) != typeStr) {
                       std::cerr << "ERROR: Column definition mismatch in file.  Expected " << columnNames[i] << ":" << dataTypeToString(columnTypes[i])
                                << " got " << colName << ":" << typeStr << std::endl;
                        return;
                    }
                    i++;

                } else {
                    std::cerr << "Error: Invalid column definition in file." << std::endl;
                    return;
                }
            }
        }


        while (std::getline(infile, line)) {
            std::stringstream ss(line);
            std::string cell;
            std::vector<std::string> row;
            while (std::getline(ss, cell, ',')) {
                row.push_back(cell);
            }
            if (row.size() == columnTypes.size()) {
                rows.push_back(row);
            } else {
                std::cerr << "Error: Incorrect number of columns in row: " << row.size() << ", expected " << columnTypes.size() << std::endl;
            }
        }

        infile.close();
    }
};



bool createDatabase(const std::string& dbName) {
    try {
        Database db(dbName); // The constructor handles the creation.
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating database: " << e.what() << std::endl;
        return false;
    }
}

bool createTable(Database& db, const std::string& tableName, const std::vector<std::pair<std::string, std::string>>& columns) {
     return db.createTable(tableName, columns);
}


std::vector<std::vector<std::string>> selectData(Database& db, const std::string& tableName, const std::string& condition) {
    Table* table = db.getTable(tableName);
    if (!table) {
        std::cerr << "Error: Table '" << tableName << "' not found." << std::endl;
        return {}; // Return an empty vector
    }
    return table->selectData(condition);
}

bool insertData(Database& db, const std::string& tableName, const std::vector<std::string>& rowData) {
    Table* table = db.getTable(tableName);
    if (!table) {
        std::cerr << "Error: Table '" << tableName << "' not found." << std::endl;
        return false;
    }
    return table->insertData(rowData);
}

bool updateData(Database& db, const std::string& tableName, const std::string& condition, const std::vector<std::pair<std::string, std::string>>& updatedValues) {
    Table* table = db.getTable(tableName);
    if (!table) {
        std::cerr << "Error: Table '" << tableName << "' not found." << std::endl;
        return false;
    }
    return table->updateData(condition, updatedValues);
}


bool deleteData(Database& db, const std::string& tableName, const std::string& condition) {
    Table* table = db.getTable(tableName);
    if (!table) {
        std::cerr << "Error: Table '" << tableName << "' not found." << std::endl;
        return false;
    }
    return table->deleteData(condition);
}


//--------------------------------------------------------------------------------------------------
// Unit Tests
//--------------------------------------------------------------------------------------------------

TEST(DatabaseTest, CreateDatabase) {
    std::string dbName = "test_db";
    ASSERT_TRUE(createDatabase(dbName));
    ASSERT_TRUE(fs::exists(dbName + ".db")); // Verify directory created

    // Clean up after the test
    fs::remove_all(dbName + ".db");
}


TEST(DatabaseTest, CreateTable) {
    std::string dbName = "test_db";
    Database db(dbName);

    std::vector<std::pair<std::string, std::string>> columns = {
        {"id", "INTEGER"},
        {"name", "TEXT"},
        {"age", "INTEGER"}
    };

    ASSERT_TRUE(createTable(db, "users", columns));
    ASSERT_NE(db.getTable("users"), nullptr);

    // Clean up after the test
    fs::remove_all(dbName + ".db");
}

TEST(DatabaseTest, InsertAndSelectData) {
    std::string dbName = "test_db";
    Database db(dbName);

    std::vector<std::pair<std::string, std::string>> columns = {
        {"id", "INTEGER"},
        {"name", "TEXT"},
        {"age", "INTEGER"}
    };
    ASSERT_TRUE(createTable(db, "users", columns));

    ASSERT_TRUE(insertData(db, "users", {"1", "Alice", "30"}));
    ASSERT_TRUE(insertData(db, "users", {"2", "Bob", "25"}));

    std::vector<std::vector<std::string>> results = selectData(db, "users", "");
    ASSERT_EQ(results.size(), 2);
    ASSERT_EQ(results[0][1], "Alice");

    results = selectData(db, "users", "age > 25");
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results[0][1], "Alice");


     // Test with no matching data
    results = selectData(db, "users", "age > 100");
    ASSERT_EQ(results.size(), 0);

    // Clean up after the test
    fs::remove_all(dbName + ".db");
}

TEST(DatabaseTest, InsertInvalidData) {
    std::string dbName = "test_db";
    Database db(dbName);

    std::vector<std::pair<std::string, std::string>> columns = {
        {"id", "INTEGER"},
        {"name", "TEXT"},
        {"age", "INTEGER"}
    };
    ASSERT_TRUE(createTable(db, "users", columns));

    // Try to insert invalid data (non-integer for age)
    ASSERT_FALSE(insertData(db, "users", {"3", "Charlie", "abc"}));

    // Try to insert too few columns
    ASSERT_FALSE(insertData(db, "users", {"4", "David"}));

    // Try to insert duplicate PK
    ASSERT_TRUE(insertData(db, "users", {"5", "Eve", "40"}));
    ASSERT_FALSE(insertData(db, "users", {"5", "Frank", "50"}));



    // Clean up after the test
    fs::remove_all(dbName + ".db");
}


TEST(DatabaseTest, UpdateData) {
    std::string dbName = "test_db";
    Database db(dbName);

    std::vector<std::pair<std::string, std::string>> columns = {
        {"id", "INTEGER"},
        {"name", "TEXT"},
        {"age", "INTEGER"}
    };
    ASSERT_TRUE(createTable(db, "users", columns));

    ASSERT_TRUE(insertData(db, "users", {"1", "Alice", "30"}));
    ASSERT_TRUE(insertData(db, "users", {"2", "Bob", "25"}));

    // Update Alice's age
    std::vector<std::pair<std::string, std::string>> updatedValues = {{"age", "31"}};
    ASSERT_TRUE(updateData(db, "users", "id = 1", updatedValues));

    // Verify the update
    std::vector<std::vector<std::string>> results = selectData(db, "users", "id = 1");
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results[0][2], "31");

     // Update non existent record
    updatedValues = {{"age", "40"}};
    ASSERT_FALSE(updateData(db, "users", "id = 3", updatedValues));


    // Clean up after the test
    fs::remove_all(dbName + ".db");
}

TEST(DatabaseTest, DeleteData) {
    std::string dbName = "test_db";
    Database db(dbName);

    std::vector<std::pair<std::string, std::string>> columns = {
        {"id", "INTEGER"},
        {"name", "TEXT"},
        {"age", "INTEGER"}
    };
    ASSERT_TRUE(createTable(db, "users", columns));

    ASSERT_TRUE(insertData(db, "users", {"1", "Alice", "30"}));
    ASSERT_TRUE(insertData(db, "users", {"2", "Bob", "25"}));

    // Delete Bob
    ASSERT_TRUE(deleteData(db, "users", "id = 2"));

    // Verify deletion
    std::vector<std::vector<std::string>> results = selectData(db, "users", "");
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results[0][1], "Alice");

    // Delete non-existent record
     ASSERT_FALSE(deleteData(db, "users", "id = 2"));


    // Clean up after the test
    fs::remove_all(dbName + ".db");
}

TEST(DatabaseTest, MultipleConditions) {
    std::string dbName = "test_db";
    Database db(dbName);

    std::vector<std::pair<std::string, std::string>> columns = {
        {"id", "INTEGER"},
        {"name", "TEXT"},
        {"age", "INTEGER"},
        {"city", "TEXT"}
    };
    ASSERT_TRUE(createTable(db, "users", columns));

    ASSERT_TRUE(insertData(db, "users", {"1", "Alice", "30", "New York"}));
    ASSERT_TRUE(insertData(db, "users", {"2", "Bob", "25", "Los Angeles"}));
    ASSERT_TRUE(insertData(db, "users", {"3", "Charlie", "30", "Chicago"}));

    // Test AND condition
    std::vector<std::vector<std::string>> results = selectData(db, "users", "age = 30 AND city = New York");
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results[0][1], "Alice");

    // Test OR condition
    results = selectData(db, "users", "age = 25 OR city = Chicago");
    ASSERT_EQ(results.size(), 2);

    bool foundBob = false;
    bool foundCharlie = false;
    for(const auto& row : results){
        if(row[1] == "Bob") foundBob = true;
        if(row[1] == "Charlie") foundCharlie = true;
    }

    ASSERT_TRUE(foundBob);
    ASSERT_TRUE(foundCharlie);



    // Clean up after the test
    fs::remove_all(dbName + ".db");
}


TEST(DatabaseTest, IntegerOverflow) {
    std::string dbName = "test_db";
    Database db(dbName);

    std::vector<std::pair<std::string, std::string>> columns = {
        {"id", "INTEGER"}
    };
    ASSERT_TRUE(createTable(db, "numbers", columns));

    // Attempt to insert a value that causes integer overflow
    std::string overflowValue = std::to_string(std::numeric_limits<long long>::max());  // Use long long to construct large number

    ASSERT_FALSE(insertData(db, "numbers", {overflowValue}));

    // Clean up after the test
    fs::remove_all(dbName + ".db");
}

TEST(DatabaseTest, DataTypeMismatchUpdate) {
    std::string dbName = "test_db";
    Database db(dbName);

    std::vector<std::pair<std::string, std::string>> columns = {
        {"id", "INTEGER"},
        {"value", "INTEGER"}
    };
    ASSERT_TRUE(createTable(db, "data", columns));
    ASSERT_TRUE(insertData(db, "data", {"1", "10"}));

    // Attempt to update with a string value in an INTEGER column
    std::vector<std::pair<std::string, std::string>> updatedValues = {{"value", "abc"}};
    ASSERT_FALSE(updateData(db, "data", "id = 1", updatedValues));

    // Verify that the value was not updated
    std::vector<std::vector<std::string>> results = selectData(db, "data", "id = 1");
    ASSERT_EQ(results[0][1], "10");

    // Clean up after the test
    fs::remove_all(dbName + ".db");
}



int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}