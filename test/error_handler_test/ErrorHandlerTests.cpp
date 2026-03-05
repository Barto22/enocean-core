#include <gtest/gtest.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include <iostream>
#include <logging/Logger.hpp>
#include <string>
#include <string_view>

#include "error_handler/ErrorHandler.hpp"

// ============================================================================
// Test Error Enums
// ============================================================================

enum class FileError : std::uint8_t {
    NotFound,
    PermissionDenied,
    AlreadyExists,
    IOError,
    InvalidFormat
};

enum class NetworkError : std::uint8_t {
    ConnectionTimeout,
    DNSResolutionFailed,
    HostUnreachable,
    InvalidResponse
};

enum class DatabaseError : std::uint8_t {
    ConnectionFailed,
    QuerySyntaxError,
    ConstraintViolation,
    Deadlock
};

// ============================================================================
// ErrorTraits Specializations
// ============================================================================

/**
 * @brief ErrorTraits specialization for FileError test enum.
 */
template <>
struct ErrorTraits<FileError> {
    /**
     * @brief Returns human-readable name for FileError.
     * @param e The FileError value
     * @return String view with error description
     */
    static constexpr std::string_view name(FileError e) {
        switch (e) {
            case FileError::NotFound:
                return "File Not Found";
            case FileError::PermissionDenied:
                return "Permission Denied";
            case FileError::AlreadyExists:
                return "File Already Exists";
            case FileError::IOError:
                return "IO Error";
            case FileError::InvalidFormat:
                return "Invalid File Format";
            default:
                return "Unknown File Error";
        }
    }
};

/**
 * @brief ErrorTraits specialization for NetworkError test enum.
 */
template <>
struct ErrorTraits<NetworkError> {
    /**
     * @brief Returns human-readable name for NetworkError.
     * @param e The NetworkError value
     * @return String view with error description
     */
    static constexpr std::string_view name(NetworkError e) {
        switch (e) {
            case NetworkError::ConnectionTimeout:
                return "Connection Timeout";
            case NetworkError::DNSResolutionFailed:
                return "DNS Resolution Failed";
            case NetworkError::HostUnreachable:
                return "Host Unreachable";
            case NetworkError::InvalidResponse:
                return "Invalid Response";
            default:
                return "Unknown Network Error";
        }
    }
};

/**
 * @brief ErrorTraits specialization for DatabaseError test enum.
 */
template <>
struct ErrorTraits<DatabaseError> {
    /**
     * @brief Returns human-readable name for DatabaseError.
     * @param e The DatabaseError value
     * @return String view with error description
     */
    static constexpr std::string_view name(DatabaseError e) {
        switch (e) {
            case DatabaseError::ConnectionFailed:
                return "Database Connection Failed";
            case DatabaseError::QuerySyntaxError:
                return "Query Syntax Error";
            case DatabaseError::ConstraintViolation:
                return "Constraint Violation";
            case DatabaseError::Deadlock:
                return "Deadlock Detected";
            default:
                return "Unknown Database Error";
        }
    }
};

// ============================================================================
// Logger Setup for Tests
// ============================================================================

namespace {
// Test fixture base class with plog initialisation
class ErrorHandlerTestFixture : public ::testing::Test {
   protected:
    static void SetUpTestSuite() {
        static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
        plog::init(plog::debug, &consoleAppender);
        std::cout << "\n=== Logger initialized for ErrorHandler tests ===\n";
    }

    static void TearDownTestSuite() {
        std::cout << "\n=== ErrorHandler tests completed ===\n";
    }
};
}  // namespace

// ============================================================================
// Basic Construction and Value Access Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, ConstructWithValue) {
    ErrorHandler<int, FileError> result(42);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_EQ(result.value(), 42);
}

TEST_F(ErrorHandlerTestFixture, ConstructWithValueString) {
    ErrorHandler<std::string, FileError> result(std::string("success"));

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "success");
}

TEST_F(ErrorHandlerTestFixture, ConstructWithError) {
    ErrorHandler<int, FileError> result(FileError::NotFound,
                                        "Configuration file missing");

    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_EQ(result.error(), FileError::NotFound);
}

TEST_F(ErrorHandlerTestFixture, ConstructWithErrorCustomMessage) {
    ErrorHandler<std::string, NetworkError> result(
        NetworkError::ConnectionTimeout,
        "Failed to connect to api.example.com after 30 seconds");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), NetworkError::ConnectionTimeout);
}

