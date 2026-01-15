#!/bin/bash
#
# Generate code coverage report for Widgeteer
#
# Prerequisites:
#   - Build with Debug mode (cmake -B build or cmake -B build -DCMAKE_BUILD_TYPE=Debug)
#   - lcov and genhtml installed (apt install lcov)
#
# Usage:
#   ./scripts/coverage.sh
#
# Output:
#   - coverage/coverage.info - lcov data file
#   - coverage/html/index.html - HTML report

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
COVERAGE_DIR="$PROJECT_DIR/coverage"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Widgeteer Code Coverage ===${NC}"

# Check for lcov
if ! command -v lcov &> /dev/null; then
    echo -e "${RED}Error: lcov not found. Install with: sudo apt install lcov${NC}"
    exit 1
fi

# Check build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found. Run 'cmake -B build && cmake --build build' first.${NC}"
    exit 1
fi

# Create coverage directory
mkdir -p "$COVERAGE_DIR/html"

# Step 1: Reset coverage counters
echo -e "${YELLOW}Resetting coverage counters...${NC}"
lcov --zerocounters --directory "$BUILD_DIR"

# Step 2: Capture baseline (before tests)
echo -e "${YELLOW}Capturing baseline coverage...${NC}"
lcov --capture --initial --directory "$BUILD_DIR" --output-file "$COVERAGE_DIR/coverage_base.info" \
    --rc lcov_branch_coverage=1 2>/dev/null || \
lcov --capture --initial --directory "$BUILD_DIR" --output-file "$COVERAGE_DIR/coverage_base.info"

# Step 3: Run tests
echo -e "${YELLOW}Running unit tests...${NC}"
cd "$BUILD_DIR"

# Disable leak detection for coverage run (ASan can interfere)
export ASAN_OPTIONS=detect_leaks=0

ctest --output-on-failure || true

cd "$PROJECT_DIR"

# Step 3b: Run integration tests (exercises sample app)
echo -e "${YELLOW}Running integration tests...${NC}"
SAMPLE_APP="$BUILD_DIR/sample/widgeteer_sample"
if [ -x "$SAMPLE_APP" ] && [ -f "$PROJECT_DIR/tests/sample_tests.json" ]; then
    python3 "$PROJECT_DIR/tests/test_executor.py" \
        --app "$SAMPLE_APP" \
        --wait 10 \
        "$PROJECT_DIR/tests/sample_tests.json" || true
else
    echo -e "${YELLOW}Skipping integration tests (sample app or test file not found)${NC}"
fi

# Step 4: Capture coverage after tests
echo -e "${YELLOW}Capturing test coverage...${NC}"
lcov --capture --directory "$BUILD_DIR" --output-file "$COVERAGE_DIR/coverage_test.info" \
    --rc lcov_branch_coverage=1 2>/dev/null || \
lcov --capture --directory "$BUILD_DIR" --output-file "$COVERAGE_DIR/coverage_test.info"

# Step 5: Combine baseline and test coverage
echo -e "${YELLOW}Combining coverage data...${NC}"
lcov --add-tracefile "$COVERAGE_DIR/coverage_base.info" \
     --add-tracefile "$COVERAGE_DIR/coverage_test.info" \
     --output-file "$COVERAGE_DIR/coverage_combined.info"

# Step 6: Filter out system headers, Qt, and test files
echo -e "${YELLOW}Filtering coverage data...${NC}"
lcov --remove "$COVERAGE_DIR/coverage_combined.info" \
    '/usr/*' \
    '*/Qt/*' \
    '*/qt/*' \
    '*/qt6/*' \
    '*/.local/qt/*' \
    '*/QtCore/*' \
    '*/QtGui/*' \
    '*/QtWidgets/*' \
    '*/QtTest/*' \
    '*/QtNetwork/*' \
    '*/QtWebSockets/*' \
    '*/tests/*' \
    '*/build/*' \
    '*moc_*' \
    '*_autogen/*' \
    --output-file "$COVERAGE_DIR/coverage.info"

# Step 7: Generate HTML report
echo -e "${YELLOW}Generating HTML report...${NC}"
genhtml "$COVERAGE_DIR/coverage.info" \
    --output-directory "$COVERAGE_DIR/html" \
    --title "Widgeteer Code Coverage" \
    --legend \
    --show-details

# Step 8: Print summary
echo ""
echo -e "${GREEN}=== Coverage Summary ===${NC}"
lcov --summary "$COVERAGE_DIR/coverage.info" 2>&1 | grep -E "lines|functions|branches" || true

echo ""
echo -e "${GREEN}HTML report generated: $COVERAGE_DIR/html/index.html${NC}"
echo -e "Open with: ${YELLOW}xdg-open $COVERAGE_DIR/html/index.html${NC}"

# Cleanup intermediate files
rm -f "$COVERAGE_DIR/coverage_base.info" "$COVERAGE_DIR/coverage_test.info" "$COVERAGE_DIR/coverage_combined.info"
