#include <memory>

#include "common/protobuf/utility.h"

#include "extensions/filters/http/common/compressor/compressor.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/runtime/mocks.h"
#include "test/mocks/stats/mocks.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

using testing::Return;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Common {
namespace Compressors {

class CompressorFilterTestFixture : public testing::Test {

};

TEST_F(CompressorFilterTestFixture, HelloWorld) {}

} // namespace Compressors
} // namespace Common
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