TEST_F(ErrorHandlerTestFixture, ConstructWithUnexpected) {
    std::unexpected<DatabaseError> unex(DatabaseError::Deadlock);
    ErrorHandler<int, DatabaseError> result(unex);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DatabaseError::Deadlock);
}

// ============================================================================
// Value Access Tests (including error conditions)
// ============================================================================

TEST_F(ErrorHandlerTestFixture, ValueAccessOnSuccess) {
    ErrorHandler<int, FileError> result(100);

    EXPECT_NO_THROW({
        int& val = result.value();
        EXPECT_EQ(val, 100);
    });
}

TEST_F(ErrorHandlerTestFixture, ValueAccessConst) {
    const ErrorHandler<int, FileError> result(200);

    EXPECT_NO_THROW({
        const int& val = result.value();
        EXPECT_EQ(val, 200);
    });
}

TEST_F(ErrorHandlerTestFixture, ValueAccessOnErrorThrows) {
    ErrorHandler<int, FileError> result(FileError::PermissionDenied);

    EXPECT_THROW(
        { [[maybe_unused]] int val{result.value()}; },
        std::bad_expected_access<FileError>);
}

TEST_F(ErrorHandlerTestFixture, ValueModification) {
    ErrorHandler<int, FileError> result(50);

    result.value() = 75;
    EXPECT_EQ(result.value(), 75);
}

// ============================================================================
// value_or Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, ValueOrWithSuccess) {
    ErrorHandler<int, FileError> result(42);

    int val{result.value_or(999)};
    EXPECT_EQ(val, 42);
}

TEST_F(ErrorHandlerTestFixture, ValueOrWithError) {
    ErrorHandler<int, FileError> result(FileError::NotFound);

    int val{result.value_or(999)};
    EXPECT_EQ(val, 999);
}

TEST_F(ErrorHandlerTestFixture, ValueOrWithString) {
    ErrorHandler<std::string, FileError> result(FileError::IOError);

    std::string val{result.value_or(std::string("default_value"))};
    EXPECT_EQ(val, "default_value");
}

// ============================================================================
// Monadic Operations: and_then Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, AndThenWithSuccess) {
    ErrorHandler<int, FileError> result(10);

    auto doubled{result.and_then([](int val) -> ErrorHandler<int, FileError> {
        return ErrorHandler<int, FileError>(val * 2);
    })};

    EXPECT_TRUE(doubled.has_value());
    EXPECT_EQ(doubled.value(), 20);
}

TEST_F(ErrorHandlerTestFixture, AndThenWithError) {
    ErrorHandler<int, FileError> result(FileError::NotFound);

    auto doubled{result.and_then([](int val) -> ErrorHandler<int, FileError> {
        return ErrorHandler<int, FileError>(val * 2);
    })};

    EXPECT_FALSE(doubled.has_value());
    EXPECT_EQ(doubled.error(), FileError::NotFound);
}

TEST_F(ErrorHandlerTestFixture, AndThenChaining) {
    ErrorHandler<int, FileError> result(5);

    auto result_val{result
                        .and_then([](int val) -> ErrorHandler<int, FileError> {
                            return ErrorHandler<int, FileError>(val +
                                                                10);  // 15
                        })
                        .and_then([](int val) -> ErrorHandler<int, FileError> {
                            return ErrorHandler<int, FileError>(val * 2);  // 30
                        })
                        .and_then([](int val) -> ErrorHandler<int, FileError> {
                            return ErrorHandler<int, FileError>(val - 5);  // 25
                        })};

    EXPECT_TRUE(result_val.has_value());
    EXPECT_EQ(result_val.value(), 25);
}

TEST_F(ErrorHandlerTestFixture, AndThenPropagatesError) {
    ErrorHandler<int, FileError> result(10);

    auto result_val{result
                        .and_then([](int val) -> ErrorHandler<int, FileError> {
                            return ErrorHandler<int, FileError>(val + 5);  // 15
                        })
                        .and_then([](int val) -> ErrorHandler<int, FileError> {
                            (void)val;  // Unused parameter
                            // Introduce error in the middle
                            return ErrorHandler<int, FileError>(
                                FileError::PermissionDenied, "Access denied");
                        })
                        .and_then([](int val) -> ErrorHandler<int, FileError> {
                            (void)val;  // Unused parameter
                            // This should not execute
                            return ErrorHandler<int, FileError>(val * 100);
                        })};

    EXPECT_FALSE(result_val.has_value());
    EXPECT_EQ(result_val.error(), FileError::PermissionDenied);
}

TEST_F(ErrorHandlerTestFixture, AndThenTypeTransformation) {
    ErrorHandler<int, FileError> result(42);

    auto str_result{
        result.and_then([](int val) -> ErrorHandler<std::string, FileError> {
            return ErrorHandler<std::string, FileError>(std::to_string(val));
        })};

    EXPECT_TRUE(str_result.has_value());
    EXPECT_EQ(str_result.value(), "42");
}

// ============================================================================
// Monadic Operations: transform Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, TransformWithSuccess) {
    ErrorHandler<int, FileError> result(10);

    auto doubled{result.transform([](int val) { return val * 2; })};

    EXPECT_TRUE(doubled.has_value());
    EXPECT_EQ(doubled.value(), 20);
}

TEST_F(ErrorHandlerTestFixture, TransformWithError) {
    ErrorHandler<int, FileError> result(FileError::IOError);

    auto doubled{result.transform([](int val) { return val * 2; })};

    EXPECT_FALSE(doubled.has_value());
    EXPECT_EQ(doubled.error(), FileError::IOError);
}

TEST_F(ErrorHandlerTestFixture, TransformTypeConversion) {
    ErrorHandler<int, FileError> result(123);

    auto str_result{
        result.transform([](int val) { return std::to_string(val); })};

    EXPECT_TRUE(str_result.has_value());
    EXPECT_EQ(str_result.value(), "123");
}

TEST_F(ErrorHandlerTestFixture, TransformChaining) {
    ErrorHandler<int, FileError> result(5);

    auto result_val{result
                        .transform([](int val) { return val + 10; })   // 15
                        .transform([](int val) { return val * 2; })    // 30
                        .transform([](int val) { return val - 5; })};  // 25

    EXPECT_TRUE(result_val.has_value());
    EXPECT_EQ(result_val.value(), 25);
}

TEST_F(ErrorHandlerTestFixture, TransformComplexType) {
    struct Point {
        int x;
        int y;
    };

    ErrorHandler<Point, FileError> result(Point{3, 4});

    auto magnitude{result.transform([](const Point& p) {
        return p.x * p.x + p.y * p.y;  // 25
    })};

    EXPECT_TRUE(magnitude.has_value());
    EXPECT_EQ(magnitude.value(), 25);
}

// ============================================================================
// Mixing and_then and transform
// ============================================================================

TEST_F(ErrorHandlerTestFixture, CombineAndThenAndTransform) {
    ErrorHandler<int, FileError> result(10);

    auto result_val{
        result
            .transform(
                [](int val) { return val * 2; })  // 20, returns expected<int>
            .and_then([](int val) -> ErrorHandler<int, FileError> {
                if (val > 15) {
                    return ErrorHandler<int, FileError>(val + 10);  // 30
                }
                return ErrorHandler<int, FileError>(FileError::InvalidFormat);
            })
            .transform([](int val) { return val / 2; })};  // 15

    EXPECT_TRUE(result_val.has_value());
    EXPECT_EQ(result_val.value(), 15);
}

// ============================================================================
// Error Types and Traits Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, MultipleErrorTypes) {
    ErrorHandler<int, FileError> file_result(FileError::NotFound,
                                             "config.ini not found");
    ErrorHandler<std::string, NetworkError> net_result(
        NetworkError::HostUnreachable, "Cannot reach server");
    ErrorHandler<double, DatabaseError> db_result(DatabaseError::Deadlock,
                                                  "Transaction deadlock");

    EXPECT_FALSE(file_result.has_value());
    EXPECT_FALSE(net_result.has_value());
    EXPECT_FALSE(db_result.has_value());

    EXPECT_EQ(file_result.error(), FileError::NotFound);
    EXPECT_EQ(net_result.error(), NetworkError::HostUnreachable);
    EXPECT_EQ(db_result.error(), DatabaseError::Deadlock);
}

TEST_F(ErrorHandlerTestFixture, AllFileErrors) {
    std::vector<FileError> errors{
        FileError::NotFound, FileError::PermissionDenied,
        FileError::AlreadyExists, FileError::IOError, FileError::InvalidFormat};

    for (const auto& err : errors) {
        ErrorHandler<int, FileError> result(err, "Test error");
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), err);
        EXPECT_FALSE(ErrorTraits<FileError>::name(err).empty());
    }
}

TEST_F(ErrorHandlerTestFixture, AllNetworkErrors) {
    std::vector<NetworkError> errors{
        NetworkError::ConnectionTimeout, NetworkError::DNSResolutionFailed,
        NetworkError::HostUnreachable, NetworkError::InvalidResponse};

    for (const auto& err : errors) {
        ErrorHandler<std::string, NetworkError> result(err, "Network test");
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), err);
        EXPECT_FALSE(ErrorTraits<NetworkError>::name(err).empty());
    }
}

TEST_F(ErrorHandlerTestFixture, AllDatabaseErrors) {
    std::vector<DatabaseError> errors{
        DatabaseError::ConnectionFailed, DatabaseError::QuerySyntaxError,
        DatabaseError::ConstraintViolation, DatabaseError::Deadlock};

    for (const auto& err : errors) {
        ErrorHandler<bool, DatabaseError> result(err, "DB test");
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), err);
        EXPECT_FALSE(ErrorTraits<DatabaseError>::name(err).empty());
    }
}

// ============================================================================
// Complex Type Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, VectorTypeSuccess) {
    std::vector<int> data{1, 2, 3, 4, 5};
    ErrorHandler<std::vector<int>, FileError> result(data);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 5);
    EXPECT_EQ(result.value()[0], 1);
    EXPECT_EQ(result.value()[4], 5);
}

TEST_F(ErrorHandlerTestFixture, CustomStructSuccess) {
    struct Configuration {
        std::string host;
        int port;
        bool use_ssl;
    };

    Configuration config{"localhost", 8080, true};
    ErrorHandler<Configuration, NetworkError> result(config);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().host, "localhost");
    EXPECT_EQ(result.value().port, 8080);
    EXPECT_TRUE(result.value().use_ssl);
}

TEST_F(ErrorHandlerTestFixture, PointerType) {
    int value{42};
    ErrorHandler<int*, FileError> result(&value);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result.value(), 42);
}

// ============================================================================
// Edge Cases and Safety Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, EmptyStringValue) {
    ErrorHandler<std::string, FileError> result(std::string(""));

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(ErrorHandlerTestFixture, ZeroValue) {
    ErrorHandler<int, FileError> result(0);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 0);
}

TEST_F(ErrorHandlerTestFixture, NegativeValue) {
    ErrorHandler<int, FileError> result(-42);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), -42);
}

TEST_F(ErrorHandlerTestFixture, LargeValue) {
    ErrorHandler<std::int64_t, FileError> result(9223372036854775807LL);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 9223372036854775807LL);
}

TEST_F(ErrorHandlerTestFixture, BooleanValueTrue) {
    ErrorHandler<bool, FileError> result(true);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
}

TEST_F(ErrorHandlerTestFixture, BooleanValueFalse) {
    ErrorHandler<bool, FileError> result(false);

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result.value());
}

// ============================================================================
// Real-World Simulation Tests
// ============================================================================

// Simulate file reading operation
ErrorHandler<std::string, FileError> read_file(const std::string& path) {
    if (path.empty()) {
        return ErrorHandler<std::string, FileError>(FileError::InvalidFormat,
                                                    "Path cannot be empty");
    }
    if (path == "/root/secret.txt") {
        return ErrorHandler<std::string, FileError>(FileError::PermissionDenied,
                                                    "Cannot access root files");
    }
    if (path == "/nonexistent/file.txt") {
        return ErrorHandler<std::string, FileError>(FileError::NotFound,
                                                    "File does not exist");
    }
    return ErrorHandler<std::string, FileError>(std::string("file contents"));
}

// Simulate parsing operation
ErrorHandler<int, FileError> parse_number(const std::string& content) {
    if (content.empty()) {
        return ErrorHandler<int, FileError>(FileError::InvalidFormat,
                                            "Cannot parse empty string");
    }
    if (content == "file contents") {
        return ErrorHandler<int, FileError>(42);
    }
    return ErrorHandler<int, FileError>(FileError::InvalidFormat,
                                        "Invalid number format");
}

TEST_F(ErrorHandlerTestFixture, FileReadingSimulation) {
    auto result{read_file("/home/user/data.txt")};
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "file contents");
}

TEST_F(ErrorHandlerTestFixture, FileReadingPermissionError) {
    auto result{read_file("/root/secret.txt")};
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileError::PermissionDenied);
}

TEST_F(ErrorHandlerTestFixture, FileReadingNotFoundError) {
    auto result{read_file("/nonexistent/file.txt")};
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileError::NotFound);
}

TEST_F(ErrorHandlerTestFixture, FileReadAndParseChain) {
    auto result_val{read_file("/home/user/data.txt")
                        .and_then([](const std::string& content) {
                            return parse_number(content);
                        })};

    EXPECT_TRUE(result_val.has_value());
    EXPECT_EQ(result_val.value(), 42);
}

TEST_F(ErrorHandlerTestFixture, FileReadAndParseChainWithError) {
    auto result_val{read_file("/nonexistent/file.txt")
                        .and_then([](const std::string& content) {
                            return parse_number(content);
                        })};

    EXPECT_FALSE(result_val.has_value());
    EXPECT_EQ(result_val.error(), FileError::NotFound);
}

TEST_F(ErrorHandlerTestFixture, ComplexOperationPipeline) {
    auto result{
        read_file("/home/user/config.txt")
            .transform([](const std::string& content) {
                return content.length();  // Get length
            })
            .and_then([](size_t len) -> ErrorHandler<size_t, FileError> {
                if (len == 0) {
                    return ErrorHandler<size_t, FileError>(
                        FileError::InvalidFormat, "Empty file");
                }
                return ErrorHandler<size_t, FileError>(len);
            })
            .transform([](size_t len) {
                return len * 2;  // Double it
            })};

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 26);  // "file contents" = 13 chars * 2
}

// ============================================================================
// Const Correctness Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, ConstErrorHandlerAccess) {
    const ErrorHandler<int, FileError> result(100);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 100);
}

TEST_F(ErrorHandlerTestFixture, ConstErrorHandlerWithError) {
    const ErrorHandler<int, FileError> result(FileError::IOError);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FileError::IOError);
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, MoveConstructionSuccess) {
    ErrorHandler<std::string, FileError> result1(std::string("original"));
    ErrorHandler<std::string, FileError> result2(std::move(result1.value()));

    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), "original");
}

TEST_F(ErrorHandlerTestFixture, MoveConstructionVector) {
    std::vector<int> large_data(1000, 42);
    ErrorHandler<std::vector<int>, FileError> result(std::move(large_data));

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1000);
    EXPECT_EQ(result.value()[0], 42);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, LongChainingOperations) {
    ErrorHandler<int, FileError> result(1);

    auto result_val{result.transform([](int v) { return v + 1; })
                        .transform([](int v) { return v * 2; })
                        .transform([](int v) { return v + 3; })
                        .transform([](int v) { return v * 4; })
                        .transform([](int v) { return v + 5; })
                        .transform([](int v) { return v * 6; })
                        .transform([](int v) { return v + 7; })
                        .transform([](int v) { return v * 8; })
                        .transform([](int v) { return v + 9; })
                        .transform([](int v) { return v * 10; })};

    // ((((((((((1+1)*2+3)*4+5)*6+7)*8+9)*10
    // = ((((((((4+3)*4+5)*6+7)*8+9)*10
    // = (((((((7*4+5)*6+7)*8+9)*10
    // = ((((((33*6+7)*8+9)*10
    // = (((((205*8+9)*10
    // = ((((1649)*10
    // = 16490

    EXPECT_TRUE(result_val.has_value());
    int expected{1};
    expected = (expected + 1) * 2;   // 4
    expected = (expected + 3) * 4;   // 28
    expected = (expected + 5) * 6;   // 198
    expected = (expected + 7) * 8;   // 1640
    expected = (expected + 9) * 10;  // 16490

    EXPECT_EQ(result_val.value(), expected);
}

TEST_F(ErrorHandlerTestFixture, MultipleIndependentInstances) {
    std::vector<ErrorHandler<int, FileError>> results;

    for (int i = 0; i < 100; ++i) {
        if (i % 2 == 0) {
            results.emplace_back(i);
        } else {
            results.emplace_back(FileError::IOError, "Test error");
        }
    }

    int success_count{0};
    int error_count{0};

    for (const auto& result : results) {
        if (result.has_value()) {
            ++success_count;
        } else {
            ++error_count;
        }
    }

    EXPECT_EQ(success_count, 50);
    EXPECT_EQ(error_count, 50);
}

// ============================================================================
// New Monadic Operations Tests: or_else
// ============================================================================

TEST_F(ErrorHandlerTestFixture, OrElseWithError) {
    ErrorHandler<int, FileError> result(FileError::NotFound, "File missing");

    auto recovered{
        result.or_else([](FileError err) -> ErrorHandler<int, FileError> {
            (void)err;  // Unused parameter
            // Recover from error
            return ErrorHandler<int, FileError>(42);
        })};

    EXPECT_TRUE(recovered.has_value());
    EXPECT_EQ(recovered.value(), 42);
}

TEST_F(ErrorHandlerTestFixture, OrElseWithSuccess) {
    ErrorHandler<int, FileError> result(100);

    auto unchanged{
        result.or_else([](FileError err) -> ErrorHandler<int, FileError> {
            (void)err;  // Unused parameter
            // This should not execute
            return ErrorHandler<int, FileError>(999);
        })};

    EXPECT_TRUE(unchanged.has_value());
    EXPECT_EQ(unchanged.value(), 100);
}

TEST_F(ErrorHandlerTestFixture, OrElseChaining) {
    ErrorHandler<int, FileError> result(FileError::NotFound, "Primary failed");

    auto result_val{
        result
            .or_else([](FileError err) -> ErrorHandler<int, FileError> {
                // First recovery attempt fails
                if (err == FileError::NotFound) {
                    return ErrorHandler<int, FileError>(
                        FileError::PermissionDenied, "Recovery1 failed");
                }
                return ErrorHandler<int, FileError>(1);
            })
            .or_else([](FileError err) -> ErrorHandler<int, FileError> {
                (void)err;  // Unused parameter
                // Second recovery succeeds
                return ErrorHandler<int, FileError>(42);
            })};

    EXPECT_TRUE(result_val.has_value());
    EXPECT_EQ(result_val.value(), 42);
}

TEST_F(ErrorHandlerTestFixture, OrElseErrorTypeTransformation) {
    ErrorHandler<std::string, FileError> result(FileError::IOError,
                                                "Disk failure");

    auto recovered{result.or_else(
        [](FileError err) -> ErrorHandler<std::string, FileError> {
            (void)err;  // Unused parameter
            return ErrorHandler<std::string, FileError>(
                std::string("fallback_value"));
        })};

    EXPECT_TRUE(recovered.has_value());
    EXPECT_EQ(recovered.value(), "fallback_value");
}

// ============================================================================
// transform_error Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, TransformErrorWithError) {
    ErrorHandler<int, FileError> result(FileError::NotFound, "File not found");

    auto transformed{result.transform_error([](FileError err) -> NetworkError {
        (void)err;  // Unused parameter
        // Convert FileError to NetworkError
        return NetworkError::HostUnreachable;
    })};

    EXPECT_FALSE(transformed.has_value());
    EXPECT_EQ(transformed.error(), NetworkError::HostUnreachable);
}

TEST_F(ErrorHandlerTestFixture, TransformErrorWithSuccess) {
    ErrorHandler<int, FileError> result(42);

    auto transformed{result.transform_error([](FileError err) -> NetworkError {
        (void)err;  // Unused parameter
        // Should not execute
        return NetworkError::ConnectionTimeout;
    })};

    EXPECT_TRUE(transformed.has_value());
    EXPECT_EQ(transformed.value(), 42);
}

TEST_F(ErrorHandlerTestFixture, TransformErrorChaining) {
    ErrorHandler<std::string, FileError> result(FileError::IOError,
                                                "Disk error");

    // First transform error type
    auto net_error{result.transform_error([](FileError err) -> NetworkError {
        (void)err;  // Unused parameter
        return NetworkError::InvalidResponse;
    })};

    // Then transform to database error
    auto db_error{
        net_error.transform_error([](NetworkError err) -> DatabaseError {
            (void)err;  // Unused parameter
            return DatabaseError::ConnectionFailed;
        })};

    EXPECT_FALSE(db_error.has_value());
    EXPECT_EQ(db_error.error(), DatabaseError::ConnectionFailed);
}

// ============================================================================
// value_or with rvalue Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, ValueOrRvalueWithSuccess) {
    // Test that rvalue overload is called and moves efficiently
    auto get_result{[]() {
        return ErrorHandler<std::string, FileError>(std::string("success"));
    }};

    std::string val{get_result().value_or(std::string("default"))};
    EXPECT_EQ(val, "success");
}

TEST_F(ErrorHandlerTestFixture, ValueOrRvalueWithError) {
    auto get_result{[]() {
        return ErrorHandler<std::string, FileError>(FileError::NotFound);
    }};

    std::string val{get_result().value_or(std::string("default"))};
    EXPECT_EQ(val, "default");
}

TEST_F(ErrorHandlerTestFixture, ValueOrLargeType) {
    struct LargeData {
        std::vector<int> data;
        LargeData() : data(1000, 42) {}
        explicit LargeData(int val) : data(1000, val) {}
    };

    auto get_result{
        []() { return ErrorHandler<LargeData, FileError>(LargeData(100)); }};

    // This should move, not copy
    LargeData result{get_result().value_or(LargeData(999))};
    EXPECT_EQ(result.data[0], 100);
    EXPECT_EQ(result.data.size(), 1000);
}

// ============================================================================
// value() rvalue overload Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, ValueRvalueMoveOut) {
    auto get_result{[]() {
        return ErrorHandler<std::string, FileError>(std::string("movable"));
    }};

    // Should move the string out of the temporary ErrorHandler
    std::string val{get_result().value()};
    EXPECT_EQ(val, "movable");
}

TEST_F(ErrorHandlerTestFixture, ValueConstRvalue) {
    const auto get_result{[]() { return ErrorHandler<int, FileError>(42); }};

    int val{get_result().value()};
    EXPECT_EQ(val, 42);
}

// ============================================================================
// Complete Pipeline Tests (combining all monadic operations)
// ============================================================================

TEST_F(ErrorHandlerTestFixture, CompleteMonadicPipeline) {
    auto result{
        read_file("/home/user/data.txt")
            .transform(
                [](const std::string& content) { return content.length(); })
            .and_then([](size_t len) -> ErrorHandler<int, FileError> {
                if (len > 0) {
                    return ErrorHandler<int, FileError>(static_cast<int>(len));
                }
                return ErrorHandler<int, FileError>(FileError::InvalidFormat,
                                                    "Empty file");
            })
            .or_else([](FileError err) -> ErrorHandler<int, FileError> {
                (void)err;  // Unused parameter
                // Recover with default value
                return ErrorHandler<int, FileError>(10);
            })
            .transform([](int val) { return val * 2; })};

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 26);  // 13 chars * 2
}

TEST_F(ErrorHandlerTestFixture, PipelineWithErrorRecovery) {
    auto process{[](const std::string& path) -> ErrorHandler<int, FileError> {
        return read_file(path)
            .and_then([](const std::string& content) {
                return parse_number(content);
            })
            .or_else([](FileError err) -> ErrorHandler<int, FileError> {
                // First recovery: try default value
                if (err == FileError::NotFound) {
                    return ErrorHandler<int, FileError>(
                        FileError::PermissionDenied, "Still failing");
                }
                return ErrorHandler<int, FileError>(0);
            })
            .or_else([](FileError err) -> ErrorHandler<int, FileError> {
                (void)err;  // Unused parameter
                // Final recovery: always succeed with sentinel
                return ErrorHandler<int, FileError>(-1);
            });
    }};

    auto result{process("/nonexistent/file.txt")};
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), -1);
}

TEST_F(ErrorHandlerTestFixture, ErrorTransformationPipeline) {
    ErrorHandler<std::string, FileError> file_result(FileError::NotFound,
                                                     "Config missing");

    auto result_val{
        file_result
            .transform_error([](FileError err) -> NetworkError {
                (void)err;  // Unused parameter
                return NetworkError::HostUnreachable;
            })
            .or_else([](NetworkError err)
                         -> ErrorHandler<std::string, NetworkError> {
                (void)err;  // Unused parameter
                return ErrorHandler<std::string, NetworkError>(
                    std::string("recovered"));
            })
            .transform([](const std::string& s) { return s.length(); })};

    EXPECT_TRUE(result_val.has_value());
    EXPECT_EQ(result_val.value(), 9);  // "recovered".length()
}

// ============================================================================
// const&& overload Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, ConstRvalueAndThen) {
    const auto get_result{[]() { return ErrorHandler<int, FileError>(10); }};

    auto result{
        get_result().and_then([](int val) -> ErrorHandler<int, FileError> {
            return ErrorHandler<int, FileError>(val * 2);
        })};

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 20);
}

TEST_F(ErrorHandlerTestFixture, ConstRvalueTransform) {
    const auto get_result{[]() { return ErrorHandler<int, FileError>(5); }};

    auto result{get_result().transform([](int val) { return val + 10; })};

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 15);
}

// ============================================================================
// Move Semantics Verification
// ============================================================================

TEST_F(ErrorHandlerTestFixture, MoveOnlyTypeSuccess) {
    struct MoveOnly {
        std::unique_ptr<int> data;
        explicit MoveOnly(int val) : data(std::make_unique<int>(val)) {}
        MoveOnly(const MoveOnly&) = delete;
        MoveOnly(MoveOnly&&) = default;
        MoveOnly& operator=(const MoveOnly&) = delete;
        MoveOnly& operator=(MoveOnly&&) = default;
        ~MoveOnly() = default;
    };

    ErrorHandler<MoveOnly, FileError> result(MoveOnly(42));

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result.value().data, 42);

    // Move the value out
    MoveOnly moved = std::move(result).value();
    EXPECT_EQ(*moved.data, 42);
}

TEST_F(ErrorHandlerTestFixture, MoveOnlyTypeTransform) {
    struct MoveOnly {
        std::unique_ptr<int> data;
        explicit MoveOnly(int val) : data(std::make_unique<int>(val)) {}
        MoveOnly(const MoveOnly&) = delete;
        MoveOnly(MoveOnly&&) = default;
        MoveOnly& operator=(const MoveOnly&) = delete;
        MoveOnly& operator=(MoveOnly&&) = default;
        ~MoveOnly() = default;
    };

    ErrorHandler<MoveOnly, FileError> result(MoveOnly(10));

    auto transformed{std::move(result).transform(
        [](MoveOnly&& mo) { return *std::move(mo).data * 2; })};

    EXPECT_TRUE(transformed.has_value());
    EXPECT_EQ(transformed.value(), 20);
}

// ============================================================================
// Exception Safety Tests
// ============================================================================

TEST_F(ErrorHandlerTestFixture, NoexceptSpecifications) {
    ErrorHandler<int, FileError> result(42);

    // These operations should be noexcept
    static_assert(noexcept(result.has_value()));
    static_assert(noexcept(static_cast<bool>(result)));
}

// ============================================================================
// Real-World Safety Critical Scenarios
// ============================================================================

TEST_F(ErrorHandlerTestFixture, SensorReadingWithFallback) {
    auto read_sensor{[](int sensor_id) -> ErrorHandler<double, FileError> {
        if (sensor_id == 1) {
            return ErrorHandler<double, FileError>(25.5);  // Temperature
        }
        return ErrorHandler<double, FileError>(FileError::IOError,
                                               "Sensor failed");
    }};

    auto result{
        read_sensor(1)
            .transform([](double temp) { return temp * 1.8 + 32.0; })  // C to F
            .or_else([](FileError err) -> ErrorHandler<double, FileError> {
                (void)err;  // Unused parameter
                return ErrorHandler<double, FileError>(
                    -273.15);  // Sentinel value
            })};

    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 77.9, 0.1);
}

TEST_F(ErrorHandlerTestFixture, CriticalSystemInitialization) {
    auto init_subsystem{
        [](const std::string& name) -> ErrorHandler<bool, FileError> {
            if (name == "critical") {
                return ErrorHandler<bool, FileError>(true);
            }
            return ErrorHandler<bool, FileError>(FileError::PermissionDenied,
                                                 "Init failed");
        }};

    auto result{init_subsystem("critical")
                    .and_then([](bool success) -> ErrorHandler<int, FileError> {
                        (void)success;  // Unused parameter
                        return ErrorHandler<int, FileError>(
                            100);  // Success code
                    })
                    .or_else([](FileError err) -> ErrorHandler<int, FileError> {
                        (void)err;  // Unused parameter
                        // Critical failure - log and use safe default
                        return ErrorHandler<int, FileError>(-1);
                    })
                    .transform([](int code) {
                        return code > 0;  // Convert to boolean status
                    })};

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
}